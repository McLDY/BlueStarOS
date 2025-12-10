// sata.h
#ifndef SATA_H
#define SATA_H

#include <stdint.h>
#include <stddef.h>

#define AHCI_CAP        0x00
#define AHCI_GHC        0x04
#define AHCI_IS         0x08
#define AHCI_PI         0x0C
#define AHCI_VS         0x10
#define AHCI_CCC_CTL    0x14
#define AHCI_CCC_PORTS  0x18
#define AHCI_EM_LOC     0x1C
#define AHCI_EM_CTL     0x20
#define AHCI_CAP2       0x24
#define AHCI_BOHC       0x28

#define PORT_CLB        0x00
#define PORT_CLBU       0x04
#define PORT_FB         0x08
#define PORT_FBU        0x0C
#define PORT_IS         0x10
#define PORT_IE         0x14
#define PORT_CMD        0x18
#define PORT_TFD        0x20
#define PORT_SIG        0x24
#define PORT_SSTS       0x28
#define PORT_SCTL       0x2C
#define PORT_SERR       0x30
#define PORT_SACT       0x34
#define PORT_CI         0x38
#define PORT_SNTF       0x3C
#define PORT_FBS        0x40

#define HBA_PxCMD_ST    0
#define HBA_PxCMD_FRE   4
#define HBA_PxCMD_FR    14
#define HBA_PxCMD_CR    15

#define HBA_PORT_IPM_ACTIVE  0x01
#define HBA_PORT_DET_PRESENT 0x03

#define ATA_CMD_IDENTIFY     0xEC
#define ATA_CMD_READ_DMA     0xC8
#define ATA_CMD_WRITE_DMA    0xCA

#define FIS_TYPE_REG_H2D     0x27
#define FIS_TYPE_REG_D2H     0x34
#define FIS_TYPE_DMA_ACT     0x39
#define FIS_TYPE_DMA_SETUP   0x41
#define FIS_TYPE_DATA        0x46
#define FIS_TYPE_BIST        0x58
#define FIS_TYPE_PIO_SETUP   0x5F
#define FIS_TYPE_DEV_BITS    0xA1

#define HBA_CMD_CFL_MASK     0x1F
#define HBA_CMD_W            (1 << 6)
#define HBA_CMD_P            (1 << 7)
#define HBA_CMD_R            (1 << 8)
#define HBA_CMD_B            (1 << 9)
#define HBA_CMD_C            (1 << 10)
#define HBA_CMD_PRDTL_SHIFT  16

#define AHCI_MAX_PORTS       32
#define AHCI_CMD_SLOTS       32
#define AHCI_PRDT_ENTRIES    8
#define AHCI_PAGE_SIZE       4096

struct fis_reg_h2d {
    uint8_t fis_type;
    uint8_t pm_port : 4;
    uint8_t reserved0 : 3;
    uint8_t c : 1;
    uint8_t command;
    uint8_t feature_low;
    
    uint8_t lba0;
    uint8_t lba1;
    uint8_t lba2;
    uint8_t device;
    
    uint8_t lba3;
    uint8_t lba4;
    uint8_t lba5;
    uint8_t feature_high;
    
    uint8_t count_low;
    uint8_t count_high;
    uint8_t icc;
    uint8_t control;
    
    uint8_t reserved1[4];
} __attribute__((packed));

struct fis_reg_d2h {
    uint8_t fis_type;
    uint8_t pm_port : 4;
    uint8_t reserved0 : 2;
    uint8_t i : 1;
    uint8_t reserved1 : 1;
    
    uint8_t status;
    uint8_t error;
    
    uint8_t lba0;
    uint8_t lba1;
    uint8_t lba2;
    uint8_t device;
    
    uint8_t lba3;
    uint8_t lba4;
    uint8_t lba5;
    uint8_t reserved2;
    
    uint8_t count_low;
    uint8_t count_high;
    uint8_t reserved3[2];
    
    uint8_t reserved4[4];
} __attribute__((packed));

struct fis_data {
    uint8_t fis_type;
    uint8_t pm_port : 4;
    uint8_t reserved0 : 4;
    uint8_t reserved1[2];
    
    uint32_t data[1];
} __attribute__((packed));

struct hba_cmd_header {
    uint16_t cfl : 5;
    uint16_t atapi : 1;
    uint16_t write : 1;
    uint16_t prefetchable : 1;
    uint16_t reset : 1;
    uint16_t bist : 1;
    uint16_t clear_busy : 1;
    uint16_t reserved0 : 1;
    uint16_t prdtl : 16;
    
    uint32_t prdbc;
    uint32_t ctba;
    uint32_t ctbau;
    uint32_t reserved1[4];
} __attribute__((packed));

struct hba_prdt_entry {
    uint32_t dba;
    uint32_t dbau;
    uint32_t reserved0;
    uint32_t dbc : 22;
    uint32_t reserved1 : 9;
    uint32_t i : 1;
} __attribute__((packed));

struct hba_cmd_table {
    uint8_t cfis[64];
    uint8_t acmd[16];
    uint8_t reserved[48];
    struct hba_prdt_entry prdt_entry[AHCI_PRDT_ENTRIES];
} __attribute__((packed));

struct hba_port {
    volatile uint32_t clb;
    volatile uint32_t clbu;
    volatile uint32_t fb;
    volatile uint32_t fbu;
    volatile uint32_t is;
    volatile uint32_t ie;
    volatile uint32_t cmd;
    volatile uint32_t reserved0;
    volatile uint32_t tfd;
    volatile uint32_t sig;
    volatile uint32_t ssts;
    volatile uint32_t sctl;
    volatile uint32_t serr;
    volatile uint32_t sact;
    volatile uint32_t ci;
    volatile uint32_t sntf;
    volatile uint32_t fbs;
    volatile uint32_t reserved1[11];
    volatile uint32_t vendor[4];
} __attribute__((packed));

struct hba_memory {
    volatile uint32_t cap;
    volatile uint32_t ghc;
    volatile uint32_t is;
    volatile uint32_t pi;
    volatile uint32_t vs;
    volatile uint32_t ccc_ctl;
    volatile uint32_t ccc_ports;
    volatile uint32_t em_loc;
    volatile uint32_t em_ctl;
    volatile uint32_t cap2;
    volatile uint32_t bohc;
    volatile uint32_t reserved[29];
    volatile uint32_t vendor[24];
    volatile struct hba_port ports[32];
} __attribute__((packed));

struct sata_device {
    struct hba_port* port;
    uint8_t port_num;
    uint8_t type;
    uint64_t sector_count;
    uint32_t sector_size;
    uint64_t command_list_phys;
    struct hba_cmd_header* command_list;
    uint64_t fis_phys;
    void* fis;
    uint64_t command_tables_phys[AHCI_CMD_SLOTS];
    struct hba_cmd_table* command_tables[AHCI_CMD_SLOTS];
    uint8_t initialized;
};

// 函数声明
int sata_init(void);
int sata_read_sectors(struct sata_device* dev, uint64_t lba, uint32_t count, void* buffer);
int sata_write_sectors(struct sata_device* dev, uint64_t lba, uint32_t count, const void* buffer);
void sata_test(void);
int sata_probe(void);
struct sata_device* sata_get_device(void);

#endif