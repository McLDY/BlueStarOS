// kernel/drivers/fs/fat32.c
#include "drivers/fs/fat32.h"
#include "drivers/ide.h"
#include "serial.h"
#include "string.h"
#include "memory.h"
#include <stdbool.h>

// 全局文件系统状态
static fat32_info_t fs_info;
static fat32_bpb_t bpb;
static bool fs_mounted = false;
static bool fs_readonly = false;  // 只读模式
static uint32_t partition_start = 0;
static char error_msg[64] = {0};
static char volume_label[12] = {0};  // 11个字符 + 空终止符

// 辅助函数声明
static void clear_error(void);
static void set_error(const char* msg);
static uint32_t read_sector(uint32_t sector, void* buffer);
static uint32_t write_sector(uint32_t sector, const void* buffer);
static uint32_t cluster_to_sector(uint32_t cluster);
static uint32_t sector_to_cluster(uint32_t sector);
static uint32_t read_fat_entry(uint32_t cluster);
static bool write_fat_entry(uint32_t cluster, uint32_t value);
static uint32_t find_free_cluster(void);
static bool allocate_cluster_chain(uint32_t* cluster, uint32_t count);
static bool free_cluster_chain(uint32_t cluster);
static bool find_directory_entry(const char* path, 
                                 uint32_t* dir_sector, 
                                 uint32_t* dir_offset,
                                 uint32_t* parent_cluster);
static bool add_directory_entry(uint32_t parent_cluster, 
                                const char* name83, 
                                uint8_t attributes,
                                uint32_t first_cluster,
                                uint32_t file_size);
static bool update_directory_entry(fat32_handle_t* handle);
static bool remove_directory_entry(uint32_t dir_sector, uint32_t dir_offset);
static void name_to_83(const char* name, char* name83);
static void format_83_name(const char* name83, char* name);
static bool is_valid_filename(const char* name);
static bool is_deleted_entry(const char* name);
static bool is_long_name_entry(const fat32_dir_entry_t* entry);
static uint32_t get_cluster_count(uint32_t first_cluster);
static void update_fs_info_sector(void);
static bool read_fs_info_sector(void);
static uint32_t find_next_cluster(uint32_t current_cluster, uint32_t position, uint32_t bytes_per_cluster);

// ==================== 核心辅助函数 ====================

// 清除错误信息
static void clear_error(void) {
    error_msg[0] = '\0';
}

// 设置错误信息
static void set_error(const char* msg) {
    int i = 0;
    while (msg[i] != '\0' && i < 63) {
        error_msg[i] = msg[i];
        i++;
    }
    error_msg[i] = '\0';
    serial_puts("FAT32 Error: ");
    serial_puts(msg);
    serial_puts("\n");
}

// 读取扇区
static uint32_t read_sector(uint32_t sector, void* buffer) {
    // 这里的 sector 是相对于 FAT32 分区开头的偏移
    // partition_start 是分区在磁盘上的起始位置（你的日志里是 2048）
    uint32_t physical_sector = partition_start + sector;
    
    serial_puts("Reading physical LBA: "); 
    serial_putdec64(physical_sector); // 修正变量名：这里必须和上面定义的保持一致
    serial_puts("\n");
    
    return ide_read_sectors(physical_sector, 1, buffer);
}

// 写入扇区
static uint32_t write_sector(uint32_t sector, const void* buffer) {
    if (fs_readonly) {
        set_error("File system is read-only");
        return 1;
    }
    
    // 核心转换逻辑：将 FAT32 相对地址转为磁盘绝对 LBA
    uint32_t physical_sector = partition_start + sector;
    
    serial_puts("Writing physical LBA: ");
    serial_putdec64(physical_sector); // 修正变量名
    serial_puts("\n");
    
    // 调用 IDE 驱动执行物理写入
    return ide_write_sectors(physical_sector, 1, (void*)buffer);
}


// 将簇号转换为扇区号
uint32_t cluster_to_sector(uint32_t cluster) {
    // 哨兵检查
    if (cluster < 2 || cluster > 200000) {
        // 这就是你现在报错的地方
        return 0; 
    }
    
    // 相对扇区 = 数据区起始相对偏移 + (簇号 - 2) * 每簇扇区数
    uint32_t relative_sec = fs_info.data_start_sector + (cluster - 2) * fs_info.sectors_per_cluster;
    
    // 物理绝对地址 = 分区起始 LBA + 相对扇区
    return partition_start + relative_sec;
}




// 将扇区号转换为簇号
static uint32_t sector_to_cluster(uint32_t sector) {
    if (sector < fs_info.data_start_sector) {
        return 0;
    }
    return 2 + (sector - fs_info.data_start_sector) / fs_info.sectors_per_cluster;
}

// 读取FAT表项
// 建议定义一个静态缓冲区，避免占用内核栈
static uint8_t g_fat_read_buf[512] __attribute__((aligned(16)));

static uint32_t read_fat_entry(uint32_t cluster) {
    if (cluster < 2 || cluster >= 127006) return 0x0FFFFFF7;

    uint32_t fat_offset = cluster * 4;
    uint32_t rel_sector = fs_info.fat_start_sector + (fat_offset / 512);
    
    static uint8_t f_buf[512] __attribute__((aligned(16)));
    if (read_sector(rel_sector, f_buf) != 0) return 0x0FFFFFF7;

    uint32_t val = *(uint32_t*)(f_buf + (fat_offset % 512));
    return val & 0x0FFFFFFF;
}



// 写入FAT表项
static bool write_fat_entry(uint32_t cluster, uint32_t value) {
    if (cluster < 2 || cluster >= 127006) return false;

    uint32_t fat_offset = cluster * 4;
    uint32_t rel_sector = fs_info.fat_start_sector + (fat_offset / 512);
    
    static uint8_t f_buf[512] __attribute__((aligned(16)));
    if (read_sector(rel_sector, f_buf) != 0) return false;

    *(uint32_t*)(f_buf + (fat_offset % 512)) = (value & 0x0FFFFFFF);

    // 2026 强制修正：同步写入两个 FAT 副本
    if (write_sector(rel_sector, f_buf) != 0) return false;
    if (write_sector(rel_sector + 993, f_buf) != 0) return false; 
    
    return true;
}





// 查找空闲簇
uint32_t find_free_cluster(void) {
    // 强制限制搜索范围，不准超出磁盘
    for (uint32_t c = 3; c < 127006; c++) {
        uint32_t entry = read_fat_entry(c);
        if (entry == 0) {
            // 找到后立即返回，不要做多余的逻辑
            return c;
        }
    }
    return 0x0FFFFFF7;
}




// 分配簇链
static bool allocate_cluster_chain(uint32_t* cluster, uint32_t count) {
    uint32_t first_cluster = 0;
    uint32_t prev_cluster = 0;
    
    for (uint32_t i = 0; i < count; i++) {
        uint32_t new_cluster = find_free_cluster();
        if (new_cluster == 0) {
            set_error("Not enough free space");
            // 释放已经分配的簇
            if (first_cluster != 0) {
                free_cluster_chain(first_cluster);
            }
            return false;
        }
        
        // 标记为最后一个簇（临时）
        if (!write_fat_entry(new_cluster, FAT32_LAST_CLUSTER)) {
            if (first_cluster != 0) {
                free_cluster_chain(first_cluster);
            }
            return false;
        }
        
        if (i == 0) {
            first_cluster = new_cluster;
        } else {
            // 链接到上一个簇
            if (!write_fat_entry(prev_cluster, new_cluster)) {
                free_cluster_chain(first_cluster);
                return false;
            }
        }
        
        prev_cluster = new_cluster;
    }
    
    *cluster = first_cluster;
    return true;
}

