// storage.h
#ifndef STORAGE_H
#define STORAGE_H

#include <stdint.h>

// 前向声明
struct sata_device;

// 声明多扇区版本的函数（与 sata.h 保持一致）
extern int sata_read_sectors(struct sata_device* dev, uint64_t lba, uint32_t count, void* buffer);
extern int sata_write_sectors(struct sata_device* dev, uint64_t lba, uint32_t count, const void* buffer);
// 定义简化的单扇区版本（不使用 struct sata_device* 参数）
// 这些函数会在内部处理设备选择
int storage_read_sector(uint64_t lba, void* buffer);
int storage_write_sector(uint64_t lba, const void* buffer);

// 通用的块读取函数（自动选择NVMe或SATA）
int storage_read_sectors(uint64_t lba, uint32_t count, void* buffer);
int storage_write_sectors(uint64_t lba, uint32_t count, const void* buffer);

#endif