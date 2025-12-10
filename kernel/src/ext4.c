// ext4.c
#include "ext4.h"
#include "storage.h"    // 包含存储设备函数
#include "serial.h"     // 包含串口函数
#include "string.h"     // 包含字符串函数
#include "memory.h"     // 包含内存分配函数
#include <stddef.h>

// 全局文件系统句柄
static struct ext4_fs fs = {0};

// 全局缓冲区
static uint8_t global_block_buffer[65536] __attribute__((aligned(4096)));
static uint8_t global_inode_buffer[4096] __attribute__((aligned(4096)));

// 初始化EXT4文件系统
int ext4_init(uint64_t partition_start) {
    fs.partition_start = partition_start;
    fs.block_buffer = global_block_buffer;
    fs.inode_buffer = global_inode_buffer;
    
    // 读取超级块
    if (!ext4_read_superblock()) {
        serial_puts("EXT4: Failed to read superblock\n");
        return 0;
    }
    
    // 验证魔术数
    if (fs.sb->s_magic != EXT4_SUPER_MAGIC) {
        serial_puts("EXT4: Invalid magic number\n");
        return 0;
    }
    
    // 计算块大小
    fs.block_size = 1024 << fs.sb->s_log_block_size;
    if (fs.block_size > EXT4_MAX_BLOCK_SIZE) {
        serial_puts("EXT4: Invalid block size\n");
        return 0;
    }
    
    // 计算块组数量
    fs.blocks_per_group = fs.sb->s_blocks_per_group;
    fs.inodes_per_group = fs.sb->s_inodes_per_group;
    fs.group_count = (fs.sb->s_blocks_count_lo + fs.blocks_per_group - 1) / fs.blocks_per_group;
    fs.first_data_block = fs.sb->s_first_data_block;
    fs.inode_size = fs.sb->s_inode_size ? fs.sb->s_inode_size : EXT4_INODE_SIZE;
    
    // 计算每个块的描述符数量
    fs.desc_per_block = fs.block_size / sizeof(struct ext4_group_desc);
    
    // 分配和读取块组描述符表
    uint32_t gdt_block = fs.first_data_block + 1;
    uint32_t gdt_size = (fs.group_count * sizeof(struct ext4_group_desc) + fs.block_size - 1) / fs.block_size;
    
    fs.gdt = (struct ext4_group_desc*)malloc(gdt_size * fs.block_size);
    if (!fs.gdt) {
        serial_puts("EXT4: Failed to allocate GDT\n");
        return 0;
    }
    
    // 读取所有GDT块
    for (uint32_t i = 0; i < gdt_size; i++) {
        if (!ext4_read_block(gdt_block + i, (uint8_t*)fs.gdt + i * fs.block_size)) {
            free(fs.gdt);
            serial_puts("EXT4: Failed to read GDT\n");
            return 0;
        }
    }
    
    serial_puts("EXT4: File system initialized successfully\n");
    ext4_print_superblock();
    
    return 1;
}

// 清理文件系统
void ext4_cleanup(void) {
    if (fs.gdt) {
        free(fs.gdt);
        fs.gdt = NULL;
    }
}

// 读取超级块
int ext4_read_superblock(void) {
    // 超级块位于1024字节偏移处
    uint32_t superblock_sector = fs.partition_start + 2; // 1024 / 512 = 2
    
    // 使用通用的存储读取函数
    if (storage_read_sectors(superblock_sector, 1, fs.block_buffer)) {
        fs.sb = (struct ext4_superblock*)fs.block_buffer;
        return 1;
    }
    
    return 0;
}

