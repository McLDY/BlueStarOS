// storage.c
#include "storage.h"
#include "sata.h"
#include "serial.h"

// 通用的存储读取函数
int storage_read_sectors(uint64_t lba, uint32_t count, void* buffer) {
    struct sata_device* sata_dev = sata_get_device();
    if (sata_dev && sata_read_sectors(sata_dev, lba, count, buffer)) {
        return 1;
    }
    
    return 0;
}

// 通用的存储写入函数
int storage_write_sectors(uint64_t lba, uint32_t count, const void* buffer) {
    struct sata_device* sata_dev = sata_get_device();
    if (sata_dev && sata_write_sectors(sata_dev, lba, count, buffer)) {
        return 1;
    }
    
    return 0;
}

// 单扇区读取
int storage_read_sector(uint64_t lba, void* buffer) {
    return storage_read_sectors(lba, 1, buffer);
}

// 单扇区写入
int storage_write_sector(uint64_t lba, const void* buffer) {
    return storage_write_sectors(lba, 1, buffer);
}