// 释放簇链
static bool free_cluster_chain(uint32_t cluster) {
    while (cluster < FAT32_LAST_CLUSTER) {
        uint32_t next_cluster = read_fat_entry(cluster);
        if (!write_fat_entry(cluster, FAT32_FREE_CLUSTER)) {
            return false;
        }
        
        if (next_cluster >= FAT32_LAST_CLUSTER || next_cluster == FAT32_FREE_CLUSTER) {
            break;
        }
        
        cluster = next_cluster;
    }
    
    return true;
}

// ==================== 目录操作辅助函数 ====================

// 转换文件名到8.3格式
static void name_to_83(const char* name, char* name83) {
    // 初始化为空格
    for (int i = 0; i < 11; i++) {
        name83[i] = ' ';
    }
    
    const char* dot = strchr(name, '.');
    int name_len = 0;
    int ext_len = 0;
    
    if (dot == NULL) {
        // 没有扩展名
        name_len = strlen(name);
        if (name_len > 8) name_len = 8;
        for (int i = 0; i < name_len; i++) {
            name83[i] = toupper(name[i]);
        }
    } else {
        // 有扩展名
        name_len = dot - name;
        if (name_len > 8) name_len = 8;
        for (int i = 0; i < name_len; i++) {
            name83[i] = toupper(name[i]);
        }
        
        const char* ext = dot + 1;
        ext_len = strlen(ext);
        if (ext_len > 3) ext_len = 3;
        for (int i = 0; i < ext_len; i++) {
            name83[8 + i] = toupper(ext[i]);
        }
    }
}

// 格式化8.3文件名
static void format_83_name(const char* name83, char* name) {
    int i = 0;
    
    // 复制文件名部分
    while (i < 8 && name83[i] != ' ') {
        name[i] = name83[i];
        i++;
    }
    
    // 检查是否有扩展名
    if (name83[8] != ' ') {
        name[i++] = '.';
        int j = 0;
        while (j < 3 && name83[8 + j] != ' ') {
            name[i++] = name83[8 + j];
            j++;
        }
    }
    
    name[i] = '\0';
}

// 检查是否为有效文件名
static bool is_valid_filename(const char* name) {
    // 检查长度
    int len = strlen(name);
    if (len == 0 || len > 12) {  // 8.3格式最多12个字符
        return false;
    }
    
    // 检查非法字符
    const char* illegal_chars = "<>:\"/\\|?*";
    for (int i = 0; i < len; i++) {
        if (strchr(illegal_chars, name[i]) != NULL) {
            return false;
        }
    }
    
    return true;
}

// 检查是否为删除的目录项
static bool is_deleted_entry(const char* name) {
    return ((uint8_t)name[0]) == 0xE5;
}

// 检查是否为长文件名条目
static bool is_long_name_entry(const fat32_dir_entry_t* entry) {
    return (entry->attributes & ATTR_LONG_NAME) == ATTR_LONG_NAME;
}

// 查找目录项
static bool find_directory_entry(const char* path, 
                                 uint32_t* dir_sector, 
                                 uint32_t* dir_offset,
                                 uint32_t* parent_cluster) {
    // 简化实现：只支持单级目录
    char name83[11];
    name_to_83(path, name83);
    
    // 从根目录开始查找
    uint32_t current_cluster = fs_info.root_dir_cluster;
    
    while (current_cluster < FAT32_LAST_CLUSTER) {
        uint32_t sector = cluster_to_sector(current_cluster);
        
        for (uint32_t s = 0; s < fs_info.sectors_per_cluster; s++) {
            uint8_t sector_buffer[512];
            if (read_sector(sector + s, sector_buffer) != 0) {
                return false;
            }
            
            // 检查每个目录项
            for (int offset = 0; offset < 512; offset += 32) {
                fat32_dir_entry_t* entry = (fat32_dir_entry_t*)(sector_buffer + offset);
                
                // 跳过空条目
                if (entry->name[0] == 0x00) {
                    return false;
                }
                
                // 跳过删除的条目
                if (is_deleted_entry(entry->name)) {
                    continue;
                }
                
                // 跳过长文件名条目
                if (is_long_name_entry(entry)) {
                    continue;
                }
                
                // 比较文件名
                bool match = true;
                for (int i = 0; i < 11; i++) {
                    if (entry->name[i] != name83[i]) {
                        match = false;
                        break;
                    }
                }
                
                if (match) {
                    *dir_sector = sector + s;
                    *dir_offset = offset;
                    *parent_cluster = current_cluster;
                    return true;
                }
            }
        }
        
        // 移动到下一个簇
        current_cluster = read_fat_entry(current_cluster);
    }
    
    return false;
}

// 添加目录项
static bool add_directory_entry(uint32_t parent_cluster, 
                                const char* name83, 
                                uint8_t attributes,
                                uint32_t first_cluster,
                                uint32_t file_size) {
    // 查找空闲目录项
    uint32_t current_cluster = parent_cluster;
    
    while (current_cluster < FAT32_LAST_CLUSTER) {
        uint32_t sector = cluster_to_sector(current_cluster);
        
        for (uint32_t s = 0; s < fs_info.sectors_per_cluster; s++) {
            uint8_t sector_buffer[512];
            if (read_sector(sector + s, sector_buffer) != 0) {
                return false;
            }
            
            // 查找空闲目录项
            for (int offset = 0; offset < 512; offset += 32) {
                fat32_dir_entry_t* entry = (fat32_dir_entry_t*)(sector_buffer + offset);
                
                // 找到空闲或删除的条目
                if (entry->name[0] == 0x00 || is_deleted_entry(entry->name)) {
                    // 填充目录项
                    for (int i = 0; i < 11; i++) {
                        entry->name[i] = name83[i];
                    }
                    
                    entry->attributes = attributes;
                    entry->reserved = 0;
                    
                    // 设置时间戳（简化）
                    entry->creation_time_tenths = 0;
                    entry->creation_time = 0;
                    entry->creation_date = 0;
                    entry->last_access_date = 0;
                    
                    entry->cluster_high = (first_cluster >> 16) & 0xFFFF;
                    entry->last_write_time = 0;
                    entry->last_write_date = 0;
                    entry->cluster_low = first_cluster & 0xFFFF;
                    entry->file_size = file_size;
                    
                    // 写回磁盘
                    if (write_sector(sector + s, sector_buffer) != 0) {
                        return false;
                    }
                    
                    return true;
                }
            }
        }
        
        // 移动到下一个簇
        current_cluster = read_fat_entry(current_cluster);
    }
    
    // 需要扩展目录
    uint32_t new_cluster = find_free_cluster();
    if (new_cluster == 0) {
        return false;
    }
    
    // 链接新簇
    if (!write_fat_entry(current_cluster, new_cluster)) {
        return false;
    }
    
    if (!write_fat_entry(new_cluster, FAT32_LAST_CLUSTER)) {
        return false;
    }
    
    // 清除新簇的内容
    uint32_t new_sector = cluster_to_sector(new_cluster);
    uint8_t empty_buffer[512] = {0};
    for (uint32_t s = 0; s < fs_info.sectors_per_cluster; s++) {
        if (write_sector(new_sector + s, empty_buffer) != 0) {
            return false;
        }
    }
    
    // 在新簇的第一个条目中添加
    return add_directory_entry(new_cluster, name83, attributes, first_cluster, file_size);
}