// 打印超级块信息
void ext4_print_superblock(void) {
    serial_puts("EXT4 Superblock Info:\n");
    
    char buf[64];
    
    // 总块数
    uint64_t total_blocks = fs.sb->s_blocks_count_lo;
    if (fs.sb->s_feature_incompat & 0x80) { // 64位特性
        total_blocks |= ((uint64_t)fs.sb->s_blocks_count_hi << 32);
    }
    
    // 总Inode数
    serial_puts("  Total inodes: ");
    itoa(fs.sb->s_inodes_count, buf, 10);
    serial_puts(buf);
    serial_puts("\n");
    
    serial_puts("  Total blocks: ");
    ulltoa(total_blocks, buf, 10);
    serial_puts(buf);
    serial_puts("\n");
    
    serial_puts("  Free blocks: ");
    uint64_t free_blocks = fs.sb->s_free_blocks_count_lo;
    if (fs.sb->s_feature_incompat & 0x80) {
        free_blocks |= ((uint64_t)fs.sb->s_free_blocks_count_hi << 32);
    }
    ulltoa(free_blocks, buf, 10);
    serial_puts(buf);
    serial_puts("\n");
    
    serial_puts("  Free inodes: ");
    itoa(fs.sb->s_free_inodes_count, buf, 10);
    serial_puts(buf);
    serial_puts("\n");
    
    serial_puts("  Block size: ");
    itoa(fs.block_size, buf, 10);
    serial_puts(buf);
    serial_puts("\n");
    
    serial_puts("  Blocks per group: ");
    itoa(fs.blocks_per_group, buf, 10);
    serial_puts(buf);
    serial_puts("\n");
    
    serial_puts("  Inodes per group: ");
    itoa(fs.inodes_per_group, buf, 10);
    serial_puts(buf);
    serial_puts("\n");
    
    serial_puts("  Inode size: ");
    itoa(fs.inode_size, buf, 10);
    serial_puts(buf);
    serial_puts("\n");
    
    serial_puts("  Volume name: ");
    for (int i = 0; i < 16 && fs.sb->s_volume_name[i]; i++) {
        serial_putc(fs.sb->s_volume_name[i]);
    }
    serial_puts("\n");
}

// 读取块
int ext4_read_block(uint32_t block_no, void* buffer) {
    uint32_t sector = ext4_block_to_sector(block_no);
    
    // 使用通用的存储读取函数
    uint32_t sectors_per_block = fs.block_size / 512;
    return storage_read_sectors(sector, sectors_per_block, buffer);
}

// 写入块
int ext4_write_block(uint32_t block_no, const void* buffer) {
    uint32_t sector = ext4_block_to_sector(block_no);
    
    // 使用通用的存储写入函数
    uint32_t sectors_per_block = fs.block_size / 512;
    return storage_write_sectors(sector, sectors_per_block, buffer);
}

// 块号转扇区号
uint32_t ext4_block_to_sector(uint32_t block_no) {
    return fs.partition_start + block_no * (fs.block_size / 512);
}

// 扇区号转块号
uint32_t ext4_sector_to_block(uint32_t sector) {
    return (sector - fs.partition_start) / (fs.block_size / 512);
}

// 获取块大小
uint32_t ext4_get_block_size(void) {
    return fs.block_size;
}

// 获取Inode所在的块组
uint32_t ext4_get_inode_group(uint32_t inode_no) {
    return (inode_no - 1) / fs.inodes_per_group;
}

// 获取Inode在块组内的索引
uint32_t ext4_get_inode_index(uint32_t inode_no) {
    return (inode_no - 1) % fs.inodes_per_group;
}

// 读取Inode
int ext4_read_inode(uint32_t inode_no, struct ext4_inode* inode) {
    if (inode_no == 0 || inode_no > fs.sb->s_inodes_count) {
        return 0;
    }
    
    uint32_t group = ext4_get_inode_group(inode_no);
    uint32_t index = ext4_get_inode_index(inode_no);
    
    if (group >= fs.group_count) {
        return 0;
    }
    
    // 获取Inode表块
    uint64_t inode_table_block = fs.gdt[group].bg_inode_table_lo;
    if (fs.sb->s_feature_incompat & 0x80) {
        inode_table_block |= ((uint64_t)fs.gdt[group].bg_inode_table_hi << 32);
    }
    
    // 计算Inode在表中的位置
    uint32_t inodes_per_block = fs.block_size / fs.inode_size;
    uint32_t block_offset = index / inodes_per_block;
    uint32_t inode_offset = index % inodes_per_block;
    
    // 读取包含Inode的块
    if (!ext4_read_block(inode_table_block + block_offset, fs.block_buffer)) {
        return 0;
    }
    
    // 复制Inode
    memcpy(inode, fs.block_buffer + inode_offset * fs.inode_size, sizeof(struct ext4_inode));
    return 1;
}

