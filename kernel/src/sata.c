// sata.c
#include "sata.h"
#include "serial.h"
#include "io.h"
#include "memory.h"
#include <string.h>

static struct hba_memory* ghba = NULL;
static struct sata_device sata_devices[AHCI_MAX_PORTS];
static int num_devices = 0;

// PCI配置空间访问
static uint32_t pci_read_dword(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset) {
    uint32_t address = (uint32_t)((bus << 16) | (slot << 11) | (func << 8) | (offset & 0xFC) | 0x80000000);
    outl(0xCF8, address);
    return inl(0xCFC);
}

static void pci_write_dword(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset, uint32_t value) {
    uint32_t address = (uint32_t)((bus << 16) | (slot << 11) | (func << 8) | (offset & 0xFC) | 0x80000000);
    outl(0xCF8, address);
    outl(0xCFC, value);
}

// 内存分配
static void* alloc_aligned(size_t size, size_t alignment, uint64_t* phys_addr) {
    static uint8_t memory_pool[4 * 1024 * 1024] __attribute__((aligned(4096)));
    static size_t pool_offset = 0;
    
    if (alignment < 4096) alignment = 4096;
    
    size = (size + alignment - 1) & ~(alignment - 1);
    
    if (pool_offset + size > sizeof(memory_pool)) {
        return NULL;
    }
    
    void* addr = &memory_pool[pool_offset];
    pool_offset += size;
    
    if (phys_addr) {
        *phys_addr = (uint64_t)(uintptr_t)addr;
    }
    
    memset(addr, 0, size);
    return addr;
}

// 内存屏障
static inline void mmio_barrier(void) {
    asm volatile("" ::: "memory");
}

// 读取MMIO寄存器
static uint32_t mmio_read32(volatile uint32_t* addr) {
    return *addr;
}

static void mmio_write32(volatile uint32_t* addr, uint32_t value) {
    *addr = value;
    mmio_barrier();
}

// 停止端口命令引擎
static void port_stop_cmd(struct hba_port* port) {
    uint32_t cmd = mmio_read32(&port->cmd);
    cmd &= ~((1 << HBA_PxCMD_ST) | (1 << HBA_PxCMD_FRE));
    mmio_write32(&port->cmd, cmd);
    
    // 等待FR和CR清除
    int timeout = 1000000;
    while (timeout--) {
        cmd = mmio_read32(&port->cmd);
        if (!(cmd & (1 << HBA_PxCMD_FR)) && !(cmd & (1 << HBA_PxCMD_CR))) {
            break;
        }
        asm volatile("pause");
    }
}

// 启动端口命令引擎
static void port_start_cmd(struct hba_port* port) {
    // 等待FR清除
    while (mmio_read32(&port->cmd) & (1 << HBA_PxCMD_FR)) {
        asm volatile("pause");
    }
    
    uint32_t cmd = mmio_read32(&port->cmd);
    cmd |= (1 << HBA_PxCMD_FRE) | (1 << HBA_PxCMD_ST);
    mmio_write32(&port->cmd, cmd);
}

// 查找实现的端口
static uint32_t find_implemented_ports(void) {
    if (!ghba) return 0;
    return mmio_read32(&ghba->pi);
}