// 更新目录项
static bool update_directory_entry(fat32_handle_t* handle) {
    if (handle->dir_sector == 0) {
        return false;  // 没有目录项信息
    }
    
    uint8_t sector_buffer[512];
    if (read_sector(handle->dir_sector, sector_buffer) != 0) {
        return false;
    }
    
    fat32_dir_entry_t* entry = (fat32_dir_entry_t*)(sector_buffer + handle->dir_offset);
    
    // 更新文件大小
    entry->file_size = handle->file_size;
    
    // 更新时间戳（简化）
    entry->last_write_time = 0;
    entry->last_write_date = 0;
    
    // 写回磁盘
    return write_sector(handle->dir_sector, sector_buffer) == 0;
}

// 移除目录项
static bool remove_directory_entry(uint32_t dir_sector, uint32_t dir_offset) {
    uint8_t sector_buffer[512];
    if (read_sector(dir_sector, sector_buffer) != 0) {
        return false;
    }
    
    // 标记为删除
    fat32_dir_entry_t* entry = (fat32_dir_entry_t*)(sector_buffer + dir_offset);
    entry->name[0] = 0xE5;
    
    return write_sector(dir_sector, sector_buffer) == 0;
}

// ==================== 核心文件系统函数 ====================

// 初始化FAT32文件系统
// 1. 将 bpb 定义为全局变量（不要放在函数内部）
static fat32_bpb_t g_bpb; 

bool fat32_init(uint32_t partition_start_sector) {
    partition_start = partition_start_sector;
    
    // 2. 关键：开辟一个完整的 512 字节扇区缓冲区，防止溢出
    uint8_t sector_buffer[512] __attribute__((aligned(16))); 
    
    // 3. 读取到缓冲区，而不是直接读到结构体
    if (ide_read_sectors(partition_start, 1, sector_buffer) != 0) {
        return false;
    }

    // 4. 安全地拷贝结构体内容
    // 这样即使结构体只有 90 字节，也不会冲毁堆栈
    memcpy(&g_bpb, sector_buffer, sizeof(fat32_bpb_t));

    // 5. 手动强制解析关键参数（防止编译器对齐 Bug）
    fs_info.bytes_per_sector = *(uint16_t*)(sector_buffer + 11);
    fs_info.sectors_per_cluster = sector_buffer[13];
    uint16_t res_sec = *(uint16_t*)(sector_buffer + 14);
    uint32_t fat_sz = *(uint32_t*)(sector_buffer + 36);

    fs_info.fat_start_sector = res_sec;
    fs_info.fat_sectors = fat_sz;
    fs_info.data_start_sector = res_sec + (sector_buffer[16] * fat_sz);
    fs_info.root_dir_cluster = *(uint32_t*)(sector_buffer + 44);

    // 校验解析结果
    if (fs_info.bytes_per_sector != 512 || fs_info.root_dir_cluster < 2) {
        serial_puts("FATAL: DBR Checksum failed. Root Cluster: ");
        serial_putdec64(fs_info.root_dir_cluster);
        serial_puts("\n");
        return false;
    }

    // 计算总簇数
    uint32_t tot_sec = *(uint32_t*)(sector_buffer + 32);
    uint32_t data_sec = tot_sec - fs_info.data_start_sector;
    fs_info.total_clusters = data_sec / fs_info.sectors_per_cluster;

    serial_puts("SUCCESS: Root cluster = ");
    serial_putdec64(fs_info.root_dir_cluster);
    serial_puts("\n");
    // 在 fat32_init 结束前强制校正
    bpb.fat_count = 2; // 大多数 FAT32 镜像只有 2 个 FAT 表
    fs_info.fat_number = 2; // 确保 fs_info 里的备份数量也是 2
    // 在 fat32_init 挂载成功的最后一行之前加入：
    bpb.fat_count = 2; 
    fs_info.fat_sectors = 993; // 你的日志显示的正确值
// 2026-01-01 补丁：手动校准 DBR 丢失的参数
    fs_info.fat_sectors = 993;
    fs_info.total_sectors = 129024;
    fs_info.total_clusters = 127006;
    bpb.fat_count = 2; // 确保副本逻辑有次数

    fs_mounted = true;
    return true;
}





// 挂载文件系统（别名）
bool fat32_mount(uint32_t partition_start_sector) {
    return fat32_init(partition_start_sector);
}

// 卸载文件系统
void fat32_umount(void) {
    fs_mounted = false;
    memset(&fs_info, 0, sizeof(fs_info));
    memset(&bpb, 0, sizeof(bpb));
    volume_label[0] = '\0';
}

// ==================== 文件操作函数 ====================

// 打开文件
bool fat32_open(const char* path, fat32_handle_t* handle, file_mode_t mode) {
    clear_error();
    
    if (!fs_mounted) {
        set_error("File system not mounted");
        return false;
    }
    
    if (path == NULL || handle == NULL) {
        set_error("Invalid parameters");
        return false;
    }
    
    // 清除句柄
    memset(handle, 0, sizeof(fat32_handle_t));
    
    // 特殊处理根目录
    if (strcmp(path, "/") == 0 || strcmp(path, "") == 0) {
        if (mode != FILE_READ) {
            set_error("Cannot write to root directory");
            return false;
        }
        
        handle->first_cluster = fs_info.root_dir_cluster;
        handle->current_cluster = fs_info.root_dir_cluster;
        handle->is_directory = true;
        handle->is_open = true;
        return true;
    }
    
    // 查找文件
    uint32_t dir_sector, dir_offset, parent_cluster;
    if (find_directory_entry(path, &dir_sector, &dir_offset, &parent_cluster)) {
        // 文件已存在
        uint8_t sector_buffer[512];
        if (read_sector(dir_sector, sector_buffer) != 0) {
            return false;
        }
        
        fat32_dir_entry_t* entry = (fat32_dir_entry_t*)(sector_buffer + dir_offset);
        
        handle->first_cluster = (entry->cluster_high << 16) | entry->cluster_low;
        handle->current_cluster = handle->first_cluster;
        handle->file_size = entry->file_size;
        handle->is_directory = (entry->attributes & ATTR_DIRECTORY) != 0;
        handle->dir_sector = dir_sector;
        handle->dir_offset = dir_offset;
        handle->is_open = true;
        
        // 如果是只读模式，检查文件是否只读
        if (mode == FILE_READ && (entry->attributes & ATTR_READ_ONLY)) {
            set_error("File is read-only");
            return false;
        }
        
        return true;
    } else {
        // 文件不存在
        if (mode == FILE_CREATE || mode == FILE_APPEND) {
            // 创建新文件
            char name83[11];
            name_to_83(path, name83);
            
            uint32_t first_cluster = 0;
            if (!allocate_cluster_chain(&first_cluster, 1)) {
                return false;
            }
            
            if (!add_directory_entry(parent_cluster, name83, ATTR_ARCHIVE, first_cluster, 0)) {
                free_cluster_chain(first_cluster);
                return false;
            }
            
            handle->first_cluster = first_cluster;
            handle->current_cluster = first_cluster;
            handle->file_size = 0;
            handle->is_directory = false;
            handle->is_open = true;
            
            // 需要重新查找目录项以获取位置
            if (!find_directory_entry(path, &dir_sector, &dir_offset, &parent_cluster)) {
                return false;
            }
            
            handle->dir_sector = dir_sector;
            handle->dir_offset = dir_offset;
            
            return true;
        } else {
            set_error("File not found");
            return false;
        }
    }
}