// 写入Inode
int ext4_write_inode(uint32_t inode_no, struct ext4_inode* inode) {
    if (inode_no == 0 || inode_no > fs.sb->s_inodes_count) {
        return 0;
    }
    
    uint32_t group = ext4_get_inode_group(inode_no);
    uint32_t index = ext4_get_inode_index(inode_no);
    
    if (group >= fs.group_count) {
        return 0;
    }
    
    // 获取Inode表块
    uint64_t inode_table_block = fs.gdt[group].bg_inode_table_lo;
    if (fs.sb->s_feature_incompat & 0x80) {
        inode_table_block |= ((uint64_t)fs.gdt[group].bg_inode_table_hi << 32);
    }
    
    // 计算Inode在表中的位置
    uint32_t inodes_per_block = fs.block_size / fs.inode_size;
    uint32_t block_offset = index / inodes_per_block;
    uint32_t inode_offset = index % inodes_per_block;
    
    // 读取包含Inode的块
    if (!ext4_read_block(inode_table_block + block_offset, fs.block_buffer)) {
        return 0;
    }
    
    // 更新Inode
    memcpy(fs.block_buffer + inode_offset * fs.inode_size, inode, sizeof(struct ext4_inode));
    
    // 写回块
    return ext4_write_block(inode_table_block + block_offset, fs.block_buffer);
}

// 读取块位图
int ext4_read_block_bitmap(uint32_t group, uint8_t* bitmap) {
    if (group >= fs.group_count) {
        return 0;
    }
    
    uint64_t bitmap_block = fs.gdt[group].bg_block_bitmap_lo;
    if (fs.sb->s_feature_incompat & 0x80) {
        bitmap_block |= ((uint64_t)fs.gdt[group].bg_block_bitmap_hi << 32);
    }
    
    return ext4_read_block(bitmap_block, bitmap);
}

// 读取Inode位图
int ext4_read_inode_bitmap(uint32_t group, uint8_t* bitmap) {
    if (group >= fs.group_count) {
        return 0;
    }
    
    uint64_t bitmap_block = fs.gdt[group].bg_inode_bitmap_lo;
    if (fs.sb->s_feature_incompat & 0x80) {
        bitmap_block |= ((uint64_t)fs.gdt[group].bg_inode_bitmap_hi << 32);
    }
    
    return ext4_read_block(bitmap_block, bitmap);
}

// 查找空闲块
int ext4_find_free_block(uint32_t group, uint32_t* block_no) {
    uint8_t bitmap[fs.block_size];
    
    if (!ext4_read_block_bitmap(group, bitmap)) {
        return 0;
    }
    
    // 查找第一个空闲位
    uint32_t blocks_in_group = fs.blocks_per_group;
    if (group == fs.group_count - 1) {
        blocks_in_group = fs.sb->s_blocks_count_lo - group * fs.blocks_per_group;
    }
    
    // 跳过前几个块（超级块、GDT等）
    uint32_t start_block = (group == 0) ? fs.first_data_block : 0;
    
    for (uint32_t i = start_block; i < blocks_in_group; i++) {
        uint32_t byte = i / 8;
        uint32_t bit = i % 8;
        
        if (!(bitmap[byte] & (1 << bit))) {
            *block_no = group * fs.blocks_per_group + i;
            return 1;
        }
    }
    
    return 0;
}

// 查找空闲Inode
int ext4_find_free_inode(uint32_t group, uint32_t* inode_no) {
    uint8_t bitmap[fs.block_size];
    
    if (!ext4_read_inode_bitmap(group, bitmap)) {
        return 0;
    }
    
    // 查找第一个空闲位（跳过Inode 0）
    for (uint32_t i = 1; i < fs.inodes_per_group; i++) {
        uint32_t byte = i / 8;
        uint32_t bit = i % 8;
        
        if (!(bitmap[byte] & (1 << bit))) {
            *inode_no = group * fs.inodes_per_group + i + 1;
            return 1;
        }
    }
    
    return 0;
}