// 检查端口类型
// 在 src/sata.c 中，修改 check_port_type 函数
static int check_port_type(volatile struct hba_port* port) {  // 添加 volatile
    uint32_t ssts = mmio_read32(&port->ssts);
    uint8_t ipm = (ssts >> 8) & 0x0F;
    uint8_t det = ssts & 0x0F;
    
    if (det != HBA_PORT_DET_PRESENT) return 0;
    if (ipm != HBA_PORT_IPM_ACTIVE) return 0;
    
    uint32_t sig = mmio_read32(&port->sig);
    
    if ((sig & 0xFFFFFF00) == 0xEB140100) {
        return 1; // SATA
    } else if ((sig & 0xFFFFFF00) == 0x96690100) {
        return 2; // SATAPI
    } else if ((sig & 0xFFFFFF00) == 0xC33C0100) {
        return 3; // Enclosure management bridge
    } else if ((sig & 0xFFFFFF00) == 0x00000100) {
        return 4; // Port multiplier
    }
    
    return 0;
}
// 初始化端口
static int init_port(uint8_t port_num) {
    struct hba_port* port = &ghba->ports[port_num];
    
    // 停止命令引擎
    port_stop_cmd(port);
    
    // 分配命令列表
    uint64_t cl_phys;
    struct hba_cmd_header* cl = alloc_aligned(32 * sizeof(struct hba_cmd_header), 
                                             AHCI_PAGE_SIZE, &cl_phys);
    if (!cl) return 0;
    
    // 分配FIS接收区
    uint64_t fis_phys;
    void* fis = alloc_aligned(256, AHCI_PAGE_SIZE, &fis_phys);
    if (!fis) return 0;
    
    // 配置端口
    mmio_write32(&port->clb, (uint32_t)cl_phys);
    mmio_write32(&port->clbu, (uint32_t)(cl_phys >> 32));
    mmio_write32(&port->fb, (uint32_t)fis_phys);
    mmio_write32(&port->fbu, (uint32_t)(fis_phys >> 32));
    
    // 启用FIS接收
    uint32_t cmd = mmio_read32(&port->cmd);
    cmd |= (1 << HBA_PxCMD_FRE);
    mmio_write32(&port->cmd, cmd);
    
    // 清除中断状态
    mmio_write32(&port->is, 0xFFFFFFFF);
    
    // 启用中断
    mmio_write32(&port->ie, 0xFFFFFFFF);
    
    // 分配命令表
    for (int i = 0; i < 32; i++) {
        uint64_t ct_phys;
        struct hba_cmd_table* ct = alloc_aligned(sizeof(struct hba_cmd_table),
                                                AHCI_PAGE_SIZE, &ct_phys);
        if (!ct) return 0;
        
        cl[i].ctba = (uint32_t)ct_phys;
        cl[i].ctbau = (uint32_t)(ct_phys >> 32);
        cl[i].prdtl = AHCI_PRDT_ENTRIES;
        
        // 存储指针
        sata_devices[num_devices].command_tables[i] = ct;
        sata_devices[num_devices].command_tables_phys[i] = ct_phys;
    }
    
    // 启动命令引擎
    port_start_cmd(port);
    
    // 初始化设备结构
    sata_devices[num_devices].port = port;
    sata_devices[num_devices].port_num = port_num;
    sata_devices[num_devices].command_list = cl;
    sata_devices[num_devices].command_list_phys = cl_phys;
    sata_devices[num_devices].fis = fis;
    sata_devices[num_devices].fis_phys = fis_phys;
    sata_devices[num_devices].initialized = 1;
    sata_devices[num_devices].type = check_port_type(port);
    
    num_devices++;
    return 1;
}

// 等待命令完成
static int wait_cmd_complete(struct hba_port* port, uint32_t slot) {
    int timeout = 1000000;
    
    while (timeout--) {
        uint32_t ci = mmio_read32(&port->ci);
        uint32_t is = mmio_read32(&port->is);
        
        if (!(ci & (1 << slot))) {
            // 清除中断状态
            mmio_write32(&port->is, is);
            return 1;
        }
        
        if (is & 0x80000000) { // TFES错误
            mmio_write32(&port->is, is);
            return 0;
        }
        
        asm volatile("pause");
    }
    
    return 0;
}