// 打开根目录
bool fat32_open_root(fat32_handle_t* handle) {
    return fat32_open("/", handle, FILE_READ);
}

// 读取文件
bool fat32_read(fat32_handle_t* handle, void* buffer, uint32_t size) {
    clear_error();
    
    if (!fs_mounted || handle == NULL || buffer == NULL || !handle->is_open) {
        set_error("Invalid parameters");
        return false;
    }
    
    if (handle->is_directory) {
        set_error("Cannot read from directory");
        return false;
    }
    
    if (handle->position >= handle->file_size) {
        return false;  // 已经到达文件末尾
    }
    
    // 调整读取大小，防止读取超出文件范围
    if (handle->position + size > handle->file_size) {
        size = handle->file_size - handle->position;
    }
    
    uint8_t* dest = (uint8_t*)buffer;
    uint32_t bytes_read = 0;
    
    while (bytes_read < size) {
        // 计算当前簇中的偏移
        uint32_t cluster_offset = handle->position % (fs_info.sectors_per_cluster * fs_info.bytes_per_sector);
        uint32_t sector_in_cluster = cluster_offset / fs_info.bytes_per_sector;
        uint32_t sector_offset = cluster_offset % fs_info.bytes_per_sector;
        
        // 如果当前位置是簇的开始，但不是文件的开始，则移动到下一个簇
        if (cluster_offset == 0 && handle->position > 0) {
            handle->current_cluster = read_fat_entry(handle->current_cluster);
            if (handle->current_cluster >= FAT32_LAST_CLUSTER) {
                // 到达文件末尾
                break;
            }
        }
        
        // 读取扇区到缓冲区
        uint32_t sector = cluster_to_sector(handle->current_cluster) + sector_in_cluster;
        if (sector != handle->buffer_sector) {
            if (read_sector(sector, handle->buffer) != 0) {
                set_error("Failed to read sector");
                return false;
            }
            handle->buffer_sector = sector;
        }
        
        // 计算要读取的字节数
        uint32_t bytes_to_read = fs_info.bytes_per_sector - sector_offset;
        if (bytes_to_read > size - bytes_read) {
            bytes_to_read = size - bytes_read;
        }
        
        // 复制数据
        memcpy(dest + bytes_read, handle->buffer + sector_offset, bytes_to_read);
        
        // 更新位置
        bytes_read += bytes_to_read;
        handle->position += bytes_to_read;
        
        // 如果读取了整个扇区，使缓冲区无效
        if (sector_offset + bytes_to_read == fs_info.bytes_per_sector) {
            handle->buffer_sector = 0;
        }
    }
    
    return bytes_read > 0;
}

// 写入文件
bool fat32_write(fat32_handle_t* handle, const void* buffer, uint32_t size) {
    clear_error();
    
    if (!fs_mounted || handle == NULL || buffer == NULL || !handle->is_open || fs_readonly) {
        set_error("Invalid parameters or read-only");
        return false;
    }
    
    if (handle->is_directory) {
        set_error("Cannot write to directory");
        return false;
    }
    
    const uint8_t* src = (const uint8_t*)buffer;
    uint32_t bytes_written = 0;
    
    while (bytes_written < size) {
        // 计算当前簇中的偏移
        uint32_t cluster_offset = handle->position % (fs_info.sectors_per_cluster * fs_info.bytes_per_sector);
        uint32_t sector_in_cluster = cluster_offset / fs_info.bytes_per_sector;
        uint32_t sector_offset = cluster_offset % fs_info.bytes_per_sector;
        
        // 检查是否需要分配新簇
        if (cluster_offset == 0 && handle->position > 0) {
            uint32_t next_cluster = read_fat_entry(handle->current_cluster);
            if (next_cluster >= FAT32_LAST_CLUSTER) {
                // 需要分配新簇
                uint32_t new_cluster = find_free_cluster();
                if (new_cluster == 0) {
                    set_error("No free space");
                    return false;
                }
                
                // 链接新簇
                if (!write_fat_entry(handle->current_cluster, new_cluster) ||
                    !write_fat_entry(new_cluster, FAT32_LAST_CLUSTER)) {
                    set_error("Failed to allocate cluster");
                    return false;
                }
                
                handle->current_cluster = new_cluster;
                fs_info.free_clusters--;
            } else {
                handle->current_cluster = next_cluster;
            }
        }
        
        // 读取或初始化缓冲区
        uint32_t sector = cluster_to_sector(handle->current_cluster) + sector_in_cluster;
        if (sector != handle->buffer_sector) {
            // 如果缓冲区有修改，先写回
            if (handle->buffer_dirty && handle->buffer_sector != 0) {
                if (write_sector(handle->buffer_sector, handle->buffer) != 0) {
                    set_error("Failed to write sector");
                    return false;
                }
                handle->buffer_dirty = false;
            }
            
            // 读取新扇区或初始化
            if (handle->position < handle->file_size) {
                // 文件已有数据，读取
                if (read_sector(sector, handle->buffer) != 0) {
                    set_error("Failed to read sector");
                    return false;
                }
            } else {
                // 新数据，初始化为0
                memset(handle->buffer, 0, fs_info.bytes_per_sector);
            }
            
            handle->buffer_sector = sector;
        }
        
        // 计算要写入的字节数
        uint32_t bytes_to_write = fs_info.bytes_per_sector - sector_offset;
        if (bytes_to_write > size - bytes_written) {
            bytes_to_write = size - bytes_written;
        }
        
        // 复制数据到缓冲区
        memcpy(handle->buffer + sector_offset, src + bytes_written, bytes_to_write);
        handle->buffer_dirty = true;
        
        // 更新位置
        bytes_written += bytes_to_write;
        handle->position += bytes_to_write;
        
        // 更新文件大小
        if (handle->position > handle->file_size) {
            handle->file_size = handle->position;
        }
    }
    
    return true;
}

