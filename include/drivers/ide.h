#ifndef IDE_H
#define IDE_H

#include <stdint.h>

// IDE端口定义（主通道）
#define IDE_DATA        0x1F0
#define IDE_ERROR       0x1F1
#define IDE_FEATURES    0x1F1
#define IDE_SECTOR_CNT  0x1F2
#define IDE_LBA_LOW     0x1F3
#define IDE_LBA_MID     0x1F4
#define IDE_LBA_HIGH    0x1F5
#define IDE_DRIVE_HEAD  0x1F6
#define IDE_STATUS      0x1F7
#define IDE_COMMAND     0x1F7
#define IDE_ALT_STATUS  0x3F6
#define IDE_DEV_CTRL    0x3F6

// 状态寄存器位
#define IDE_STATUS_ERR  0x01    // 错误
#define IDE_STATUS_DRQ  0x08    // 数据请求就绪
#define IDE_STATUS_SRV  0x10    // 服务请求
#define IDE_STATUS_DF   0x20    // 驱动器故障
#define IDE_STATUS_RDY  0x40    // 驱动器就绪
#define IDE_STATUS_BSY  0x80    // 忙

// 命令
#define IDE_CMD_READ    0x20    // 读扇区
#define IDE_CMD_WRITE   0x30    // 写扇区
#define IDE_CMD_IDENT   0xEC    // 识别设备

// 设备选择
#define IDE_MASTER      0xE0    // 主设备
#define IDE_SLAVE       0xF0    // 从设备
#define IDE_LBA_MODE    0x40    // LBA模式

//默认设备
#define defult_device   0xF0

// 错误寄存器位
#define IDE_ERR_AMNF    0x01    // 地址标记未找到
#define IDE_ERR_TK0NF   0x02    // 磁道0未找到
#define IDE_ERR_ABRT    0x04    // 命令被中止
#define IDE_ERR_MCR     0x08    // 介质改变请求
#define IDE_ERR_IDNF    0x10    // ID未找到
#define IDE_ERR_MC      0x20    // 介质改变
#define IDE_ERR_UNC     0x40    // 不可纠正的数据错误
#define IDE_ERR_BBK     0x80    // 坏块检测

void ide_init(void);
int ide_read_sectors(uint32_t lba, uint8_t num_sectors, void* buffer);
int ide_write_sectors(uint32_t lba, uint8_t num_sectors, void* buffer);
void ide_identify(void);
uint8_t ide_get_status(void);
void ide_wait_not_busy(void);
int ide_wait_drq(void);

#endif // IDE_H