// 执行ATA命令
static int exec_ata_command(struct sata_device* dev, uint8_t command, 
                           uint64_t lba, uint16_t count, void* buffer, int write) {
    struct hba_port* port = dev->port;
    
    // 查找空闲命令槽
    uint32_t slot = 0;
    uint32_t ci = mmio_read32(&port->ci);
    uint32_t sact = mmio_read32(&port->sact);
    
    for (slot = 0; slot < 32; slot++) {
        if (!(ci & (1 << slot)) && !(sact & (1 << slot))) {
            break;
        }
    }
    
    if (slot >= 32) {
        return 0; // 无空闲槽
    }
    
    struct hba_cmd_header* cmd_header = &dev->command_list[slot];
    struct hba_cmd_table* cmd_table = dev->command_tables[slot];
    
    // 构建FIS
    struct fis_reg_h2d* fis = (struct fis_reg_h2d*)&cmd_table->cfis[0];
    memset(fis, 0, sizeof(struct fis_reg_h2d));
    
    fis->fis_type = FIS_TYPE_REG_H2D;
    fis->c = 1;
    fis->command = command;
    
    if (lba) {
        fis->lba0 = (lba >> 0) & 0xFF;
        fis->lba1 = (lba >> 8) & 0xFF;
        fis->lba2 = (lba >> 16) & 0xFF;
        fis->device = 0x40; // LBA模式
        fis->lba3 = (lba >> 24) & 0xFF;
        fis->lba4 = (lba >> 32) & 0xFF;
        fis->lba5 = (lba >> 40) & 0xFF;
    }
    
    fis->count_low = count & 0xFF;
    fis->count_high = (count >> 8) & 0xFF;
    
    // 配置命令头
    cmd_header->cfl = sizeof(struct fis_reg_h2d) / sizeof(uint32_t);
    cmd_header->write = write ? 1 : 0;
    cmd_header->prdtl = 1;
    
    // 配置PRDT
    if (buffer) {
        uint64_t buffer_phys = (uint64_t)(uintptr_t)buffer;
        struct hba_prdt_entry* prdt = &cmd_table->prdt_entry[0];
        
        prdt->dba = (uint32_t)buffer_phys;
        prdt->dbau = (uint32_t)(buffer_phys >> 32);
        prdt->dbc = (count * 512) - 1;
        prdt->i = 1;
    }
    
    // 发布命令
    mmio_write32(&port->ci, 1 << slot);
    
    // 等待完成
    return wait_cmd_complete(port, slot);
}

// 识别设备
static int identify_device(struct sata_device* dev) {
    uint16_t identify_data[256];
    
    if (!exec_ata_command(dev, ATA_CMD_IDENTIFY, 0, 1, identify_data, 0)) {
        return 0;
    }
    
    // 解析识别数据
    dev->sector_size = 512;
    
    // 获取扇区数（LBA48如果支持）
    if (identify_data[83] & (1 << 10)) { // LBA48支持
        dev->sector_count = ((uint64_t)identify_data[103] << 48) |
                           ((uint64_t)identify_data[102] << 32) |
                           ((uint64_t)identify_data[101] << 16) |
                           identify_data[100];
    } else { // LBA28
        dev->sector_count = (identify_data[61] << 16) | identify_data[60];
    }
    
    return 1;
}

// 查找AHCI控制器
int sata_probe(void) {
    // 扫描PCI总线查找AHCI控制器
    for (uint8_t bus = 0; bus < 256; bus++) {
        for (uint8_t slot = 0; slot < 32; slot++) {
            for (uint8_t func = 0; func < 8; func++) {
                uint32_t vendor_device = pci_read_dword(bus, slot, func, 0x00);
                uint32_t class_revision = pci_read_dword(bus, slot, func, 0x08);
                
                uint16_t vendor_id = vendor_device & 0xFFFF;
                uint16_t device_id = vendor_device >> 16;
                uint8_t class_code = (class_revision >> 24) & 0xFF;
                uint8_t subclass = (class_revision >> 16) & 0xFF;
                uint8_t prog_if = (class_revision >> 8) & 0xFF;
                
                if (vendor_id == 0xFFFF) continue;
                
                if (class_code == 0x01 && subclass == 0x06 && prog_if == 0x01) {
                    // 启用总线主控和内存空间
                    uint32_t command = pci_read_dword(bus, slot, func, 0x04);
                    command |= (1 << 2) | (1 << 1);
                    pci_write_dword(bus, slot, func, 0x04, command);
                    
                    // 读取BAR5（AHCI寄存器空间）
                    uint32_t bar5 = pci_read_dword(bus, slot, func, 0x24);
                    ghba = (struct hba_memory*)(uintptr_t)(bar5 & ~0xF);
                    
                    return 1;
                }
            }
        }
    }
    
    return 0;
}