// 设置块使用状态
int ext4_set_block_used(uint32_t block_no, int used) {
    uint32_t group = block_no / fs.blocks_per_group;
    uint32_t index = block_no % fs.blocks_per_group;
    
    uint8_t bitmap[fs.block_size];
    if (!ext4_read_block_bitmap(group, bitmap)) {
        return 0;
    }
    
    uint32_t byte = index / 8;
    uint32_t bit = index % 8;
    
    if (used) {
        bitmap[byte] |= (1 << bit);
    } else {
        bitmap[byte] &= ~(1 << bit);
    }
    
    // 写入块位图
    uint64_t bitmap_block = fs.gdt[group].bg_block_bitmap_lo;
    if (fs.sb->s_feature_incompat & 0x80) {
        bitmap_block |= ((uint64_t)fs.gdt[group].bg_block_bitmap_hi << 32);
    }
    
    return ext4_write_block(bitmap_block, bitmap);
}

// 设置Inode使用状态
int ext4_set_inode_used(uint32_t inode_no, int used) {
    uint32_t group = ext4_get_inode_group(inode_no);
    uint32_t index = ext4_get_inode_index(inode_no);
    
    uint8_t bitmap[fs.block_size];
    if (!ext4_read_inode_bitmap(group, bitmap)) {
        return 0;
    }
    
    uint32_t byte = index / 8;
    uint32_t bit = index % 8;
    
    if (used) {
        bitmap[byte] |= (1 << bit);
    } else {
        bitmap[byte] &= ~(1 << bit);
    }
    
    // 写入Inode位图
    uint64_t bitmap_block = fs.gdt[group].bg_inode_bitmap_lo;
    if (fs.sb->s_feature_incompat & 0x80) {
        bitmap_block |= ((uint64_t)fs.gdt[group].bg_inode_bitmap_hi << 32);
    }
    
    return ext4_write_block(bitmap_block, bitmap);
}

// 分配块
int ext4_alloc_block(uint32_t* block_no) {
    // 尝试在每个块组中查找空闲块
    for (uint32_t group = 0; group < fs.group_count; group++) {
        if (ext4_find_free_block(group, block_no)) {
            if (ext4_set_block_used(*block_no, 1)) {
                // 更新超级块中的空闲块计数
                fs.sb->s_free_blocks_count_lo--;
                if (fs.sb->s_feature_incompat & 0x80) {
                    fs.sb->s_free_blocks_count_hi--;
                }
                
                // 更新块组描述符
                fs.gdt[group].bg_free_blocks_count_lo--;
                if (fs.sb->s_feature_incompat & 0x80) {
                    fs.gdt[group].bg_free_blocks_count_hi--;
                }
                
                // 写回超级块
                uint32_t superblock_block = fs.first_data_block;
                if (fs.block_size > 1024) {
                    superblock_block = 0;
                }
                ext4_write_block(superblock_block, fs.sb);
                
                // 写回GDT
                uint32_t gdt_block = fs.first_data_block + 1;
                uint32_t gdt_size = (fs.group_count * sizeof(struct ext4_group_desc) + fs.block_size - 1) / fs.block_size;
                for (uint32_t i = 0; i < gdt_size; i++) {
                    ext4_write_block(gdt_block + i, (uint8_t*)fs.gdt + i * fs.block_size);
                }
                
                return 1;
            }
        }
    }
    
    return 0;
}

// 释放块
int ext4_free_block(uint32_t block_no) {
    if (ext4_set_block_used(block_no, 0)) {
        // 更新超级块中的空闲块计数
        fs.sb->s_free_blocks_count_lo++;
        if (fs.sb->s_feature_incompat & 0x80) {
            fs.sb->s_free_blocks_count_hi++;
        }
        
        // 更新块组描述符
        uint32_t group = block_no / fs.blocks_per_group;
        fs.gdt[group].bg_free_blocks_count_lo++;
        if (fs.sb->s_feature_incompat & 0x80) {
            fs.gdt[group].bg_free_blocks_count_hi++;
        }
        
        // 写回超级块和GDT
        uint32_t superblock_block = fs.first_data_block;
        if (fs.block_size > 1024) {
            superblock_block = 0;
        }
        ext4_write_block(superblock_block, fs.sb);
        
        uint32_t gdt_block = fs.first_data_block + 1;
        uint32_t gdt_size = (fs.group_count * sizeof(struct ext4_group_desc) + fs.block_size - 1) / fs.block_size;
        for (uint32_t i = 0; i < gdt_size; i++) {
            ext4_write_block(gdt_block + i, (uint8_t*)fs.gdt + i * fs.block_size);
        }
        
        return 1;
    }
    
    return 0;
}

