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

#include "kernel.h"
#include "stdint.h"
#include "graphics.h"
#include "shell.h"
#include "drivers/fs/fat32.h"
#include "string.h"

// 终端窗口配置
typedef struct {
    uint32_t x, y;               // 窗口左上角坐标
    uint32_t w, h;               // 窗口尺寸
    uint32_t cursor_x, cursor_y; // 终端内部光标位置
    uint32_t bg_color;
    uint32_t text_color;
} terminal_t;

// 终端配置信息
static terminal_t g_term = {
    .x = 80,
    .y = 80,
    .w = 600,
    .h = 400,
    .cursor_x = 8,
    .cursor_y = 8,
    .bg_color = 0x000000,
    .text_color = 0x00FF00 // 经典黑底绿字
};

// 图形相关
uint32_t screen_width;
uint32_t screen_height;

// Shell输出到图形终端的回调函数
static void shell_term_output(const char* str);
// FAT32测试函数
static void test_fat32(void);
// 自动检测FAT32分区
static uint32_t detect_fat32_partition(void);
// 格式化8.3文件名
static void format_83_name(const char* src, char* dest);

void draw_terminal_window();
void term_putc(char c);
void term_puts(const char* str);
void test_fat32_all(void);

__attribute__((ms_abi, target("no-sse"), target("general-regs-only"))) void kmain(void *params) {
    // 初始化串口
    serial_init(0x3F8);
    serial_puts("Kernel starting...\n");
    
    // 基础硬件初始化
    gdt_init();
    idt_init();
    pic_remap(32, 40);
    asm volatile("sti");

    // 图形渲染初始化
    framebuffer_info_t *fb_info = (framebuffer_info_t*)params;
    if (!fb_info) {
        serial_puts("ERROR: Framebuffer info is null\n");
        while (1);
    }

    // 驱动初始化
    ide_init();
    serial_puts("IDE driver loaded\n");
    
    // 检测并挂载FAT32分区
    uint32_t fat32_partition_start = detect_fat32_partition();
    if (fat32_partition_start != 0) {
        serial_puts("FAT32 partition detected at LBA: ");
        serial_putdec64(fat32_partition_start);
        serial_puts("\n");
        
        if (fat32_mount(fat32_partition_start)) {
            serial_puts("FAT32 file system mounted successfully\n");
            
            // 测试文件系统功能
            test_fat32();
        } else {
            serial_puts("Failed to mount FAT32 file system\n");
            serial_puts("Error: ");
            serial_puts(fat32_get_error());
            serial_puts("\n");
        }
    } else {
        serial_puts("No FAT32 partition found\n");
        // 尝试使用默认分区起始位置 (2048)
        serial_puts("Trying default partition start (LBA 2048)...\n");
        if (fat32_mount(2048)) {
            serial_puts("FAT32 mounted at default location (LBA 2048)\n");
        }
    }
    test_fat32_all();
    // 图形初始化
    graphics_init(fb_info);
    screen_width = fb_info->framebuffer_width;
    screen_height = fb_info->framebuffer_height;

    // 绘制桌面背景
    clear_screen(0x169de2); 
    
    // 初始化并绘制窗口
    draw_terminal_window();
    
    // 初始化Shell并设置图形终端输出回调
    shell_set_term_output(shell_term_output);
    shell_init();
    
    term_puts("MWOS Kernel initialized successfully!\n");
    term_puts("FAT32 File System: ");
    
    if (fat32_partition_start != 0) {
        char status_msg[64];
        strcpy(status_msg, "Mounted at LBA ");
        
        // 将分区起始位置转换为字符串
        char num_str[12];
        uint32_t temp = fat32_partition_start;
        int i = 0;
        
        do {
            num_str[i++] = '0' + (temp % 10);
            temp /= 10;
        } while (temp > 0);
        
        for (int j = i - 1; j >= 0; j--) {
            char c[2] = {num_str[j], 0};
            strcat(status_msg, c);
        }
        
        term_puts(status_msg);
    } else {
        term_puts("Not available");
    }
    
    term_puts("\n");
    term_puts("Use serial port (COM1) to enter commands.\n");
    term_puts("Type 'help' for available commands.\n");
    term_puts("Type 'ls' to list files on FAT32 partition.\n\n");

    // 在 kmain.c 的主循环中，确保正确处理所有字符
    while (1) {
        // 检查串口输入
        if (serial_received()) {
            char c = serial_getc();
            
            // 处理特殊字符转换
            if (c == '\r') {
                c = '\n';  // 转换回车符
            } else if (c == 0x7F) {
                c = '\b';  // 转换删除键
            }
            
            // 输出到图形终端
            term_putc(c);
            
            // 传递给Shell处理
            shell_process_char(c);
        }
        
        // 短暂的延迟
        for (volatile int i = 0; i < 1000; i++) {
            asm volatile("nop");
        }
    }
}