// 初始化SATA/AHCI
int sata_init(void) {
    if (!sata_probe()) {
        return 0;
    }
    
    if (!ghba) {
        return 0;
    }
    
    // 启用AHCI
    uint32_t ghc = mmio_read32(&ghba->ghc);
    ghc |= (1 << 31); // AE位
    mmio_write32(&ghba->ghc, ghc);
    
    // 查找实现的端口
    uint32_t ports = find_implemented_ports();
    
    // 初始化每个端口
    for (int i = 0; i < 32; i++) {
        if (ports & (1 << i)) {
            if (check_port_type(&ghba->ports[i])) {
                if (init_port(i)) {
                    // 识别设备
                    if (identify_device(&sata_devices[num_devices - 1])) {
                        serial_puts("SATA: Found device on port ");
                        serial_putc('0' + i);
                        serial_puts(" with ");
                        // 打印扇区数
                        char buf[32];
                        char* p = buf;
                        uint64_t sectors = sata_devices[num_devices - 1].sector_count;
                        do {
                            *p++ = '0' + (sectors % 10);
                            sectors /= 10;
                        } while (sectors);
                        *p-- = '\0';
                        while (p > buf) {
                            char tmp = *p;
                            *p = *buf;
                            *buf = tmp;
                            p--;
                            p++;
                        }
                        serial_puts(buf);
                        serial_puts(" sectors\n");
                    }
                }
            }
        }
    }
    
    return (num_devices > 0);
}

// 读取扇区
int sata_read_sectors(struct sata_device* dev, uint64_t lba, uint32_t count, void* buffer) {
    if (!dev || !dev->initialized) {
        return 0;
    }
    
    // 限制每次传输的扇区数
    const uint32_t max_sectors_per_transfer = 256;
    
    while (count > 0) {
        uint32_t sectors_this_time = count;
        if (sectors_this_time > max_sectors_per_transfer) {
            sectors_this_time = max_sectors_per_transfer;
        }
        
        if (!exec_ata_command(dev, ATA_CMD_READ_DMA, lba, sectors_this_time, 
                             buffer, 0)) {
            return 0;
        }
        
        lba += sectors_this_time;
        buffer = (uint8_t*)buffer + (sectors_this_time * 512);
        count -= sectors_this_time;
    }
    
    return 1;
}

// 写入扇区
int sata_write_sectors(struct sata_device* dev, uint64_t lba, uint32_t count, const void* buffer) {
    if (!dev || !dev->initialized) {
        return 0;
    }
    
    const uint32_t max_sectors_per_transfer = 256;
    
    while (count > 0) {
        uint32_t sectors_this_time = count;
        if (sectors_this_time > max_sectors_per_transfer) {
            sectors_this_time = max_sectors_per_transfer;
        }
        
        if (!exec_ata_command(dev, ATA_CMD_WRITE_DMA, lba, sectors_this_time, 
                             (void*)buffer, 1)) {
            return 0;
        }
        
        lba += sectors_this_time;
        buffer = (const uint8_t*)buffer + (sectors_this_time * 512);
        count -= sectors_this_time;
    }
    
    return 1;
}

// 获取设备
struct sata_device* sata_get_device(void) {
    if (num_devices > 0) {
        return &sata_devices[0];
    }
    return NULL;
}

// 测试SATA
void sata_test(void) {
    struct sata_device* dev = sata_get_device();
    if (!dev) {
        serial_puts("SATA: No device found\n");
        return;
    }
    
    uint8_t buffer[4096];
    uint8_t pattern[4096];
    
    // 创建测试模式
    for (int i = 0; i < sizeof(pattern); i++) {
        pattern[i] = 0xFF - (i % 256);
    }
    
    // 写入测试（使用LBA 1000避免覆盖重要数据）
    serial_puts("SATA: Writing test pattern to sector 1000...\n");
    if (sata_write_sectors(dev, 1000, 8, pattern)) {
        serial_puts("SATA: Write successful\n");
    } else {
        serial_puts("SATA: Write failed\n");
        return;
    }
    
    // 读取测试
    serial_puts("SATA: Reading back from sector 1000...\n");
    if (sata_read_sectors(dev, 1000, 8, buffer)) {
        serial_puts("SATA: Read successful\n");
    } else {
        serial_puts("SATA: Read failed\n");
        return;
    }
    
    // 验证数据
    int errors = 0;
    for (int i = 0; i < sizeof(buffer); i++) {
        if (buffer[i] != pattern[i]) {
            errors++;
        }
    }
    
    if (errors == 0) {
        serial_puts("SATA: All data verified successfully!\n");
    } else {
        serial_puts("SATA: Found ");
        char buf[20];
        char* p = buf;
        uint32_t err = errors;
        do {
            *p++ = '0' + (err % 10);
            err /= 10;
        } while (err);
        *p-- = '\0';
        while (p > buf) {
            char tmp = *p;
            *p = *buf;
            *buf = tmp;
            p--;
            p++;
        }
        serial_puts(buf);
        serial_puts(" errors\n");
    }
}