// 分配Inode
int ext4_alloc_inode(uint32_t* inode_no) {
    // 尝试在每个块组中查找空闲Inode
    for (uint32_t group = 0; group < fs.group_count; group++) {
        if (ext4_find_free_inode(group, inode_no)) {
            if (ext4_set_inode_used(*inode_no, 1)) {
                // 更新超级块中的空闲Inode计数
                fs.sb->s_free_inodes_count--;
                
                // 更新块组描述符
                fs.gdt[group].bg_free_inodes_count_lo--;
                if (fs.sb->s_feature_incompat & 0x80) {
                    fs.gdt[group].bg_free_inodes_count_hi--;
                }
                
                // 写回超级块和GDT
                uint32_t superblock_block = fs.first_data_block;
                if (fs.block_size > 1024) {
                    superblock_block = 0;
                }
                ext4_write_block(superblock_block, fs.sb);
                
                uint32_t gdt_block = fs.first_data_block + 1;
                uint32_t gdt_size = (fs.group_count * sizeof(struct ext4_group_desc) + fs.block_size - 1) / fs.block_size;
                for (uint32_t i = 0; i < gdt_size; i++) {
                    ext4_write_block(gdt_block + i, (uint8_t*)fs.gdt + i * fs.block_size);
                }
                
                return 1;
            }
        }
    }
    
    return 0;
}

// 释放Inode
int ext4_free_inode(uint32_t inode_no) {
    if (ext4_set_inode_used(inode_no, 0)) {
        // 更新超级块中的空闲Inode计数
        fs.sb->s_free_inodes_count++;
        
        // 更新块组描述符
        uint32_t group = ext4_get_inode_group(inode_no);
        fs.gdt[group].bg_free_inodes_count_lo++;
        if (fs.sb->s_feature_incompat & 0x80) {
            fs.gdt[group].bg_free_inodes_count_hi++;
        }
        
        // 写回超级块和GDT
        uint32_t superblock_block = fs.first_data_block;
        if (fs.block_size > 1024) {
            superblock_block = 0;
        }
        ext4_write_block(superblock_block, fs.sb);
        
        uint32_t gdt_block = fs.first_data_block + 1;
        uint32_t gdt_size = (fs.group_count * sizeof(struct ext4_group_desc) + fs.block_size - 1) / fs.block_size;
        for (uint32_t i = 0; i < gdt_size; i++) {
            ext4_write_block(gdt_block + i, (uint8_t*)fs.gdt + i * fs.block_size);
        }
        
        return 1;
    }
    
    return 0;
}

// 读取Extent数据块
int ext4_extent_read(struct ext4_inode* inode, uint32_t block_offset, uint32_t* phys_block) {
    // 检查是否使用extent
    if (!(inode->i_flags & EXT4_EXTENTS_FL)) {
        // 使用传统的间接块映射
        if (block_offset < 12) {
            *phys_block = inode->i_block[block_offset];
            return (*phys_block != 0);
        }
        // 简化：不支持间接块
        return 0;
    }
    
    // 简化：只处理直接extent
    struct ext4_extent_header* eh = (struct ext4_extent_header*)inode->i_block;
    
    if (eh->eh_magic != 0xF30A) {
        return 0;
    }
    
    // 只处理叶节点
    if (eh->eh_depth != 0) {
        return 0;
    }
    
    struct ext4_extent* extents = (struct ext4_extent*)(eh + 1);
    
    for (int i = 0; i < eh->eh_entries; i++) {
        if (block_offset >= extents[i].ee_block && 
            block_offset < extents[i].ee_block + extents[i].ee_len) {
            *phys_block = extents[i].ee_start_lo + (block_offset - extents[i].ee_block);
            return 1;
        }
    }
    
    return 0;
}