// Shell输出到图形终端的回调函数
static void shell_term_output(const char* str) {
    term_puts(str);
}

// FAT32文件系统测试函数
static void test_fat32(void) {
    serial_puts("=== FAT32 File System Test ===\n");
    
    // 尝试打开根目录
    fat32_handle_t root;
    if (fat32_open_root(&root)) {
        serial_puts("Root directory opened successfully\n");
        
        // 读取并显示目录内容
        fat32_dir_entry_t entry;
        int file_count = 0, dir_count = 0;
        
        serial_puts("Contents of root directory:\n");
        while (fat32_read_dir(&root, &entry)) {
            // 跳过空条目和长文件名条目
            if (entry.name[0] == 0x00 || entry.name[0] == 0xE5 || 
                entry.attributes == 0x0F) {
                continue;
            }
            
            // 格式化文件名
            char name[13];
            format_83_name(entry.name, name);
            
            serial_puts("  ");
            serial_puts(name);
            
            if (entry.attributes & ATTR_DIRECTORY) {
                serial_puts(" [DIR]");
                dir_count++;
            } else {
                serial_puts(" (");
                serial_putdec64(entry.file_size);
                serial_puts(" bytes)");
                file_count++;
            }
            serial_puts("\n");
        }
        
        serial_puts("\nTotal: ");
        serial_putdec64(file_count);
        serial_puts(" files, ");
        serial_putdec64(dir_count);
        serial_puts(" directories\n");
        
        fat32_close(&root);
        
        // 尝试读取一个已知文件（如果存在）
        if (fat32_file_exists("/README.TXT")) {
            serial_puts("\nTesting file read...\n");
            
            fat32_handle_t file;
            if (fat32_open("/README.TXT", &file, FILE_READ)) {
                char buffer[256];
                if (fat32_read(&file, buffer, sizeof(buffer) - 1)) {
                    buffer[sizeof(buffer) - 1] = '\0';
                    serial_puts("Content of README.TXT (first 255 bytes):\n");
                    serial_puts(buffer);
                    serial_puts("\n");
                }
                fat32_close(&file);
            }
        }
        
        // 检查可用空间
        // 注意：这个函数需要在fat32.c中实现
        // uint32_t free_space = fat32_get_free_space();
        // serial_puts("Free space: ");
        // serial_putdec(free_space);
        // serial_puts(" bytes\n");
        
    } else {
        serial_puts("Failed to open root directory\n");
        serial_puts("Error: ");
        serial_puts(fat32_get_error());
        serial_puts("\n");
    }
    
    serial_puts("=== End of FAT32 Test ===\n\n");
}