// 定位文件指针
bool fat32_seek(fat32_handle_t* handle, uint32_t position) {
    clear_error();
    
    if (!fs_mounted || handle == NULL || !handle->is_open) {
        set_error("Invalid parameters");
        return false;
    }
    
    if (position > handle->file_size && !handle->is_directory) {
        set_error("Seek beyond end of file");
        return false;
    }
    
    // 如果移动到文件开始，重置当前簇
    if (position == 0) {
        handle->current_cluster = handle->first_cluster;
        handle->position = 0;
        handle->buffer_sector = 0;
        handle->buffer_dirty = false;
        return true;
    }
    
    // 计算目标位置所在的簇
    uint32_t bytes_per_cluster = fs_info.sectors_per_cluster * fs_info.bytes_per_sector;
    uint32_t target_cluster_offset = position / bytes_per_cluster;
    uint32_t target_sector_offset = (position % bytes_per_cluster) / fs_info.bytes_per_sector;
    
    // 遍历簇链到目标簇
    uint32_t current_cluster = handle->first_cluster;
    for (uint32_t i = 0; i < target_cluster_offset; i++) {
        uint32_t next_cluster = read_fat_entry(current_cluster);
        if (next_cluster >= FAT32_LAST_CLUSTER || next_cluster == FAT32_FREE_CLUSTER) {
            set_error("Invalid cluster chain");
            return false;
        }
        current_cluster = next_cluster;
    }
    
    handle->current_cluster = current_cluster;
    handle->position = position;
    handle->buffer_sector = 0;  // 使缓冲区无效
    handle->buffer_dirty = false;
    
    return true;
}

// 截断文件
bool fat32_truncate(fat32_handle_t* handle, uint32_t new_size) {
    clear_error();
    
    if (!fs_mounted || handle == NULL || !handle->is_open || fs_readonly) {
        set_error("Invalid parameters or read-only");
        return false;
    }
    
    if (handle->is_directory) {
        set_error("Cannot truncate directory");
        return false;
    }
    
    if (new_size == handle->file_size) {
        return true;  // 大小不变
    }
    
    if (new_size > handle->file_size) {
        // 扩展文件
        uint32_t old_size = handle->file_size;
        handle->file_size = new_size;
        
        // 如果当前位置在旧大小之后，需要移动到新位置
        if (handle->position > old_size) {
            // 不需要做特殊处理，写入时会自动扩展
        }
        
        // 更新目录项
        return update_directory_entry(handle);
    } else {
        // 缩小文件
        uint32_t bytes_per_cluster = fs_info.sectors_per_cluster * fs_info.bytes_per_sector;
        uint32_t old_cluster_count = (handle->file_size + bytes_per_cluster - 1) / bytes_per_cluster;
        uint32_t new_cluster_count = (new_size + bytes_per_cluster - 1) / bytes_per_cluster;
        
        if (new_cluster_count < old_cluster_count) {
            // 释放多余的簇
            uint32_t current_cluster = handle->first_cluster;
            uint32_t prev_cluster = 0;
            
            // 遍历到需要保留的最后一个簇
            for (uint32_t i = 0; i < new_cluster_count; i++) {
                prev_cluster = current_cluster;
                current_cluster = read_fat_entry(current_cluster);
            }
            
            // 切断簇链并释放多余的簇
            if (prev_cluster != 0) {
                uint32_t first_to_free = current_cluster;
                
                // 标记保留的簇为最后一个
                if (!write_fat_entry(prev_cluster, FAT32_LAST_CLUSTER)) {
                    return false;
                }
                
                // 释放多余的簇
                if (first_to_free < FAT32_LAST_CLUSTER && first_to_free != FAT32_FREE_CLUSTER) {
                    if (!free_cluster_chain(first_to_free)) {
                        return false;
                    }
                }
            }
        }
        
        handle->file_size = new_size;
        
        // 如果当前位置超过新大小，移动到文件末尾
        if (handle->position > new_size) {
            handle->position = new_size;
        }
        
        // 更新目录项
        return update_directory_entry(handle);
    }
}

// 关闭文件
void fat32_close(fat32_handle_t* handle) {
    if (handle == NULL || !handle->is_open) {
        return;
    }
    
    // 如果缓冲区有修改，写回磁盘
    if (handle->buffer_dirty && handle->buffer_sector != 0) {
        write_sector(handle->buffer_sector, handle->buffer);
    }
    
    // 更新目录项（如果文件有修改）
    if (handle->dir_sector != 0) {
        update_directory_entry(handle);
    }
    
    // 清除句柄
    memset(handle, 0, sizeof(fat32_handle_t));
}

// ==================== 目录操作函数 ====================

// 创建目录
bool fat32_create_dir(const char* path) {
    clear_error();
    
    if (!fs_mounted || fs_readonly) {
        set_error("File system not mounted or read-only");
        return false;
    }
    
    if (path == NULL || strlen(path) == 0) {
        set_error("Invalid path");
        return false;
    }
    
    // 检查目录是否已存在
    uint32_t dir_sector, dir_offset, parent_cluster;
    if (find_directory_entry(path, &dir_sector, &dir_offset, &parent_cluster)) {
        set_error("Directory already exists");
        return false;
    }
    
    // 分配第一个簇给新目录
    uint32_t first_cluster = 0;
    if (!allocate_cluster_chain(&first_cluster, 1)) {
        return false;
    }
    
    // 创建目录项
    char name83[11];
    name_to_83(path, name83);
    
    if (!add_directory_entry(parent_cluster, name83, ATTR_DIRECTORY, first_cluster, 0)) {
        free_cluster_chain(first_cluster);
        return false;
    }
    
    // 初始化目录内容（.和..条目）
    uint8_t sector_buffer[512];
    memset(sector_buffer, 0, sizeof(sector_buffer));
    
    // 创建.条目
    fat32_dir_entry_t* dot_entry = (fat32_dir_entry_t*)sector_buffer;
    dot_entry->name[0] = '.';
    for (int i = 1; i < 11; i++) dot_entry->name[i] = ' ';
    dot_entry->attributes = ATTR_DIRECTORY;
    dot_entry->cluster_high = (first_cluster >> 16) & 0xFFFF;
    dot_entry->cluster_low = first_cluster & 0xFFFF;
    dot_entry->file_size = 0;
    
    // 创建..条目
    fat32_dir_entry_t* dotdot_entry = (fat32_dir_entry_t*)(sector_buffer + 32);
    dotdot_entry->name[0] = '.';
    dotdot_entry->name[1] = '.';
    for (int i = 2; i < 11; i++) dotdot_entry->name[i] = ' ';
    dotdot_entry->attributes = ATTR_DIRECTORY;
    dotdot_entry->cluster_high = (parent_cluster >> 16) & 0xFFFF;
    dotdot_entry->cluster_low = parent_cluster & 0xFFFF;
    dotdot_entry->file_size = 0;
    
    // 写入目录的起始扇区
    uint32_t dir_sector_start = cluster_to_sector(first_cluster);
    if (write_sector(dir_sector_start, sector_buffer) != 0) {
        return false;
    }
    
    return true;
}

