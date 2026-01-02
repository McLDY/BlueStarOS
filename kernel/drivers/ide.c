/*
                   _ooOoo_
                  o8888888o
                  88" . "88
                  (| -_- |)
                  O\  =  /O
               ____/`---'\____
             .'  \\|     |//  `.
            /  \\|||  :  |||//  \
           /  _||||| -:- |||||-  \
           |   | \\\  -  /// |   |
           | \_|  ''\---/''  |   |
           \  .-\__  `-`  ___/-. /
         ___`. .'  /--.--\  `. . __
      ."" '<  `.___\_<|>_/___.'  >'"".
     | | :  `- \`.;`\ _ /`;.`/ - ` : | |
     \  \ `-.   \_ __\ /__ _/   .-` /  /
======`-.____`-.___\_____/___.-`____.-'======
                   `=---='
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
            佛祖保佑       永无BUG
*/

#include "drivers/ide.h"
#include "serial.h"
#include "io.h"

// 端口操作函数
/*static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    asm volatile("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

static inline void outb(uint16_t port, uint8_t value) {
    asm volatile("outb %0, %1" : : "a"(value), "Nd"(port));
}

static inline uint16_t inw(uint16_t port) {
    uint16_t ret;
    asm volatile("inw %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

static inline void outw(uint16_t port, uint16_t value) {
    asm volatile("outw %0, %1" : : "a"(value), "Nd"(port));
}

static inline void io_wait(void) {
    asm volatile("outb %%al, $0x80" : : "a"(0));
}*/

// 修改 ide_wait_drq，使其能告知调用者是否发生了错误
int ide_wait_ready(void) {
    uint32_t timeout = 100000; // 设置一个超时计数
    while (timeout--) {
        uint8_t status = inb(IDE_STATUS);
        
        // 如果 BSY 位清零，说明驱动器已经处理完之前的命令
        if (!(status & IDE_STATUS_BSY)) {
            // 只有在 BSY=0 时检查 ERR 才是有意义的
            if (status & IDE_STATUS_ERR) {
                uint8_t err = inb(IDE_ERROR);
                serial_puts("IDE Status Error: 0x");
                serial_puthex8(err);
                serial_puts("\n");
                return -1;
            }
            return 0; // 成功
        }
    }
    serial_puts("IDE Wait Ready Timeout!\n");
    return -1;
}

int ide_wait_drq(void) {
    uint32_t timeout = 100000;
    while (timeout--) {
        uint8_t status = inb(IDE_STATUS);
        
        // 出现错误位，立即退出
        if (status & IDE_STATUS_ERR) {
            return -1;
        }
        
        // BSY 必须为 0 且 DRQ 必须为 1 才是真的准备好传输数据了
        if (!(status & IDE_STATUS_BSY) && (status & IDE_STATUS_DRQ)) {
            return 0;
        }
    }
    serial_puts("IDE Wait DRQ Timeout!\n");
    return -1;
}



// 检查磁盘是否存在
uint8_t ide_check_drive(void) {
    // 选择主设备
    outb(IDE_DRIVE_HEAD, defult_device);
    io_wait();
    
    // 写入一些值然后读回
    outb(IDE_SECTOR_CNT, 0xAA);
    outb(IDE_LBA_LOW, 0x55);
    outb(IDE_LBA_MID, 0xAA);
    outb(IDE_LBA_HIGH, 0x55);
    
    // 读回检查
    if (inb(IDE_SECTOR_CNT) != 0xAA) return 0;
    if (inb(IDE_LBA_LOW) != 0x55) return 0;
    if (inb(IDE_LBA_MID) != 0xAA) return 0;
    if (inb(IDE_LBA_HIGH) != 0x55) return 0;
    
    return 1;
}

void ide_init(void) {
    serial_puts("Initializing IDE controller...\n");
    
    // 禁用中断
    outb(IDE_DEV_CTRL, 0x02);
    io_wait();
    
    // 检查磁盘是否存在
    if (ide_check_drive()) {
        serial_puts("IDE drive detected, initializing...\n");
    } else {
        serial_puts("No IDE drive detected\n");
        return;
    }
    
    // 选择主设备，LBA模式
    outb(IDE_DRIVE_HEAD, defult_device | IDE_LBA_MODE);
    io_wait();
    
    // 等待就绪
    ide_wait_ready();
    
    serial_puts("IDE controller initialized successfully\n");
}