// 自动检测FAT32分区
static uint32_t detect_fat32_partition(void) {
    uint8_t mbr[512];
    
    // 读取MBR（主引导记录）
    if (ide_read_sectors(0, 1, mbr) != 0) {
        serial_puts("Failed to read MBR\n");
        return 0;
    }
    
    // 检查引导签名
    if (mbr[510] != 0x55 || mbr[511] != 0xAA) {
        serial_puts("Invalid boot signature\n");
        return 0;
    }
    
    serial_puts("Valid MBR boot signature found\n");
    
    // 查找FAT32分区（最多4个主分区）
    for (int i = 0; i < 4; i++) {
        int offset = 0x1BE + i * 16;  // 分区表条目偏移
        
        // 分区类型
        uint8_t type = mbr[offset + 4];
        
        // 检查是否是FAT32分区
        // 0x0B = FAT32 (CHS), 0x0C = FAT32 (LBA)
        if (type == 0x0B || type == 0x0C || type == 0x0E) {
            // 获取分区起始LBA地址
            uint32_t lba_start = 
                (uint32_t)mbr[offset + 8] |
                ((uint32_t)mbr[offset + 9] << 8) |
                ((uint32_t)mbr[offset + 10] << 16) |
                ((uint32_t)mbr[offset + 11] << 24);
            
            serial_puts("Found FAT32 partition at index ");
            serial_putdec64(i + 1);
            serial_puts(", LBA start: ");
            serial_putdec64(lba_start);
            serial_puts("\n");
            
            // 验证分区引导扇区
            uint8_t boot_sector[512];
            if (ide_read_sectors(lba_start, 1, boot_sector) == 0) {
                // 检查FAT32签名
                if (boot_sector[510] == 0x55 && boot_sector[511] == 0xAA) {
                    // 检查文件系统类型字符串
                    char fs_type[9];
                    for (int j = 0; j < 8; j++) {
                        fs_type[j] = boot_sector[0x52 + j];
                    }
                    fs_type[8] = '\0';
                    
                    serial_puts("File system type: ");
                    serial_puts(fs_type);
                    serial_puts("\n");
                    
                    return lba_start;
                }
            }
        }
    }
    
    serial_puts("No FAT32 partition found in MBR\n");
    
    // 如果MBR中没有找到，尝试使用常见的位置
    // 常见分区起始位置：2048 (1MB对齐)
    serial_puts("Checking common partition start (LBA 2048)...\n");
    
    uint8_t boot_sector[512];
    if (ide_read_sectors(2048, 1, boot_sector) == 0) {
        if (boot_sector[510] == 0x55 && boot_sector[511] == 0xAA) {
            // 检查是否真的是FAT32
            char fs_type[9];
            for (int i = 0; i < 8; i++) {
                fs_type[i] = boot_sector[0x52 + i];
            }
            fs_type[8] = '\0';
            
            if (strncmp(fs_type, "FAT32", 5) == 0 || 
                strncmp(fs_type, "FAT", 3) == 0) {
                serial_puts("Found FAT32 at default location (LBA 2048)\n");
                return 2048;
            }
        }
    }
    
    return 0;
}

// 格式化8.3文件名
static void format_83_name(const char* src, char* dest) {
    int i, j = 0;
    
    // 复制文件名部分（前8个字符）
    for (i = 0; i < 8 && src[i] != ' '; i++) {
        dest[j++] = src[i];
    }
    
    // 如果有扩展名，添加点号
    if (src[8] != ' ') {
        dest[j++] = '.';
        
        // 复制扩展名（后3个字符）
        for (i = 8; i < 11 && src[i] != ' '; i++) {
            dest[j++] = src[i];
        }
    }
    
    dest[j] = '\0';
}

// 终端渲染
void draw_terminal_window() {
    // 绘制标题栏 (深蓝色)
    draw_rect(g_term.x, g_term.y, g_term.w, 28, 0x224488);
    print_string("MWOS System Terminal v1.0", g_term.x + 10, g_term.y + 6, 0xFFFFFF);
    // 绘制终端黑色背景区
    draw_rect(g_term.x, g_term.y + 28, g_term.w, g_term.h - 28, g_term.bg_color);
}