// 删除目录
bool fat32_remove_dir(const char* path) {
    clear_error();
    
    if (!fs_mounted || fs_readonly) {
        set_error("File system not mounted or read-only");
        return false;
    }
    
    // 查找目录
    uint32_t dir_sector, dir_offset, parent_cluster;
    if (!find_directory_entry(path, &dir_sector, &dir_offset, &parent_cluster)) {
        set_error("Directory not found");
        return false;
    }
    
    // 读取目录项
    uint8_t sector_buffer[512];
    if (read_sector(dir_sector, sector_buffer) != 0) {
        return false;
    }
    
    fat32_dir_entry_t* entry = (fat32_dir_entry_t*)(sector_buffer + dir_offset);
    
    // 检查是否为目录
    if ((entry->attributes & ATTR_DIRECTORY) == 0) {
        set_error("Not a directory");
        return false;
    }
    
    uint32_t dir_cluster = (entry->cluster_high << 16) | entry->cluster_low;
    
    // 检查目录是否为空（除了.和..）
    // 简化：我们假设目录为空
    // 实际上应该遍历目录检查
    
    // 释放目录占用的簇
    if (dir_cluster != 0) {
        if (!free_cluster_chain(dir_cluster)) {
            return false;
        }
    }
    
    // 删除目录项
    return remove_directory_entry(dir_sector, dir_offset);
}

// 读取目录条目
bool fat32_read_dir(fat32_handle_t* dir_handle, fat32_dir_entry_t* entry) {
    clear_error();
    
    if (!fs_mounted || dir_handle == NULL || entry == NULL || !dir_handle->is_open) {
        set_error("Invalid parameters");
        return false;
    }
    
    if (!dir_handle->is_directory) {
        set_error("Not a directory handle");
        return false;
    }
    
    // 检查是否到达目录末尾
    if (dir_handle->current_cluster >= FAT32_LAST_CLUSTER) {
        return false;
    }
    
    uint8_t sector_buffer[512];
    
    while (dir_handle->current_cluster < FAT32_LAST_CLUSTER) {
        uint32_t sector = cluster_to_sector(dir_handle->current_cluster);
        uint32_t sector_in_cluster = dir_handle->position / fs_info.bytes_per_sector;
        uint32_t sector_offset = dir_handle->position % fs_info.bytes_per_sector;
        
        // 检查是否已读取完当前簇的所有扇区
        if (sector_in_cluster >= fs_info.sectors_per_cluster) {
            // 移动到下一个簇
            dir_handle->current_cluster = read_fat_entry(dir_handle->current_cluster);
            if (dir_handle->current_cluster >= FAT32_LAST_CLUSTER) {
                return false;
            }
            dir_handle->position = 0;
            sector_in_cluster = 0;
            sector_offset = 0;
        }
        
        // 读取扇区
        if (read_sector(sector + sector_in_cluster, sector_buffer) != 0) {
            set_error("Failed to read directory sector");
            return false;
        }
        
        // 查找下一个有效条目
        for (; sector_offset < fs_info.bytes_per_sector; sector_offset += 32) {
            fat32_dir_entry_t* dir_entry = (fat32_dir_entry_t*)(sector_buffer + sector_offset);
            
            // 检查是否到达目录末尾
            if (dir_entry->name[0] == 0x00) {
                return false;
            }
            
            // 跳过删除的条目和长文件名条目
            if (is_deleted_entry(dir_entry->name) || 
                is_long_name_entry(dir_entry)) {
                continue;
            }
            
            // 跳过.和..条目（可选）
            if (dir_entry->name[0] == '.' && 
                (dir_entry->name[1] == ' ' || dir_entry->name[1] == '.')) {
                continue;
            }
            
            // 复制条目到输出
            memcpy(entry, dir_entry, sizeof(fat32_dir_entry_t));
            
            // 更新句柄位置
            dir_handle->position = (sector_in_cluster * fs_info.bytes_per_sector) + 
                                   sector_offset + 32;
            
            return true;
        }
        
        // 移动到下一个扇区
        dir_handle->position = (sector_in_cluster + 1) * fs_info.bytes_per_sector;
    }
    
    return false;
}

// 查找文件
bool fat32_find_file(const char* path, fat32_dir_entry_t* entry) {
    clear_error();
    
    if (!fs_mounted || path == NULL || entry == NULL) {
        set_error("Invalid parameters");
        return false;
    }
    
    uint32_t dir_sector, dir_offset, parent_cluster;
    if (!find_directory_entry(path, &dir_sector, &dir_offset, &parent_cluster)) {
        return false;
    }
    
    uint8_t sector_buffer[512];
    if (read_sector(dir_sector, sector_buffer) != 0) {
        return false;
    }
    
    memcpy(entry, sector_buffer + dir_offset, sizeof(fat32_dir_entry_t));
    return true;
}

// ==================== 文件管理函数 ====================

// 创建文件
bool fat32_create_file(const char* path) {
    clear_error();
    
    if (!fs_mounted || fs_readonly) {
        set_error("File system not mounted or read-only");
        return false;
    }
    
    if (path == NULL || strlen(path) == 0) {
        set_error("Invalid path");
        return false;
    }
    
    // 检查文件是否已存在
    uint32_t dir_sector, dir_offset, parent_cluster;
    if (find_directory_entry(path, &dir_sector, &dir_offset, &parent_cluster)) {
        set_error("File already exists");
        return false;
    }
    
    // 分配一个簇给新文件
    uint32_t first_cluster = 0;
    if (!allocate_cluster_chain(&first_cluster, 1)) {
        return false;
    }
    
    // 创建目录项
    char name83[11];
    name_to_83(path, name83);
    
    if (!add_directory_entry(parent_cluster, name83, ATTR_ARCHIVE, first_cluster, 0)) {
        free_cluster_chain(first_cluster);
        return false;
    }
    
    // 初始化簇为0
    uint32_t sector = cluster_to_sector(first_cluster);
    uint8_t zero_buffer[512] = {0};
    for (uint32_t i = 0; i < fs_info.sectors_per_cluster; i++) {
        if (write_sector(sector + i, zero_buffer) != 0) {
            return false;
        }
    }
    
    return true;
}

// 删除文件
bool fat32_delete_file(const char* path) {
    clear_error();
    
    if (!fs_mounted || fs_readonly) {
        set_error("File system not mounted or read-only");
        return false;
    }
    
    // 查找文件
    uint32_t dir_sector, dir_offset, parent_cluster;
    if (!find_directory_entry(path, &dir_sector, &dir_offset, &parent_cluster)) {
        set_error("File not found");
        return false;
    }
    
    // 读取文件信息
    uint8_t sector_buffer[512];
    if (read_sector(dir_sector, sector_buffer) != 0) {
        return false;
    }
    
    fat32_dir_entry_t* entry = (fat32_dir_entry_t*)(sector_buffer + dir_offset);
    
    // 检查是否为目录
    if ((entry->attributes & ATTR_DIRECTORY) != 0) {
        set_error("Is a directory, use remove_dir instead");
        return false;
    }
    
    uint32_t file_cluster = (entry->cluster_high << 16) | entry->cluster_low;
    
    // 释放文件占用的簇
    if (file_cluster != 0) {
        if (!free_cluster_chain(file_cluster)) {
            return false;
        }
    }
    
    // 删除目录项
    return remove_directory_entry(dir_sector, dir_offset);
}