// 搜索Extent树
int ext4_extent_search(struct ext4_extent_header* eh, uint32_t block_offset, 
                       uint32_t* phys_block, int depth) {
    // 简化实现
    if (eh->eh_depth == depth) {
        // 叶节点，包含extent
        struct ext4_extent* extents = (struct ext4_extent*)(eh + 1);
        
        for (int i = 0; i < eh->eh_entries; i++) {
            if (block_offset >= extents[i].ee_block && 
                block_offset < extents[i].ee_block + extents[i].ee_len) {
                *phys_block = extents[i].ee_start_lo + (block_offset - extents[i].ee_block);
                return 1;
            }
        }
    }
    
    return 0;
}

// 读取文件（修复返回类型）
size_t ext4_read(struct ext4_file* file, void* buffer, size_t size) {
    if (!file || !buffer || file->is_dir) {
        return 0;
    }
    
    if (file->position >= file->size) {
        return 0;
    }
    
    size_t bytes_to_read = size;
    if (file->position + bytes_to_read > file->size) {
        bytes_to_read = file->size - file->position;
    }
    
    size_t bytes_read = 0;
    uint8_t* buf = (uint8_t*)buffer;
    
    while (bytes_read < bytes_to_read) {
        uint32_t block_offset = (file->position + bytes_read) / fs.block_size;
        uint32_t offset_in_block = (file->position + bytes_read) % fs.block_size;
        
        uint32_t phys_block;
        if (!ext4_extent_read(file->inode, block_offset, &phys_block)) {
            break;
        }
        
        // 读取块
        if (!ext4_read_block(phys_block, fs.block_buffer)) {
            break;
        }
        
        size_t bytes_in_block = fs.block_size - offset_in_block;
        if (bytes_in_block > bytes_to_read - bytes_read) {
            bytes_in_block = bytes_to_read - bytes_read;
        }
        
        memcpy(buf + bytes_read, fs.block_buffer + offset_in_block, bytes_in_block);
        bytes_read += bytes_in_block;
    }
    
    file->position += bytes_read;
    return bytes_read;
}

// 写入Extent数据块
int ext4_extent_write(struct ext4_inode* inode, uint32_t block_offset, uint32_t phys_block) {
    // 简化实现：使用直接块映射
    if (block_offset < 12) {
        inode->i_block[block_offset] = phys_block;
        return 1;
    }
    
    // 对于更大的文件，需要实现完整的extent树
    return 0;
}

// 分配Extent块
int ext4_extent_alloc(struct ext4_inode* inode, uint32_t block_offset, uint32_t* phys_block) {
    // 分配新块
    if (!ext4_alloc_block(phys_block)) {
        return 0;
    }
    
    // 写入extent树
    return ext4_extent_write(inode, block_offset, *phys_block);
}

// 写入文件（修复返回类型）
size_t ext4_write(struct ext4_file* file, const void* buffer, size_t size) {
    if (!file || !buffer || file->is_dir) {
        return 0;
    }
    
    size_t bytes_written = 0;
    const uint8_t* buf = (const uint8_t*)buffer;
    
    while (bytes_written < size) {
        uint32_t block_offset = (file->position + bytes_written) / fs.block_size;
        uint32_t offset_in_block = (file->position + bytes_written) % fs.block_size;
        
        // 获取物理块
        uint32_t phys_block;
        if (!ext4_extent_read(file->inode, block_offset, &phys_block)) {
            // 需要分配新块
            if (!ext4_extent_alloc(file->inode, block_offset, &phys_block)) {
                break;
            }
        }
        
        // 读取现有块（如果部分写入）
        if (offset_in_block > 0 || (bytes_written + fs.block_size - offset_in_block) < size) {
            if (!ext4_read_block(phys_block, fs.block_buffer)) {
                memset(fs.block_buffer, 0, fs.block_size);
            }
        } else {
            memset(fs.block_buffer, 0, fs.block_size);
        }
        
        size_t bytes_in_block = fs.block_size - offset_in_block;
        if (bytes_in_block > size - bytes_written) {
            bytes_in_block = size - bytes_written;
        }
        
        memcpy(fs.block_buffer + offset_in_block, buf + bytes_written, bytes_in_block);
        
        // 写入块
        if (!ext4_write_block(phys_block, fs.block_buffer)) {
            break;
        }
        
        bytes_written += bytes_in_block;
    }
    
    file->position += bytes_written;
    if (file->position > file->size) {
        file->size = file->position;
        file->inode->i_size_lo = file->size;
    }
    
    return bytes_written;
}

