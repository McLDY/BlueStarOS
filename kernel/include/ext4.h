// ext4.h
#ifndef EXT4_H
#define EXT4_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

// 定义 ssize_t，因为freestanding环境可能没有
#ifndef _SSIZE_T
#define _SSIZE_T
typedef long ssize_t;
#endif

// EXT4魔术数
#define EXT4_SUPER_MAGIC 0xEF53

// 块大小常量
#define EXT4_MIN_BLOCK_SIZE 1024
#define EXT4_MAX_BLOCK_SIZE 65536

// Inode相关常量
#define EXT4_N_BLOCKS 15
#define EXT4_INODE_SIZE 256

// Inode标志
#define EXT4_SECRM_FL      0x00000001
#define EXT4_UNRM_FL       0x00000002
#define EXT4_COMPR_FL      0x00000004
#define EXT4_SYNC_FL       0x00000008
#define EXT4_IMMUTABLE_FL  0x00000010
#define EXT4_APPEND_FL     0x00000020
#define EXT4_NODUMP_FL     0x00000040
#define EXT4_NOATIME_FL    0x00000080
#define EXT4_DIRTY_FL      0x00000100
#define EXT4_COMPRBLK_FL   0x00000200
#define EXT4_NOCOMPR_FL    0x00000400
#define EXT4_ENCrypt_FL    0x00000800
#define EXT4_INDEX_FL      0x00001000
#define EXT4_IMAGIC_FL     0x00002000
#define EXT4_JOURNAL_DATA_FL 0x00004000
#define EXT4_NOTAIL_FL     0x00008000
#define EXT4_DIRSYNC_FL    0x00010000
#define EXT4_TOPDIR_FL     0x00020000
#define EXT4_HUGE_FILE_FL  0x00040000
#define EXT4_EXTENTS_FL    0x00080000
#define EXT4_EA_INODE_FL   0x00200000
#define EXT4_EOFBLOCKS_FL  0x00400000
#define EXT4_SNAPFILE_FL   0x01000000
#define EXT4_SNAPFILE_DELETED_FL 0x04000000
#define EXT4_SNAPFILE_SHRUNK_FL  0x08000000
#define EXT4_INLINE_DATA_FL 0x10000000
#define EXT4_PROJINHERIT_FL 0x20000000
#define EXT4_RESERVED_FL    0x80000000

// Inode模式
#define EXT4_S_IFMT   0xF000
#define EXT4_S_IFSOCK 0xC000
#define EXT4_S_IFLNK  0xA000
#define EXT4_S_IFREG  0x8000
#define EXT4_S_IFBLK  0x6000
#define EXT4_S_IFDIR  0x4000
#define EXT4_S_IFCHR  0x2000
#define EXT4_S_IFIFO  0x1000
#define EXT4_S_ISUID  0x0800
#define EXT4_S_ISGID  0x0400
#define EXT4_S_ISVTX  0x0200
#define EXT4_S_IRWXU  0x01C0
#define EXT4_S_IRUSR  0x0100
#define EXT4_S_IWUSR  0x0080
#define EXT4_S_IXUSR  0x0040
#define EXT4_S_IRWXG  0x0038
#define EXT4_S_IRGRP  0x0020
#define EXT4_S_IWGRP  0x0010
#define EXT4_S_IXGRP  0x0008
#define EXT4_S_IRWXO  0x0007
#define EXT4_S_IROTH  0x0004
#define EXT4_S_IWOTH  0x0002
#define EXT4_S_IXOTH  0x0001

// 目录项类型
#define EXT4_FT_UNKNOWN  0
#define EXT4_FT_REG_FILE 1
#define EXT4_FT_DIR      2
#define EXT4_FT_CHRDEV   3
#define EXT4_FT_BLKDEV   4
#define EXT4_FT_FIFO     5
#define EXT4_FT_SOCK     6
#define EXT4_FT_SYMLINK  7