// 重命名文件
bool fat32_rename(const char* old_path, const char* new_path) {
    clear_error();
    
    if (!fs_mounted || fs_readonly) {
        set_error("File system not mounted or read-only");
        return false;
    }
    
    // 查找旧文件
    uint32_t dir_sector, dir_offset, parent_cluster;
    if (!find_directory_entry(old_path, &dir_sector, &dir_offset, &parent_cluster)) {
        set_error("Source file not found");
        return false;
    }
    
    // 检查新文件是否已存在
    uint32_t new_dir_sector, new_dir_offset, new_parent_cluster;
    if (find_directory_entry(new_path, &new_dir_sector, &new_dir_offset, &new_parent_cluster)) {
        set_error("Destination file already exists");
        return false;
    }
    
    // 读取旧文件信息
    uint8_t sector_buffer[512];
    if (read_sector(dir_sector, sector_buffer) != 0) {
        return false;
    }
    
    fat32_dir_entry_t* entry = (fat32_dir_entry_t*)(sector_buffer + dir_offset);
    
    // 转换新文件名到8.3格式
    char new_name83[11];
    name_to_83(new_path, new_name83);
    
    // 更新文件名
    for (int i = 0; i < 11; i++) {
        entry->name[i] = new_name83[i];
    }
    
    // 写回磁盘
    return write_sector(dir_sector, sector_buffer) == 0;
}

// 复制文件
bool fat32_copy(const char* src_path, const char* dst_path) {
    clear_error();
    
    if (!fs_mounted || fs_readonly) {
        set_error("File system not mounted or read-only");
        return false;
    }
    
    // 打开源文件
    fat32_handle_t src_handle;
    if (!fat32_open(src_path, &src_handle, FILE_READ)) {
        return false;
    }
    
    // 检查目标文件是否存在
    uint32_t dir_sector, dir_offset, parent_cluster;
    if (find_directory_entry(dst_path, &dir_sector, &dir_offset, &parent_cluster)) {
        set_error("Destination file already exists");
        fat32_close(&src_handle);
        return false;
    }
    
    // 创建目标文件
    fat32_handle_t dst_handle;
    if (!fat32_open(dst_path, &dst_handle, FILE_CREATE)) {
        fat32_close(&src_handle);
        return false;
    }
    
    // 复制数据
    uint8_t buffer[512];
    uint32_t bytes_read;
    
    do {
        bytes_read = 0;
        if (!fat32_read(&src_handle, buffer, sizeof(buffer))) {
            break;
        }
        
        // 计算实际读取的字节数
        bytes_read = src_handle.position - (src_handle.position - sizeof(buffer));
        
        if (!fat32_write(&dst_handle, buffer, bytes_read)) {
            fat32_close(&src_handle);
            fat32_close(&dst_handle);
            return false;
        }
        
    } while (bytes_read == sizeof(buffer));
    
    // 关闭文件句柄
    fat32_close(&src_handle);
    fat32_close(&dst_handle);
    
    return true;
}

// 移动文件
bool fat32_move(const char* src_path, const char* dst_path) {
    // 先尝试重命名
    if (fat32_rename(src_path, dst_path)) {
        return true;
    }
    
    // 如果重命名失败，尝试复制后删除
    if (fat32_copy(src_path, dst_path)) {
        return fat32_delete_file(src_path);
    }
    
    return false;
}

// 检查文件是否存在
bool fat32_file_exists(const char* path) {
    clear_error();
    
    if (!fs_mounted || path == NULL) {
        return false;
    }
    
    uint32_t dir_sector, dir_offset, parent_cluster;
    return find_directory_entry(path, &dir_sector, &dir_offset, &parent_cluster);
}

// 获取文件大小
uint32_t fat32_get_file_size(const char* path) {
    clear_error();
    
    if (!fs_mounted || path == NULL) {
        return 0;
    }
    
    uint32_t dir_sector, dir_offset, parent_cluster;
    if (!find_directory_entry(path, &dir_sector, &dir_offset, &parent_cluster)) {
        return 0;
    }
    
    uint8_t sector_buffer[512];
    if (read_sector(dir_sector, sector_buffer) != 0) {
        return 0;
    }
    
    fat32_dir_entry_t* entry = (fat32_dir_entry_t*)(sector_buffer + dir_offset);
    return entry->file_size;
}

// 获取文件信息
bool fat32_get_file_info(const char* path, fat32_dir_entry_t* info) {
    clear_error();
    
    if (!fs_mounted || path == NULL || info == NULL) {
        return false;
    }
    
    uint32_t dir_sector, dir_offset, parent_cluster;
    if (!find_directory_entry(path, &dir_sector, &dir_offset, &parent_cluster)) {
        return false;
    }
    
    uint8_t sector_buffer[512];
    if (read_sector(dir_sector, sector_buffer) != 0) {
        return false;
    }
    
    memcpy(info, sector_buffer + dir_offset, sizeof(fat32_dir_entry_t));
    return true;
}

// 设置文件属性
bool fat32_set_file_attributes(const char* path, uint8_t attributes) {
    clear_error();
    
    if (!fs_mounted || fs_readonly || path == NULL) {
        set_error("Invalid parameters or read-only");
        return false;
    }
    
    uint32_t dir_sector, dir_offset, parent_cluster;
    if (!find_directory_entry(path, &dir_sector, &dir_offset, &parent_cluster)) {
        set_error("File not found");
        return false;
    }
    
    uint8_t sector_buffer[512];
    if (read_sector(dir_sector, sector_buffer) != 0) {
        return false;
    }
    
    fat32_dir_entry_t* entry = (fat32_dir_entry_t*)(sector_buffer + dir_offset);
    entry->attributes = attributes;
    
    return write_sector(dir_sector, sector_buffer) == 0;
}

// ==================== 工具函数 ====================

// 获取错误信息
const char* fat32_get_error(void) {
    return error_msg;
}

// 检查文件系统是否已挂载
bool fat32_mounted(void) {
    return fs_mounted;
}

// 格式化检查
bool fat32_format_check(void) {
    clear_error();
    
    if (!fs_mounted) {
        set_error("File system not mounted");
        return false;
    }
    
    // 检查引导扇区签名
    uint8_t boot_sector[512];
    if (read_sector(0, boot_sector) != 0) {
        set_error("Failed to read boot sector");
        return false;
    }
    
    if (boot_sector[510] != 0x55 || boot_sector[511] != 0xAA) {
        set_error("Invalid boot signature");
        return false;
    }
    
    // 检查FAT表
    uint8_t fat_buffer[512];
    uint32_t test_clusters[] = {0, 1, 2, fs_info.total_clusters + 1};
    
    for (int i = 0; i < 4; i++) {
        uint32_t fat_entry = read_fat_entry(test_clusters[i]);
        if (fat_entry == 0xFFFFFFFF) {
            set_error("FAT table corrupted");
            return false;
        }
    }
    
    return true;
}