// 定位文件指针
int ext4_seek(struct ext4_file* file, uint32_t offset) {
    if (!file) {
        return 0;
    }
    
    if (offset > file->size) {
        offset = file->size;
    }
    
    file->position = offset;
    return 1;
}

// 获取文件指针位置
uint32_t ext4_tell(struct ext4_file* file) {
    return file ? file->position : 0;
}

// 截断文件
int ext4_truncate(struct ext4_file* file, uint32_t size) {
    if (!file || file->is_dir) {
        return 0;
    }
    
    if (size >= file->size) {
        return 1; // 无需截断
    }
    
    // 计算需要释放的块
    uint32_t old_blocks = (file->size + fs.block_size - 1) / fs.block_size;
    uint32_t new_blocks = (size + fs.block_size - 1) / fs.block_size;
    
    // 释放多余的块
    for (uint32_t i = new_blocks; i < old_blocks; i++) {
        uint32_t phys_block;
        if (ext4_extent_read(file->inode, i, &phys_block)) {
            ext4_free_block(phys_block);
            ext4_extent_write(file->inode, i, 0);
        }
    }
    
    file->size = size;
    file->inode->i_size_lo = size;
    
    if (file->position > size) {
        file->position = size;
    }
    
    // 更新Inode
    return ext4_write_inode(file->inode_no, file->inode);
}

// 测试EXT4文件系统
void ext4_test(void) {
    serial_puts("\n=== EXT4 File System Test ===\n");
    
    // 假设分区从LBA 2048开始（典型的MBR分区）
    if (!ext4_init(2048)) {
        serial_puts("EXT4: Initialization failed\n");
        return;
    }
    
    // 创建测试文件
    serial_puts("Creating test file...\n");
    if (ext4_create("/test.txt", 0644)) {
        serial_puts("File created successfully\n");
        
        // 打开文件
        struct ext4_file* file = ext4_open("/test.txt");
        if (file) {
            // 写入数据
            const char* data = "Hello, EXT4 File System from Limine Kernel!\n";
            size_t written = ext4_write(file, data, strlen(data));
            
            char buf[32];
            itoa(written, buf, 10);
            serial_puts("Written ");
            serial_puts(buf);
            serial_puts(" bytes\n");
            
            // 回到文件开头
            ext4_seek(file, 0);
            
            // 读取数据
            char read_buffer[256];
            size_t read = ext4_read(file, read_buffer, sizeof(read_buffer) - 1);
            if (read > 0) {
                read_buffer[read] = '\0';
                serial_puts("Read data: ");
                serial_puts(read_buffer);
            }
            
            // 关闭文件
            ext4_close(file);
            
            // 验证文件内容
            file = ext4_open("/test.txt");
            if (file) {
                char verify_buffer[256];
                ext4_read(file, verify_buffer, sizeof(verify_buffer) - 1);
                verify_buffer[strlen(data)] = '\0';
                
                if (strcmp(verify_buffer, data) == 0) {
                    serial_puts("\nFile content verified successfully!\n");
                } else {
                    serial_puts("\nFile content verification failed!\n");
                }
                
                ext4_close(file);
            }
        }
        
        // 删除测试文件
        serial_puts("\nDeleting test file...\n");
        if (ext4_unlink("/test.txt")) {
            serial_puts("File deleted successfully\n");
        }
    } else {
        serial_puts("Failed to create test file\n");
    }
    
    ext4_cleanup();
    serial_puts("\n=== EXT4 Test Complete ===\n");
}