int ide_read_sectors(uint32_t lba, uint8_t num_sectors, void* buffer) {
    /*serial_puts("Reading LBA...");
    serial_putdec64(lba);
    serial_puts("\n");*/
    uint16_t* buf = (uint16_t*)buffer;
    
    // 等待磁盘就绪
    ide_wait_ready();
    
    // 设置LBA地址和扇区数
    outb(IDE_DRIVE_HEAD, defult_device | IDE_LBA_MODE | ((lba >> 24) & 0x0F));
    outb(IDE_SECTOR_CNT, num_sectors);
    outb(IDE_LBA_LOW, lba & 0xFF);
    outb(IDE_LBA_MID, (lba >> 8) & 0xFF);
    outb(IDE_LBA_HIGH, (lba >> 16) & 0xFF);
    
    // 发送读命令
    outb(IDE_COMMAND, IDE_CMD_READ);
    
    for (int sector = 0; sector < num_sectors; sector++) {
        // 等待数据就绪
        if (ide_wait_drq() != 0) {
            serial_puts("Read failed: DRQ not set\n");
            return -1;
        }
        
        
        // 读取256个字（512字节）
        for (int i = 0; i < 256; i++) {
            buf[i] = inw(IDE_DATA);
        }
        buf += 256;
        
        // 短暂延迟
        io_wait();
    }
    
    return 0;
}

int ide_write_sectors(uint32_t lba, uint8_t num_sectors, void* buffer) {
    uint16_t* buf = (uint16_t*)buffer;
    
    // 调试信息：打印当前尝试写入的地址
    // 如果报错时这个 LBA 很大（比如超过 129024），说明 FAT32 的簇计算逻辑有 Bug
    serial_puts("IDE Write -> LBA: ");
    serial_putdec64(lba);
    serial_puts(", Count: ");
    serial_putdec64(num_sectors);
    serial_puts("\n");

    if (num_sectors == 0) return -1;

    // 1. 等待就绪
    if (ide_wait_ready() != 0) return -1;
    
    // 2. 设置寄存器
    outb(IDE_DRIVE_HEAD, 0xE0 | ((lba >> 24) & 0x0F)); // 强制使用 LBA 模式
    outb(IDE_SECTOR_CNT, num_sectors);
    outb(IDE_LBA_LOW, lba & 0xFF);
    outb(IDE_LBA_MID, (lba >> 8) & 0xFF);
    outb(IDE_LBA_HIGH, (lba >> 16) & 0xFF);
    
    // 3. 发送写命令
    outb(IDE_COMMAND, IDE_CMD_WRITE);
    
    for (int sector = 0; sector < num_sectors; sector++) {
        // 4. 【关键】等待 DRQ。如果返回 -1，说明磁盘拒绝了写入请求（ABRT）
        if (ide_wait_drq() != 0) {
            serial_puts("Write failed at sector ");
            serial_putdec64(sector);
            serial_puts("\n");
            return -1; // 立即退出，不要写数据端口！
        }
        
        // 5. 写入 512 字节
        for (int i = 0; i < 256; i++) {
            outw(IDE_DATA, buf[i]);
        }
        buf += 256;
        
        // 短暂延迟让硬件处理
        io_wait();
    }
    
    // 6. 刷新缓存并等待完成
    outb(IDE_COMMAND, 0xE7); // IDE_CMD_FLUSH
    ide_wait_ready();
    
    return 0;
}

void ide_identify(void) {
    uint16_t buffer[256];
    
    serial_puts("Attempting to identify IDE device...\n");
    
    // 等待磁盘就绪
    ide_wait_ready();
    
    // 发送识别命令
    outb(IDE_DRIVE_HEAD, defult_device);
    outb(IDE_SECTOR_CNT, 0);
    outb(IDE_LBA_LOW, 0);
    outb(IDE_LBA_MID, 0);
    outb(IDE_LBA_HIGH, 0);
    outb(IDE_COMMAND, IDE_CMD_IDENT);
    
    // 检查状态
    uint8_t status = inb(IDE_STATUS);
    if (status == 0) {
        serial_puts("No drive present\n");
        return;
    }
    
    // 等待数据就绪
    ide_wait_drq();
    
    // 读取识别数据
    for (int i = 0; i < 256; i++) {
        buffer[i] = inw(IDE_DATA);
    }
    
    // 提取型号（位于27-46字）
    char model[41];
    for (int i = 0; i < 20; i++) {
        // 注意：IDE字符串是字节交换的
        uint16_t word = buffer[27 + i];
        model[i * 2] = word >> 8;
        model[i * 2 + 1] = word & 0xFF;
    }
    model[40] = '\0';
    
    // 去除尾部空格
    for (int i = 39; i >= 0 && model[i] == ' '; i--) {
        model[i] = '\0';
    }
    
    serial_puts("IDE Device Model: ");
    serial_puts(model);
    serial_puts("\n");
    
    // 打印容量信息
    uint32_t sectors = *(uint32_t*)&buffer[60];
    serial_puts("Total Sectors: ");
    
    // 简单的数字转字符串（仅用于调试）
    char num_str[12];
    int n = 0;
    uint32_t temp = sectors;
    do {
        num_str[n++] = '0' + (temp % 10);
        temp /= 10;
    } while (temp > 0);
    
    for (int i = n - 1; i >= 0; i--) {
        char c[2] = {num_str[i], 0};
        serial_puts(c);
    }
    serial_puts("\n");
}