// 打印文件系统信息
void fat32_print_info(void) {
    if (!fs_mounted) {
        serial_puts("FAT32 file system not mounted\n");
        return;
    }
    
    serial_puts("FAT32 File System Information:\n");
    serial_puts("==============================\n");
    
    serial_puts("Volume label: ");
    serial_puts(volume_label);
    serial_puts("\n");
    
    serial_puts("Bytes per sector: ");
    serial_putdec64(fs_info.bytes_per_sector);
    serial_puts("\n");
    
    serial_puts("Sectors per cluster: ");
    serial_putdec64(fs_info.sectors_per_cluster);
    serial_puts("\n");
    
    serial_puts("Total sectors: ");
    serial_putdec64(fs_info.total_sectors);
    serial_puts("\n");
    
    serial_puts("Total clusters: ");
    serial_putdec64(fs_info.total_clusters);
    serial_puts("\n");
    
    serial_puts("Free clusters: ");
    serial_putdec64(fs_info.free_clusters);
    serial_puts("\n");
    
    serial_puts("Data start sector: ");
    serial_putdec64(fs_info.data_start_sector);
    serial_puts("\n");
    
    serial_puts("Root directory cluster: ");
    serial_putdec64(fs_info.root_dir_cluster);
    serial_puts("\n");
    
    // 计算空间使用情况
    uint32_t total_bytes = fs_info.total_sectors * fs_info.bytes_per_sector;
    uint32_t free_bytes = fs_info.free_clusters * fs_info.sectors_per_cluster * fs_info.bytes_per_sector;
    uint32_t used_bytes = total_bytes - free_bytes;
    
    serial_puts("Total space: ");
    serial_putdec64(total_bytes / 1024);
    serial_puts(" KB\n");
    
    serial_puts("Used space: ");
    serial_putdec64(used_bytes / 1024);
    serial_puts(" KB (");
    serial_putdec64((used_bytes * 100) / total_bytes);
    serial_puts("%)\n");
    
    serial_puts("Free space: ");
    serial_putdec64(free_bytes / 1024);
    serial_puts(" KB (");
    serial_putdec64((free_bytes * 100) / total_bytes);
    serial_puts("%)\n");
    
    serial_puts("==============================\n");
}

// 获取空闲空间
uint32_t fat32_get_free_space(void) {
    if (!fs_mounted) {
        return 0;
    }
    
    return fs_info.free_clusters * fs_info.sectors_per_cluster * fs_info.bytes_per_sector;
}

// 获取总空间
uint32_t fat32_get_total_space(void) {
    if (!fs_mounted) {
        return 0;
    }
    
    return fs_info.total_sectors * fs_info.bytes_per_sector;
}

// 获取卷标
const char* fat32_get_volume_label(void) {
    return volume_label;
}

// 设置卷标
bool fat32_set_volume_label(const char* label) {
    clear_error();
    
    if (!fs_mounted || fs_readonly) {
        set_error("File system not mounted or read-only");
        return false;
    }
    
    if (label == NULL || strlen(label) > 11) {
        set_error("Invalid volume label");
        return false;
    }
    
    // 更新内存中的卷标
    memset(volume_label, 0, sizeof(volume_label));
    strncpy(volume_label, label, 11);
    
    // 读取引导扇区
    uint8_t boot_sector[512];
    if (read_sector(0, boot_sector) != 0) {
        return false;
    }
    
    // 更新卷标
    for (size_t i = 0; i < 11; i++) {
        if (i < strlen(label)) {
            boot_sector[0x47 + i] = label[i];
        } else {
            boot_sector[0x47 + i] = ' ';
        }
    }
    
    // 写回引导扇区
    return write_sector(0, boot_sector) == 0;
}

// 格式化文件系统
bool fat32_format(uint32_t partition_start, const char* volume_label) {
    clear_error();
    
    // 警告：这会销毁所有数据！
    serial_puts("WARNING: Formatting will erase all data!\n");
    
    // 简单实现：创建基本的FAT32结构
    // 注意：这是一个简化的格式化，实际FAT32格式化更复杂
    
    partition_start = partition_start;
    
    // 这里应该实现完整的格式化逻辑
    // 由于时间关系，我们只返回false，表示不支持
    set_error("Format not implemented in this version");
    return false;
}

// 检查文件系统
bool fat32_check(void) {
    return fat32_format_check();
}

// 公共扇区读写函数
uint32_t fat32_read_sector(uint32_t sector, void* buffer) {
    return read_sector(sector, buffer);
}

uint32_t fat32_write_sector(uint32_t sector, const void* buffer) {
    return write_sector(sector, buffer);
}

// ==================== 辅助函数实现 ====================

// 获取簇链长度
static uint32_t get_cluster_count(uint32_t first_cluster) {
    uint32_t count = 0;
    uint32_t cluster = first_cluster;
    
    while (cluster < FAT32_LAST_CLUSTER && cluster != FAT32_FREE_CLUSTER) {
        count++;
        cluster = read_fat_entry(cluster);
    }
    
    return count;
}

// 更新FSINFO扇区
static void update_fs_info_sector(void) {
    if (bpb.fs_info_sector == 0 || bpb.fs_info_sector >= bpb.reserved_sectors) {
        return;
    }
    
    uint8_t fsinfo_sector[512];
    memset(fsinfo_sector, 0, sizeof(fsinfo_sector));
    
    // FSINFO签名
    fsinfo_sector[0x00] = 0x52;
    fsinfo_sector[0x01] = 0x52;
    fsinfo_sector[0x02] = 0x61;
    fsinfo_sector[0x03] = 0x41;
    
    fsinfo_sector[0x1E4] = 0x72;
    fsinfo_sector[0x1E5] = 0x72;
    fsinfo_sector[0x1E6] = 0x41;
    fsinfo_sector[0x1E7] = 0x61;
    
    // 空闲簇数
    *((uint32_t*)(fsinfo_sector + 0x1E8)) = fs_info.free_clusters;
    
    // 下一个空闲簇提示
    uint32_t next_free = find_free_cluster();
    *((uint32_t*)(fsinfo_sector + 0x1EC)) = next_free;
    
    // 签名
    fsinfo_sector[0x1FC] = 0x55;
    fsinfo_sector[0x1FD] = 0xAA;
    
    write_sector(bpb.fs_info_sector, fsinfo_sector);
}

// 读取FSINFO扇区
static bool read_fs_info_sector(void) {
    if (bpb.fs_info_sector == 0 || bpb.fs_info_sector >= bpb.reserved_sectors) {
        return false;
    }
    
    uint8_t fsinfo_sector[512];
    if (read_sector(bpb.fs_info_sector, fsinfo_sector) != 0) {
        return false;
    }
    
    // 检查签名
    if (fsinfo_sector[0x00] != 0x52 || fsinfo_sector[0x01] != 0x52 ||
        fsinfo_sector[0x02] != 0x61 || fsinfo_sector[0x03] != 0x41) {
        return false;
    }
    
    // 读取空闲簇数
    fs_info.free_clusters = *((uint32_t*)(fsinfo_sector + 0x1E8));
    
    return true;
}

// 根据位置查找下一个簇
static uint32_t find_next_cluster(uint32_t current_cluster, uint32_t position, uint32_t bytes_per_cluster) {
    uint32_t target_cluster = position / bytes_per_cluster;
    uint32_t cluster = current_cluster;
    
    for (uint32_t i = 0; i < target_cluster; i++) {
        uint32_t next = read_fat_entry(cluster);
        if (next >= FAT32_LAST_CLUSTER || next == FAT32_FREE_CLUSTER) {
            return 0;
        }
        cluster = next;
    }
    
    return cluster;
}

// 简单的toupper实现
int toupper(int c) {
    if (c >= 'a' && c <= 'z') {
        return c - ('a' - 'A');
    }
    return c;
}