void term_putc(char c) {
    // 字符实际渲染坐标 = 窗口起始点 + 内容区偏移 + 光标偏移
    uint32_t real_x = g_term.x + g_term.cursor_x;
    uint32_t real_y = g_term.y + 28 + g_term.cursor_y;

    if (c == '\n') {
        g_term.cursor_x = 8;
        g_term.cursor_y += 18;
    } else if (c == '\r') {
        g_term.cursor_x = 8;
    } else if (c == '\b') {  // 退格键处理
        if (g_term.cursor_x > 8) {
            g_term.cursor_x -= 8;
            // 擦除字符
            draw_rect(real_x - 8, real_y, 8, 16, g_term.bg_color);
        }
        return;  // 不绘制字符
    } else {
        put_char(c, real_x, real_y, g_term.text_color);
        g_term.cursor_x += 8;
        
        // 自动换行
        if (g_term.cursor_x + 16 > g_term.w) {
            g_term.cursor_x = 8;
            g_term.cursor_y += 18;
        }
    }

    // 简单的滚动检查：如果到底了就重置到顶部并清屏
    if (g_term.cursor_y + 18 > g_term.h - 30) {
        draw_rect(g_term.x, g_term.y + 28, g_term.w, g_term.h - 28, g_term.bg_color);
        g_term.cursor_y = 8;
    }
}

void term_puts(const char* str) {
    while (*str) {
        term_putc(*str++);
   
    }
}

