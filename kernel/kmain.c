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

              ,----------------,              ,---------,
         ,-----------------------,          ,"        ,"|
       ,"                      ,"|        ,"        ,"  |
      +-----------------------+  |      ,"        ,"    |
      |  .-----------------.  |  |     +---------+      |
      |  |                 |  |  |     | -==----'|      |
      |  |  Never gonna    |  |  |     |         |      |
      |  |  Give you bug   |  |  |/----|`---=    |      |
      |  |  C:\>_          |  |  |   ,/|==== ooo |      ;
      |  |                 |  |  |  // |(((( [33]|    ,"
      |  `-----------------'  |," .;'| |((((     |  ,"
      +-----------------------+  ;;  | |         |,"
         /_)______________(_/  //'   | +---------+
    ___________________________/___  `,
   /  oooooooooooooooo  .o.  oooo /,   \,"-----------
  / ==ooooooooooooooo==.o.  ooo= //   ,`\--{)B     ,"
 /_==__==========__==_ooo__ooo=_/'   /___________,"
*/

#include "kernel.h"
#include "stdint.h"
#include "graphics.h"
#include "kernel.h"
#include "stdint.h"
#include "graphics.h"
#include "shell.h"
#include "drivers/fs/fat32.h"
#include "string.h"

// 终端窗口配置
typedef struct
{
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
    .text_color = 0x00FF00 // 黑底绿字
};

// 上一次鼠标的位置，初始化为 -1 表示还未绘制过
static int32_t old_mouse_x = -1;
static int32_t old_mouse_y = -1;

// 鼠标
int32_t mouse_x = 0;
int32_t mouse_y = 0;

// 图形相关
uint32_t screen_width;
uint32_t screen_height;

// 参数声明
boot_params_t kernel_params;

// 命令保存
#define MAX_COMMAND_LEN 256
static char g_input_buffer[MAX_COMMAND_LEN];
static uint32_t g_input_index = 0;

void draw_terminal_window();
void term_putc(char c);
void on_keyboard_pressed(uint8_t scancode, uint8_t final_char);
void term_puts(const char *str);
void test_fat32_all(void);
static uint32_t detect_fat32_partition(void);
static void test_fat32(void);
static void format_83_name(const char* src, char* dest);

__attribute__((ms_abi, target("no-sse"), target("general-regs-only")))
void kmain(void *params) {
    // 基础架构初始化
    serial_init(0x3F8);
    gdt_init();
    idt_init();
    
    // PIC 重映射 (此时默认全屏蔽)
    pic_remap(32, 40); 
    
    // 时钟初始化 (注册 IDT 32)
    timer_init(1000); 

    // 获取并解析启动参数
    /*boot_params_t *lp_params = (boot_params_t *)params;
    kernel_params = *lp_params;*/
    boot_params_t *lp_params = (boot_params_t *)params;
    kernel_params = *lp_params;
    uint32_t bytes_per_pixel = kernel_params.framebuffer_bpp / 8;
    uint64_t framebuffer_size = (uint64_t)kernel_params.framebuffer_pitch * kernel_params.framebuffer_height * bytes_per_pixel;

    // 内存与驱动初始化
    serial_puthex64(kernel_params.memory_map_addr);
    serial_puts("\n");
    serial_puthex64(kernel_params.memory_map_size);
    serial_puts("\n");
    serial_puthex64(kernel_params.descriptor_size);
    serial_puts("\n");
    
    pmm_init((void *)kernel_params.memory_map_addr, 
             kernel_params.memory_map_size, 
             kernel_params.descriptor_size);
    //serial_puts("a\n")   ;      
    ide_init();
    keyboard_init();
    mouse_init();
    
    /*uint32_t fat32_partition_start = detect_fat32_partition();
    if (fat32_partition_start != 0) {
        serial_puts("FAT32 partition detected at LBA: ");
        serial_putdec64(fat32_partition_start);
        serial_puts("\n");

        if (fat32_mount(fat32_partition_start)) {
            serial_puts("FAT32 file system mounted successfully\n");

            test_fat32();
        } else {
            serial_puts("Failed to mount FAT32 file system\n");
            serial_puts("Error: ");
            serial_puts(fat32_get_error());
            serial_puts("\n");
        }
    } else {
        serial_puts("No FAT32 partition found\n");
        serial_puts("Trying default partition start (LBA 2048)...\n");
        if (fat32_mount(2048)) {
            serial_puts("FAT32 mounted at default location (LBA 2048)\n");
        }
    }
    test_fat32_all();*/
    
    // 图形系统
    graphics_init(&kernel_params);
    
    // 开启主片: IRQ0(时钟), IRQ1(键盘), IRQ2(级联)
    outb(0x21, 0xF8);
    // 开启从片: IRQ12(鼠标)
    outb(0xA1, 0xEF);
    asm volatile("sti");
    //serial_puts("a");

    // UI 绘制
    clear_screen(0x169de2);
    draw_terminal_window();
    term_puts("NovaVector MWOS [版本1.0.0]\n");
    term_puts("(C) NovaVector Studio 保留所有权利\n");
    term_puts("\n");
    term_puts("Root@MWOS: /# ");

    // 主循环
    while (1) {
        asm volatile("hlt");
    }
}

// 终端渲染
void draw_terminal_window()
{
    // 绘制标题栏 (深蓝色)
    draw_rect(g_term.x, g_term.y, g_term.w, 28, 0x224488);
    print_string("MWOS System Terminal v1.0", g_term.x + 10, g_term.y + 6, 0xFFFFFF);
    // 绘制终端黑色背景区
    draw_rect(g_term.x, g_term.y + 28, g_term.w, g_term.h - 28, g_term.bg_color);
}

void term_putc(char c)
{
    // 字符实际渲染坐标 = 窗口起始点 + 内容区偏移 + 光标偏移
    uint32_t real_x = g_term.x + g_term.cursor_x;
    uint32_t real_y = g_term.y + 28 + g_term.cursor_y;

    if (c == '\n')
    {
        g_term.cursor_x = 8;
        g_term.cursor_y += 18;
    }
    else if (c == '\r')
    {
        g_term.cursor_x = 8;
    }
    else
    {
        put_char(c, real_x, real_y, g_term.text_color);
        g_term.cursor_x += 8;

        // 自动换行
        if (g_term.cursor_x + 16 > g_term.w)
        {
            g_term.cursor_x = 8;
            g_term.cursor_y += 18;
        }
    }

    // 滚动检查
    if (g_term.cursor_y + 18 > g_term.h - 30)
    {
        draw_rect(g_term.x, g_term.y + 28, g_term.w, g_term.h - 28, g_term.bg_color);
        g_term.cursor_y = 8;
    }
}

void term_puts(const char *str) {
    uint8_t *p = (uint8_t *)str;
    while (*p) {
        uint32_t real_x = g_term.x + g_term.cursor_x;
        uint32_t real_y = g_term.y + 28 + g_term.cursor_y;

        // 处理换行符
        if (*p == '\n') {
            g_term.cursor_x = 8;
            g_term.cursor_y += 18;
            p++;
        }
        else if (*p == '\r') {
            g_term.cursor_x = 8;
            p++;
        }
        // 处理 ASCII (单字节)
        else if (*p < 0x80) {
            put_char(*p, real_x, real_y, g_term.text_color);
            g_term.cursor_x += 8;
            p++;
        }
        // 处理 UTF-8 汉字 (3 字节)
        else if ((*p & 0xE0) == 0xE0) {
            char utf8_buf[4];
            utf8_buf[0] = p[0];
            utf8_buf[1] = p[1];
            utf8_buf[2] = p[2];
            utf8_buf[3] = '\0';

            print_string(utf8_buf, real_x, real_y, g_term.text_color);
            
            g_term.cursor_x += 16; // 汉字占据 16 像素宽
            p += 3;
        } 
        else {
            p++;
        }

        // 自动换行
        if (g_term.cursor_x + 16 > g_term.w) {
            g_term.cursor_x = 8;
            g_term.cursor_y += 18;
        }

        // 滚动检查
        if (g_term.cursor_y + 18 > g_term.h - 30) {
            draw_rect(g_term.x, g_term.y + 28, g_term.w, g_term.h - 28, g_term.bg_color);
            g_term.cursor_y = 8;
        }
    }
}

// 清空缓冲区
void clear_input_buffer() {
    for (uint32_t i = 0; i < MAX_COMMAND_LEN; i++) {
        g_input_buffer[i] = '\0';
    }
    g_input_index = 0;
}

// 键盘回调
void on_keyboard_pressed(uint8_t scancode, uint8_t final_char)
{
    if (final_char == 0) return;

    // 处理回车
    if (final_char == '\n' || final_char == '\r') {
        term_putc('\n');
        if (g_input_index > 0) {
            if (strcmp(g_input_buffer, "version") == 0)
            {
                term_puts("NovaVector MWOS V1.0.0\n(C) NovaVector Studio 保留所有权利\n");
            }
            else if (strcmp(g_input_buffer, "/kill McLDY") == 0)
            {
                term_puts("McLDY was slain by _Undefiend404\n");
            }
            else
            {
                term_puts("Unknown command: ");
                term_puts(g_input_buffer);
                term_putc('\n');
            }
        }

        clear_input_buffer();
        term_puts("Root@MWOS: /# ");
    }
    // 处理退格
    else if (final_char == '\b') {
        if (g_input_index > 0) {
            g_input_index--;
            g_input_buffer[g_input_index] = '\0';
            
            // 视觉上回退
            if (g_term.cursor_x > 8) {
                g_term.cursor_x -= 8;
                uint32_t real_x = g_term.x + g_term.cursor_x;
                uint32_t real_y = g_term.y + 28 + g_term.cursor_y;
                draw_rect(real_x, real_y, 8, 16, g_term.bg_color);
            }
        }
    }
    // 处理普通字符
    else {
        if (g_input_index < MAX_COMMAND_LEN - 1) {
            g_input_buffer[g_input_index++] = (char)final_char;
            g_input_buffer[g_input_index] = '\0';
            term_putc((char)final_char);
        }
    }
}

// 鼠标回调
void on_mouse_update(int32_t x_rel, int32_t y_rel, uint8_t left_button, uint8_t middle_button, uint8_t right_button)
{
    // 擦除旧光标
    // 如果不是第一次绘制，先恢复上一次保存的背景
    if (old_mouse_x != -1)
    {
        restore_mouse_background();
    }

    // 更新并限制鼠标坐标
    mouse_x += x_rel;
    mouse_y -= y_rel; // PS/2 Y轴向上为正，屏幕向下为正，所以用减法

    // 边界检查
    if (mouse_x < 0)
        mouse_x = 0;
    if (mouse_y < 0)
        mouse_y = 0;
    if (mouse_x >= (int32_t)g_framebuffer->framebuffer_width)
        mouse_x = g_framebuffer->framebuffer_width - 1;
    if (mouse_y >= (int32_t)g_framebuffer->framebuffer_height)
        mouse_y = g_framebuffer->framebuffer_height - 1;

    // 绘制检查
    save_mouse_background(mouse_x, mouse_y);
    draw_mouse_cursor(mouse_x, mouse_y);

    // 记录当前位置为旧位置
    old_mouse_x = mouse_x;
    old_mouse_y = mouse_y;
}

static void test_fat32(void) {
    serial_puts("=== FAT32 File System Test ===\n");

    fat32_handle_t root;
    if (fat32_open_root(&root)) {
        serial_puts("Root directory opened successfully\n");

        fat32_dir_entry_t entry;
        int file_count = 0, dir_count = 0;

        serial_puts("Contents of root directory:\n");
        while (fat32_read_dir(&root, &entry)) {
            if (entry.name[0] == 0x00 || entry.name[0] == 0xE5 ||
                entry.attributes == 0x0F) {
                continue;
            }

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

static uint32_t detect_fat32_partition(void) {
    uint8_t mbr[512];

    if (ide_read_sectors(0, 1, mbr) != 0) {
        serial_puts("Failed to read MBR\n");
        return 0;
    }

    if (mbr[510] != 0x55 || mbr[511] != 0xAA) {
        serial_puts("Invalid boot signature\n");
        return 0;
    }

    serial_puts("Valid MBR boot signature found\n");

    for (int i = 0; i < 4; i++) {
        int offset = 0x1BE + i * 16;

        uint8_t type = mbr[offset + 4];

        // 0x0B = FAT32 (CHS), 0x0C = FAT32 (LBA)
        if (type == 0x0B || type == 0x0C || type == 0x0E) {
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

            uint8_t boot_sector[512];
            if (ide_read_sectors(lba_start, 1, boot_sector) == 0) {
                if (boot_sector[510] == 0x55 && boot_sector[511] == 0xAA) {
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

    serial_puts("Checking common partition start (LBA 2048)...\n");

    uint8_t boot_sector[512];
    if (ide_read_sectors(2048, 1, boot_sector) == 0) {
        if (boot_sector[510] == 0x55 && boot_sector[511] == 0xAA) {
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

static void format_83_name(const char* src, char* dest) {
    int i, j = 0;

    for (i = 0; i < 8 && src[i] != ' '; i++) {
        dest[j++] = src[i];
    }

    if (src[8] != ' ') {
        dest[j++] = '.';

        for (i = 8; i < 11 && src[i] != ' '; i++) {
            dest[j++] = src[i];
        }
    }

    dest[j] = '\0';
}

void test_fat32_all(void) {
    serial_puts("\n=== FAT32 完整功能测试 ===\n");

    if (!fat32_mounted()) {
        serial_puts("FAT32 文件系统未挂载\n");
        return;
    }

    fat32_print_info();

    serial_puts("\n3. 测试目录操作:\n");

    if (fat32_create_dir("/TEST_DIR")) {
        serial_puts("  创建目录 /TEST_DIR 成功\n");
    } else {
        serial_puts("  创建目录失败: ");
        serial_puts(fat32_get_error());
        serial_puts("\n");
    }

    if (fat32_create_dir("/TEST_DIR/SUBDIR")) {
        serial_puts("  创建目录 /TEST_DIR/SUBDIR 成功\n");
    }

    serial_puts("\n4. 测试文件操作:\n");

    if (fat32_create_file("/TEST_DIR/test1.txt")) {
        serial_puts("  创建文件 /TEST_DIR/test1.txt 成功\n");
    }

    fat32_handle_t file1;
    if (fat32_open("/TEST_DIR/test1.txt", &file1, FILE_WRITE)) {
        const char* content = "Hello FAT32 File System!\nThis is a test file.\n";
        if (fat32_write(&file1, content, strlen(content))) {
            serial_puts("  写入文件成功\n");
        }
        fat32_close(&file1);
    }

    if (fat32_open("/TEST_DIR/test1.txt", &file1, FILE_READ)) {
        char buffer[100];
        if (fat32_read(&file1, buffer, sizeof(buffer) - 1)) {
            buffer[sizeof(buffer) - 1] = '\0';
            serial_puts("  读取文件内容:\n");
            serial_puts(buffer);
        }
        fat32_close(&file1);
    }

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

    serial_puts("\n7. 测试文件操作:\n");

    if (fat32_copy("/TEST_DIR/test1.txt", "/TEST_DIR/test2.txt")) {
        serial_puts("  复制文件成功\n");
    }

    if (fat32_rename("/TEST_DIR/test2.txt", "/TEST_DIR/test_renamed.txt")) {
        serial_puts("  重命名文件成功\n");
    }

    if (fat32_delete_file("/TEST_DIR/test_renamed.txt")) {
        serial_puts("  删除文件成功\n");
    }

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

    serial_puts("\n9. 测试大文件操作:\n");

    fat32_handle_t big_file;
    if (fat32_open("/bigfile.dat", &big_file, FILE_CREATE)) {
        uint8_t data[1024];
        for (int i = 0; i < 1024; i++) {
            data[i] = i % 256;
        }

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

        uint32_t file_size = fat32_get_file_size("/bigfile.dat");
        serial_puts("  验证文件大小: ");
        serial_putdec64(file_size);
        serial_puts(" 字节\n");

        if (fat32_delete_file("/bigfile.dat")) {
            serial_puts("  删除大文件成功\n");
        }
    }

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