// 超级块结构
struct ext4_superblock {
    uint32_t s_inodes_count;
    uint32_t s_blocks_count_lo;
    uint32_t s_r_blocks_count_lo;
    uint32_t s_free_blocks_count_lo;
    uint32_t s_free_inodes_count;
    uint32_t s_first_data_block;
    uint32_t s_log_block_size;
    uint32_t s_log_cluster_size;
    uint32_t s_blocks_per_group;
    uint32_t s_clusters_per_group;
    uint32_t s_inodes_per_group;
    uint32_t s_mtime;
    uint32_t s_wtime;
    uint16_t s_mnt_count;
    uint16_t s_max_mnt_count;
    uint16_t s_magic;
    uint16_t s_state;
    uint16_t s_errors;
    uint16_t s_minor_rev_level;
    uint32_t s_lastcheck;
    uint32_t s_checkinterval;
    uint32_t s_creator_os;
    uint32_t s_rev_level;
    uint16_t s_def_resuid;
    uint16_t s_def_resgid;
    uint32_t s_first_ino;
    uint16_t s_inode_size;
    uint16_t s_block_group_nr;
    uint32_t s_feature_compat;
    uint32_t s_feature_incompat;
    uint32_t s_feature_ro_compat;
    uint8_t  s_uuid[16];
    char     s_volume_name[16];
    char     s_last_mounted[64];
    uint32_t s_algorithm_usage_bitmap;
    uint8_t  s_prealloc_blocks;
    uint8_t  s_prealloc_dir_blocks;
    uint16_t s_reserved_gdt_blocks;
    uint8_t  s_journal_uuid[16];
    uint32_t s_journal_inum;
    uint32_t s_journal_dev;
    uint32_t s_last_orphan;
    uint32_t s_hash_seed[4];
    uint8_t  s_def_hash_version;
    uint8_t  s_jnl_backup_type;
    uint16_t s_desc_size;
    uint32_t s_default_mount_opts;
    uint32_t s_first_meta_bg;
    uint32_t s_mkfs_time;
    uint32_t s_jnl_blocks[17];
    uint32_t s_blocks_count_hi;
    uint32_t s_r_blocks_count_hi;
    uint32_t s_free_blocks_count_hi;
    uint16_t s_min_extra_isize;
    uint16_t s_want_extra_isize;
    uint32_t s_flags;
    uint16_t s_raid_stride;
    uint16_t s_mmp_interval;
    uint64_t s_mmp_block;
    uint32_t s_raid_stripe_width;
    uint8_t  s_log_groups_per_flex;
    uint8_t  s_checksum_type;
    uint16_t s_reserved_pad;
    uint64_t s_kbytes_written;
    uint32_t s_snapshot_inum;
    uint32_t s_snapshot_id;
    uint64_t s_snapshot_r_blocks_count;
    uint32_t s_snapshot_list;
    uint32_t s_error_count;
    uint32_t s_first_error_time;
    uint32_t s_first_error_ino;
    uint64_t s_first_error_block;
    uint8_t  s_first_error_func[32];
    uint32_t s_first_error_line;
    uint32_t s_last_error_time;
    uint32_t s_last_error_ino;
    uint32_t s_last_error_line;
    uint64_t s_last_error_block;
    uint8_t  s_last_error_func[32];
    uint8_t  s_mount_opts[64];
    uint32_t s_usr_quota_inum;
    uint32_t s_grp_quota_inum;
    uint32_t s_overhead_blocks;
    uint32_t s_backup_bgs[2];
    uint8_t  s_encrypt_algos[4];
    uint8_t  s_encrypt_pw_salt[16];
    uint32_t s_lpf_ino;
    uint32_t s_prj_quota_inum;
    uint32_t s_checksum_seed;
    uint32_t s_reserved[98];
    uint32_t s_checksum;
} __attribute__((packed));

// 块组描述符
struct ext4_group_desc {
    uint32_t bg_block_bitmap_lo;
    uint32_t bg_inode_bitmap_lo;
    uint32_t bg_inode_table_lo;
    uint16_t bg_free_blocks_count_lo;
    uint16_t bg_free_inodes_count_lo;
    uint16_t bg_used_dirs_count_lo;
    uint16_t bg_flags;
    uint32_t bg_exclude_bitmap_lo;
    uint16_t bg_block_bitmap_csum_lo;
    uint16_t bg_inode_bitmap_csum_lo;
    uint16_t bg_itable_unused_lo;
    uint16_t bg_checksum;
    uint32_t bg_block_bitmap_hi;
    uint32_t bg_inode_bitmap_hi;
    uint32_t bg_inode_table_hi;
    uint16_t bg_free_blocks_count_hi;
    uint16_t bg_free_inodes_count_hi;
    uint16_t bg_used_dirs_count_hi;
    uint16_t bg_itable_unused_hi;
    uint32_t bg_exclude_bitmap_hi;
    uint16_t bg_block_bitmap_csum_hi;
    uint16_t bg_inode_bitmap_csum_hi;
    uint32_t bg_reserved;
} __attribute__((packed));

// Inode结构
struct ext4_inode {
    uint16_t i_mode;
    uint16_t i_uid;
    uint32_t i_size_lo;
    uint32_t i_atime;
    uint32_t i_ctime;
    uint32_t i_mtime;
    uint32_t i_dtime;
    uint16_t i_gid;
    uint16_t i_links_count;
    uint32_t i_blocks_lo;
    uint32_t i_flags;
    uint32_t i_osd1;
    uint32_t i_block[EXT4_N_BLOCKS];
    uint32_t i_generation;
    uint32_t i_file_acl_lo;
    uint32_t i_size_high;
    uint32_t i_obso_faddr;
    uint16_t i_blocks_high;
    uint16_t i_file_acl_high;
    uint16_t i_uid_high;
    uint16_t i_gid_high;
    uint32_t i_author;
} __attribute__((packed));

// 目录项结构
struct ext4_dir_entry {
    uint32_t inode;
    uint16_t rec_len;
    uint8_t  name_len;
    uint8_t  file_type;
    char     name[];
} __attribute__((packed));

// 目录项2（新版本）
struct ext4_dir_entry_2 {
    uint32_t inode;
    uint16_t rec_len;
    uint8_t  name_len;
    uint8_t  file_type;
    char     name[];
} __attribute__((packed));