void test_fat32_all(void) {
    serial_puts("\n=== FAT32 完整功能测试 ===\n");
    
    // 1. 检查文件系统是否已挂载
    if (!fat32_mounted()) {
        serial_puts("FAT32 文件系统未挂载\n");
        return;
    }
    
    // 2. 显示文件系统信息
    fat32_print_info();
    
    // 3. 测试目录操作
    serial_puts("\n3. 测试目录操作:\n");
    
    // 创建目录
    if (fat32_create_dir("/TEST_DIR")) {
        serial_puts("  创建目录 /TEST_DIR 成功\n");
    } else {
        serial_puts("  创建目录失败: ");
        serial_puts(fat32_get_error());
        serial_puts("\n");
    }
    
    // 创建子目录
    if (fat32_create_dir("/TEST_DIR/SUBDIR")) {
        serial_puts("  创建目录 /TEST_DIR/SUBDIR 成功\n");
    }
    
    // 4. 测试文件操作
    serial_puts("\n4. 测试文件操作:\n");
    
    // 创建文件
    if (fat32_create_file("/TEST_DIR/test1.txt")) {
        serial_puts("  创建文件 /TEST_DIR/test1.txt 成功\n");
    }
    
    // 写入文件
    fat32_handle_t file1;
    if (fat32_open("/TEST_DIR/test1.txt", &file1, FILE_WRITE)) {
        const char* content = "Hello FAT32 File System!\nThis is a test file.\n";
        if (fat32_write(&file1, content, strlen(content))) {
            serial_puts("  写入文件成功\n");
        }
        fat32_close(&file1);
    }
    
    // 读取文件
    if (fat32_open("/TEST_DIR/test1.txt", &file1, FILE_READ)) {
        char buffer[100];
        if (fat32_read(&file1, buffer, sizeof(buffer) - 1)) {
            buffer[sizeof(buffer) - 1] = '\0';
            serial_puts("  读取文件内容:\n");
            serial_puts(buffer);
        }
        fat32_close(&file1);
    }
    
    // 5. 测试文件属性
    serial_puts("\n5. 测试文件属性:\n");
    
    fat32_dir_entry_t file_info;
    if (fat32_get_file_info("/TEST_DIR/test1.txt", &file_info)) {
        serial_puts("  文件信息:\n");
        
        char name[13];
        format_83_name(file_info.name, name);
        serial_puts("    文件名: ");
        serial_puts(name);
        serial_puts("\n");
        
        serial_puts("    文件大小: ");
        serial_putdec64(file_info.file_size);
        serial_puts(" 字节\n");
        
        serial_puts("    属性: ");
        if (file_info.attributes & ATTR_READ_ONLY) serial_puts("R");
        if (file_info.attributes & ATTR_HIDDEN) serial_puts("H");
        if (file_info.attributes & ATTR_SYSTEM) serial_puts("S");
        if (file_info.attributes & ATTR_DIRECTORY) serial_puts("D");
        if (file_info.attributes & ATTR_ARCHIVE) serial_puts("A");
        serial_puts("\n");
    }
    
    // 6. 测试目录遍历
    serial_puts("\n6. 测试目录遍历:\n");
    
    fat32_handle_t dir_handle;
    if (fat32_open("/TEST_DIR", &dir_handle, FILE_READ)) {
        fat32_dir_entry_t entry;
        int count = 0;
        
        serial_puts("  /TEST_DIR 目录内容:\n");
        while (fat32_read_dir(&dir_handle, &entry)) {
            char name[13];
            format_83_name(entry.name, name);
            
            serial_puts("    ");
            serial_puts(name);
            
            if (entry.attributes & ATTR_DIRECTORY) {
                serial_puts(" [目录]");
            } else {
                serial_puts(" (");
                serial_putdec64(entry.file_size);
                serial_puts(" 字节)");
            }
            serial_puts("\n");
            
            count++;
        }
        
        if (count == 0) {
            serial_puts("    (空)\n");
        }
        
        fat32_close(&dir_handle);
    }
    
    // 7. 测试文件操作
    serial_puts("\n7. 测试文件操作:\n");
    
    // 复制文件
    if (fat32_copy("/TEST_DIR/test1.txt", "/TEST_DIR/test2.txt")) {
        serial_puts("  复制文件成功\n");
    }
    
    // 重命名文件
    if (fat32_rename("/TEST_DIR/test2.txt", "/TEST_DIR/test_renamed.txt")) {
        serial_puts("  重命名文件成功\n");
    }
    
    // 删除文件
    if (fat32_delete_file("/TEST_DIR/test_renamed.txt")) {
        serial_puts("  删除文件成功\n");
    }
    
    // 8. 测试空间信息
    serial_puts("\n8. 测试空间信息:\n");
    
    //uint32_t total = fat32_get_total_space();
    //uint32_t free = fat32_get_free_space();
    //uint32_t used = total - free;
    
    /*serial_puts("  总空间: ");
    serial_putdec64(total / 1024);
    serial_puts(" KB\n");
    
    serial_puts("  已用空间: ");
    serial_putdec64(used / 1024);
    serial_puts(" KB\n");
    
    serial_puts("  空闲空间: ");
    serial_putdec64(free / 1024);
    serial_puts(" KB\n");*/
    
    // 9. 测试大文件操作
    serial_puts("\n9. 测试大文件操作:\n");
    
    fat32_handle_t big_file;
    if (fat32_open("/bigfile.dat", &big_file, FILE_CREATE)) {
        // 写入大量数据
        uint8_t data[1024];
        for (int i = 0; i < 1024; i++) {
            data[i] = i % 256;
        }
        
        // 写入100KB数据
        for (int i = 0; i < 100; i++) {
            if (!fat32_write(&big_file, data, sizeof(data))) {
                serial_puts("  写入大文件失败\n");
                break;
            }
        }
        
        serial_puts("  写入大文件成功，大小: ");
        serial_putdec64(big_file.file_size);
        serial_puts(" 字节\n");
        
        fat32_close(&big_file);
        
        // 验证文件大小
        uint32_t file_size = fat32_get_file_size("/bigfile.dat");
        serial_puts("  验证文件大小: ");
        serial_putdec64(file_size);
        serial_puts(" 字节\n");
        
        // 删除大文件
        if (fat32_delete_file("/bigfile.dat")) {
            serial_puts("  删除大文件成功\n");
        }
    }
    
    // 10. 清理测试文件
    serial_puts("\n10. 清理测试文件:\n");
    
    if (fat32_delete_file("/TEST_DIR/test1.txt")) {
        serial_puts("  删除测试文件成功\n");
    }
    
    if (fat32_remove_dir("/TEST_DIR/SUBDIR")) {
        serial_puts("  删除子目录成功\n");
    }
    
    if (fat32_remove_dir("/TEST_DIR")) {
        serial_puts("  删除测试目录成功\n");
    }
    
    serial_puts("\n=== FAT32 测试完成 ===\n");
}