// Extent头
struct ext4_extent_header {
    uint16_t eh_magic;
    uint16_t eh_entries;
    uint16_t eh_max;
    uint16_t eh_depth;
    uint32_t eh_generation;
} __attribute__((packed));

// Extent索引节点
struct ext4_extent_idx {
    uint32_t ei_block;
    uint32_t ei_leaf_lo;
    uint16_t ei_leaf_hi;
    uint16_t ei_unused;
} __attribute__((packed));

// Extent节点
struct ext4_extent {
    uint32_t ee_block;
    uint16_t ee_len;
    uint16_t ee_start_hi;
    uint32_t ee_start_lo;
} __attribute__((packed));

// 文件系统句柄
struct ext4_fs {
    struct ext4_superblock* sb;
    struct ext4_group_desc* gdt;
    uint32_t block_size;
    uint32_t blocks_per_group;
    uint32_t inodes_per_group;
    uint32_t group_count;
    uint32_t first_data_block;
    uint32_t inode_size;
    uint32_t desc_per_block;
    uint32_t bg_desc_reserve_blocks;
    uint64_t partition_start;
    uint8_t* block_buffer;
    uint8_t* inode_buffer;
};

// 文件句柄
struct ext4_file {
    struct ext4_inode* inode;
    uint32_t inode_no;
    uint32_t position;
    uint32_t size;
    uint16_t mode;
    bool is_dir;
    struct ext4_fs* fs;
};

// 目录遍历句柄
struct ext4_dir {
    struct ext4_file file;
    uint32_t offset;
    uint8_t* buffer;
};

// 函数声明
// 初始化
int ext4_init(uint64_t partition_start);
void ext4_cleanup(void);

// 超级块操作
int ext4_read_superblock(void);
void ext4_print_superblock(void);

// Inode操作
int ext4_read_inode(uint32_t inode_no, struct ext4_inode* inode);
int ext4_write_inode(uint32_t inode_no, struct ext4_inode* inode);
int ext4_alloc_inode(uint32_t* inode_no);
int ext4_free_inode(uint32_t inode_no);

// 块操作
int ext4_read_block(uint32_t block_no, void* buffer);
int ext4_write_block(uint32_t block_no, const void* buffer);
int ext4_alloc_block(uint32_t* block_no);
int ext4_free_block(uint32_t block_no);

// 文件操作
struct ext4_file* ext4_open(const char* path);
int ext4_close(struct ext4_file* file);
size_t ext4_read(struct ext4_file* file, void* buffer, size_t size);  // 修改：ssize_t -> size_t
size_t ext4_write(struct ext4_file* file, const void* buffer, size_t size);  // 修改：ssize_t -> size_t
int ext4_seek(struct ext4_file* file, uint32_t offset);
uint32_t ext4_tell(struct ext4_file* file);
int ext4_truncate(struct ext4_file* file, uint32_t size);

// 目录操作
int ext4_mkdir(const char* path);
int ext4_rmdir(const char* path);
struct ext4_dir* ext4_opendir(const char* path);
int ext4_closedir(struct ext4_dir* dir);
struct ext4_dir_entry_2* ext4_readdir(struct ext4_dir* dir);

// 文件和目录管理
int ext4_create(const char* path, uint16_t mode);
int ext4_unlink(const char* path);
int ext4_rename(const char* old_path, const char* new_path);
int ext4_stat(const char* path, struct ext4_inode* inode);

// 路径解析
int ext4_path_lookup(const char* path, uint32_t* inode_no);
int ext4_path_resolve(const char* path, char** components, int* count);

// 工具函数
uint32_t ext4_block_to_sector(uint32_t block_no);
uint32_t ext4_sector_to_block(uint32_t sector);
uint32_t ext4_get_block_size(void);
uint64_t ext4_get_free_space(void);
uint64_t ext4_get_total_space(void);
uint32_t ext4_get_inode_group(uint32_t inode_no);
uint32_t ext4_get_inode_index(uint32_t inode_no);

// 位图操作
int ext4_read_block_bitmap(uint32_t group, uint8_t* bitmap);
int ext4_read_inode_bitmap(uint32_t group, uint8_t* bitmap);
int ext4_write_block_bitmap(uint32_t group, const uint8_t* bitmap);
int ext4_write_inode_bitmap(uint32_t group, const uint8_t* bitmap);
int ext4_find_free_block(uint32_t group, uint32_t* block_no);
int ext4_find_free_inode(uint32_t group, uint32_t* inode_no);
int ext4_set_block_used(uint32_t block_no, int used);
int ext4_set_inode_used(uint32_t inode_no, int used);

// Extent操作
int ext4_extent_search(struct ext4_extent_header* eh, uint32_t block_offset, 
                       uint32_t* phys_block, int depth);
int ext4_extent_read(struct ext4_inode* inode, uint32_t block_offset, uint32_t* phys_block);
int ext4_extent_write(struct ext4_inode* inode, uint32_t block_offset, uint32_t phys_block);
int ext4_extent_alloc(struct ext4_inode* inode, uint32_t block_offset, uint32_t* phys_block);

// 测试函数
void ext4_test(void);

#endif