#include <stdint.h>
#include "graphics.h"
#include "fbtext.h"
#include "io.h"
#include "limine.h"
#include "sb16.h"
#include "sata.h"
#include "keyboard.h"
#include "mouse.h"
#include "idt.h"
#include "pic.h"

volatile struct limine_framebuffer_request framebuffer_request
    __attribute__((section(".limine"), used)) = {
        .id = LIMINE_FRAMEBUFFER_REQUEST_ID,
        .revision = 0,
        .response = 0
    };
/*
#if defined(LIMINE_REQUEST_ID_FRAMEBUFFER)
    .id = LIMINE_REQUEST_ID_FRAMEBUFFER,
#elif defined(LIMINE_FRAMEBUFFER_REQUEST)
    .id = LIMINE_FRAMEBUFFER_REQUEST,
#else
    .id = 0x9d8a0f4f4f15c0c0ULL,
#endif
    .revision = 0,
    .response = 0
};*/

// 如果你的内核是 higher-half（非恒等映射），在编译时定义 PHYS_OFFSET，把物理地址转换为虚拟地址。
// 例如：-DPHYS_OFFSET=0xffffffff80000000ULL
#ifndef PHYS_OFFSET
#define PHYS_TO_VIRT(p) ((void*)(uintptr_t)(p))
#else
#define PHYS_TO_VIRT(p) ((void*)(uintptr_t)((uint64_t)(p) + (uint64_t)PHYS_OFFSET))
#endif

// 简单串口 (COM1 0x3F8) 调试输出，帮助确认内核是否真的跑到这里以及 Limine 返回了什么
#define COM1 0x3F8

static void serial_init(void) {
    outb(COM1 + 1, 0x00); // Disable all interrupts
    outb(COM1 + 3, 0x80); // Enable DLAB (set baud rate divisor)
    outb(COM1 + 0, 0x03); // Set divisor to 3 (lo byte) 38400 baud
    outb(COM1 + 1, 0x00); //                  (hi byte)
    outb(COM1 + 3, 0x03); // 8 bits, no parity, one stop bit
    outb(COM1 + 2, 0xC7); // Enable FIFO, clear them, with 14-byte threshold
    outb(COM1 + 4, 0x0B); // IRQs enabled, RTS/DSR set
}

static int serial_is_transmit_empty(void) {
    return inb(COM1 + 5) & 0x20;
}

static void serial_putc(char c) {
    while (!serial_is_transmit_empty()) { /* spin */ }
    outb(COM1, (unsigned char)c);
}

static void serial_puts(const char *s) {
    while (*s) {
        if (*s == '\n') serial_putc('\r');
        serial_putc(*s++);
    }
}

// 打印十六进制数（64-bit）
static void serial_puthex64(uint64_t v) {
    char buf[17];
    const char *hex = "0123456789ABCDEF";
    for (int i = 0; i < 16; i++) {
        buf[15 - i] = hex[v & 0xF];
        v >>= 4;
    }
    buf[16] = '\0';
    serial_puts("0x");
    serial_puts(buf);
}

// 绘制简单的鼠标指针
void draw_cursor(int x, int y) {
    // 绘制简单的十字光标
    for (int i = -5; i <= 5; i++) {
        if (x + i >= 0 && x + i < g_framebuffer->framebuffer_width) {
            put_pixel(x + i, y, COLOR_WHITE);
        }
        if (y + i >= 0 && y + i < g_framebuffer->framebuffer_height) {
            put_pixel(x, y + i, COLOR_WHITE);
        }
    }
}

// 清除鼠标指针
void clear_cursor(int x, int y) {
    // 简单的清除方法：重新绘制背景
    // 注意：这应该根据实际背景更智能地实现
    for (int i = -5; i <= 5; i++) {
        if (x + i >= 0 && x + i < g_framebuffer->framebuffer_width) {
            put_pixel(x + i, y, 0x008080); // Win98蓝色背景
        }
        if (y + i >= 0 && y + i < g_framebuffer->framebuffer_height) {
            put_pixel(x, y + i, 0x008080); // Win98蓝色背景
        }
    }
}

void kmain(void *params) {
    (void)params;

    // 初始化串口尽快输出调试信息
    serial_init();
    serial_puts("kernel: entered kmain\n");

    if (!framebuffer_request.response) {
        serial_puts("framebuffer_request.response == NULL\n");
    } else {
        serial_puts("framebuffer_request.response present\n");
    }

    if (framebuffer_request.response && framebuffer_request.response->framebuffer_count > 0) {
        struct limine_framebuffer *fb = framebuffer_request.response->framebuffers[0];
        if (fb) {
            serial_puts("limine framebuffer found:\n");
            serial_puts("  address: ");
            serial_puthex64((uint64_t)(uintptr_t)fb->address);
            serial_puts("\n  width: ");
            // convert numbers to decimal roughly (simple)
            {
                char dec[32];
                int pos = 0;
                uint64_t w = fb->width;
                if (w == 0) { dec[pos++] = '0'; }
                else {
                    char rev[32]; int rp = 0;
                    while (w) { rev[rp++] = '0' + (w % 10); w /= 10; }
                    while (rp--) dec[pos++] = rev[rp];
                }
                dec[pos] = '\0';
                serial_puts(dec);
            }
            serial_puts("\n  height: ");
            {
                char dec[32];
                int pos = 0;
                uint64_t h = fb->height;
                if (h == 0) { dec[pos++] = '0'; }
                else {
                    char rev[32]; int rp = 0;
                    while (h) { rev[rp++] = '0' + (h % 10); h /= 10; }
                    while (rp--) dec[pos++] = rev[rp];
                }
                dec[pos] = '\0';
                serial_puts(dec);
            }
            serial_puts("\n  pitch: ");
            {
                char dec[32];
                int pos = 0;
                uint64_t p = fb->pitch;
                if (p == 0) { dec[pos++] = '0'; }
                else {
                    char rev[32]; int rp = 0;
                    while (p) { rev[rp++] = '0' + (p % 10); p /= 10; }
                    while (rp--) dec[pos++] = rev[rp];
                }
                dec[pos] = '\0';
                serial_puts(dec);
            }
            serial_puts("\n  bpp: ");
            {
                char dec[8];
                int pos = 0;
                uint64_t b = fb->bpp;
                if (b == 0) { dec[pos++] = '0'; }
                else {
                    char rev[8]; int rp = 0;
                    while (b) { rev[rp++] = '0' + (b % 10); b /= 10; }
                    while (rp--) dec[pos++] = rev[rp];
                }
                dec[pos] = '\0';
                serial_puts(dec);
            }

            // framebuffer format 字段在不同版本名可能不同（framebuffer_format 或 format），做兼容处理：
            uint64_t fb_format_u64 = 0;
            // 尝试直接读取 named member framebuffer_format/format，如果 header 提供了该成员就可以直接用。
            // 为兼容不同 header，我们也用按偏移读取（第6个 u64 字段）。
            uint64_t *fb_u64 = (uint64_t *)fb;
            fb_format_u64 = fb_u64[5]; // 0-based index: 0=address,1=pitch,2=width,3=height,4=bpp,5=format

            serial_puts("\n  format (raw u64): ");
            serial_puthex64(fb_format_u64);
            serial_puts("\n");

            // 将 Limine 返回的地址（可能是物理地址）转换为可访问虚拟地址
            void *virt_addr = PHYS_TO_VIRT((uint64_t)(uintptr_t)fb->address);

            serial_puts("  virt_addr: ");
            serial_puthex64((uint64_t)(uintptr_t)virt_addr);
            serial_puts("\n");

            // 初始化 framebuffer（graphics_init_raw 期望可访问的虚拟地址）
            graphics_init_raw((uint64_t)(uintptr_t)virt_addr,
                              (uint32_t)fb->width,
                              (uint32_t)fb->height,
                              (uint32_t)fb->pitch,
                              (uint8_t)fb->bpp,
                              (uint32_t)fb_format_u64);
        } else {
            serial_puts("framebuffer pointer is NULL\n");
        }
    } else {
        serial_puts("no framebuffers returned by limine\n");
    }

    if (!g_framebuffer) {
        serial_puts("g_framebuffer is NULL -> halting\n");
        while (1) { asm volatile ("hlt"); }
    }

    serial_puts("graphics initialized, clearing screen\n");
    
    uint32_t SCREEN_WIDTH = g_framebuffer->framebuffer_width;
    uint32_t SCREEN_HEIGHT = g_framebuffer->framebuffer_height;


    // 初始化文本颜色并清屏

    /*
    // 绘图与文本测试
    draw_test_square();

    // color bars
    for (uint32_t x = 0; x < g_framebuffer->framebuffer_width; x++) {
        uint32_t color;
        if (x < g_framebuffer->framebuffer_width / 6) color = COLOR_RED;
        else if (x < g_framebuffer->framebuffer_width / 3) color = COLOR_GREEN;
        else if (x < g_framebuffer->framebuffer_width / 2) color = COLOR_BLUE;
        else if (x < g_framebuffer->framebuffer_width * 2 / 3) color = COLOR_CYAN;
        else if (x < g_framebuffer->framebuffer_width * 5 / 6) color = COLOR_MAGENTA;
        else color = COLOR_YELLOW;
        for (uint32_t y = 0; y < 20; y++) {
            put_pixel(x, y, color);
        }
    }

    //fb_puts("Limine framebuffer active.\n");
    //fb_puts("Hello from kernel in graphics mode!\n");
    //clear_screen();
    */
    // 清除屏幕为 Win98 经典蓝色背景
    clear_screen(0x008080); // 经典的 Win98 蓝色背景
    
    // 绘制任务栏（底部深灰色条）
    draw_rect(0, g_framebuffer->framebuffer_height - 30,
              g_framebuffer->framebuffer_width, 30, 0xC0C0C0);
    
    // 绘制开始按钮（任务栏左侧）
    draw_rect(2, g_framebuffer->framebuffer_height - 28,
              80, 24, 0xC0C0C0);
    
    // 开始按钮边框效果（3D效果）
    // 上边和左边亮灰色边框
    for (int i = 0; i < 2; i++) {
        draw_rect(2 + i, g_framebuffer->framebuffer_height - 28 + i,
                  80 - 2*i, 1, 0xFFFFFF); // 上边框
        draw_rect(2 + i, g_framebuffer->framebuffer_height - 28 + i,
                  1, 24 - 2*i, 0xFFFFFF); // 左边框
    }
    
    // 下边和右边深灰色边框（阴影效果）
    for (int i = 0; i < 2; i++) {
        draw_rect(2 + i, g_framebuffer->framebuffer_height - 4 - i,
                  80 - 2*i, 1, 0x808080); // 下边框
        draw_rect(81 - i, g_framebuffer->framebuffer_height - 28 + i,
                  1, 24 - 2*i, 0x808080); // 右边框
    }
    
    print_string("Start", 30, SCREEN_HEIGHT-22, 0x000000);
    
    // 绘制窗口（Win98经典窗口样式）
    int win_width = 400;
    int win_height = 300;
    int win_x = (g_framebuffer->framebuffer_width - win_width) / 2;
    int win_y = (g_framebuffer->framebuffer_height - win_height - 30) / 2; // 留出任务栏空间
    
    // 窗口边框（深灰色）
    draw_rect(win_x, win_y, win_width, win_height, 0x808080);
    
    // 窗口背景（亮灰色）
    draw_rect(win_x + 2, win_y + 2, win_width - 4, win_height - 4, 0xC0C0C0);
    
    // 标题栏（深蓝色）
    draw_rect(win_x + 4, win_y + 4, win_width - 8, 30, 0x000080);
    
    // 标题栏文字区域
    draw_rect(win_x + 6, win_y + 6, win_width - 12, 26, 0x000080);
    
    // 标题栏上的窗口控制按钮（最小化、最大化、关闭）
    draw_rect(win_x + win_width - 85, win_y + 10, 20, 20, 0xC0C0C0); // 最小化
    draw_rect(win_x + win_width - 60, win_y + 10, 20, 20, 0xC0C0C0); // 最大化
    draw_rect(win_x + win_width - 35, win_y + 10, 20, 20, 0xC0C0C0); // 关闭
    
    // 绘制控制按钮的符号（简化版）
    // 关闭按钮上的"X"
    for (int i = 0; i < 10; i++) {
        put_pixel(win_x + win_width - 35 + 5 + i, win_y + 10 + 5 + i, COLOR_BLACK);
        put_pixel(win_x + win_width - 35 + 5 + i, win_y + 10 + 15 - i, COLOR_BLACK);
    }
    
    // 窗口内容区域（白色）
    draw_rect(win_x + 4, win_y + 38, win_width - 8, win_height - 46, COLOR_WHITE);
    
    // 在窗口中绘制一些Win98风格的图标/按钮
    // "My Computer"图标框
    draw_rect(win_x + 20, win_y + 50, 60, 80, 0xC0C0C0);
    // 图标框选中效果（浅蓝色）
    draw_rect(win_x + 22, win_y + 52, 56, 76, 0x00A0FF);
    
    // "Recycle Bin"图标框
    draw_rect(win_x + 100, win_y + 50, 60, 80, 0xC0C0C0);
    
    // 窗口中的按钮
    draw_rect(win_x + win_width - 120, win_y + win_height - 40, 80, 25, 0xC0C0C0);
    // 按钮边框
    draw_rect(win_x + win_width - 120, win_y + win_height - 40, 80, 1, 0xFFFFFF);
    draw_rect(win_x + win_width - 120, win_y + win_height - 40, 1, 25, 0xFFFFFF);
    draw_rect(win_x + win_width - 120, win_y + win_height - 16, 80, 1, 0x808080);
    draw_rect(win_x + win_width - 41, win_y + win_height - 40, 1, 25, 0x808080);
    
    // 任务栏上的系统托盘图标区域
    draw_rect(g_framebuffer->framebuffer_width - 100, g_framebuffer->framebuffer_height - 28,
              98, 24, 0xC0C0C0);
    
    // 系统时钟区域
    draw_rect(g_framebuffer->framebuffer_width - 80, g_framebuffer->framebuffer_height - 28,
              78, 24, 0xC0C0C0);
    
    serial_puts("Initializing SB16 sound card...\n");
    if (sb16_init()) {
        serial_puts("SB16 initialized successfully!\n");
        
        // 设置音量
        sb16_set_volume(0x3F, 0x3F);  // 最大音量
        
        // 播放测试音
        serial_puts("Playing test sound...\n");
        sb16_test_sound();
        
        // 等待测试音播放完成
        for (volatile int i = 0; i < 2000000; i++);
    }
    
    // main.c (添加以下内容到适当位置)

    // 在kmain函数中添加
    serial_puts("Initializing storage devices...\n");
    /*
    // 初始化SATA
    serial_puts("Initializing SATA/AHCI controller...\n");
    if (sata_init()) {
        serial_puts("SATA controller initialized successfully\n");
        // 测试SATA
        sata_test();
    } else {
        serial_puts("SATA initialization failed or no SATA controller found\n");
    }*/
    
    /*    // 初始化中断
    serial_puts("Initializing interrupt system...\n");
    idt_init();*/
    pic_init();
    
    // 初始化键盘
    serial_puts("Initializing keyboard...\n");
    keyboard_init();
    
    // 初始化鼠标
    serial_puts("Initializing mouse...\n");
    mouse_init();
    
    // 启用键盘和鼠标中断
    pic_enable_irq(1);  // 键盘 (IRQ1)
    pic_enable_irq(12); // 鼠标 (IRQ12)
    
    serial_puts("Keyboard and mouse initialized.\n");
    

    
    // 简单的鼠标指针（白色箭头）
    for (int i = 0; i < 10; i++) {
        put_pixel(g_framebuffer->framebuffer_width / 2 + i,
                  g_framebuffer->framebuffer_height / 2, COLOR_WHITE); // 水平线
        put_pixel(g_framebuffer->framebuffer_width / 2,
                  g_framebuffer->framebuffer_height / 2 + i, COLOR_WHITE); // 垂直线
    }
    
    // 延迟一下，让画面稳定
    //for (volatile int i = 0; i < 500000; i++) { /* busy wait */ }

    while (1) {
        int cursor_x = g_framebuffer->framebuffer_width / 2;
        int cursor_y = g_framebuffer->framebuffer_height / 2;
        int last_mouse_x = cursor_x;
        int last_mouse_y = cursor_y;
        
        // 绘制初始鼠标指针
        draw_cursor(cursor_x, cursor_y);
        
        while (1) {
            // 处理键盘输入
            if (keyboard_available()) {
                int key = get_key();
                char c = get_ascii_char(key);
                
                if (c != 0) {
                    // 显示按键
                    char buf[2] = {c, '\0'};
                    print_string(buf,10,10,0x000000);
                } else {
                    // 处理特殊键
                    switch (key) {
                        case KEY_ENTER:
                            print_string("\n",10,20,0x000000);
                            break;
                        case KEY_BACKSPACE:
                            // 处理退格键
                            break;
                        case KEY_UP:
                        case KEY_DOWN:
                        case KEY_LEFT:
                        case KEY_RIGHT:
                            // 方向键处理
                            break;
                    }
                }
            }
            
            // 处理鼠标移动
            int mouse_x, mouse_y;
            uint8_t mouse_buttons;
            get_mouse_state(&mouse_x, &mouse_y, &mouse_buttons);
            
            // 限制鼠标在屏幕范围内
            if (mouse_x < 0) mouse_x = 0;
            if (mouse_x >= g_framebuffer->framebuffer_width) 
                mouse_x = g_framebuffer->framebuffer_width - 1;
            if (mouse_y < 0) mouse_y = 0;
            if (mouse_y >= g_framebuffer->framebuffer_height)
                mouse_y = g_framebuffer->framebuffer_height - 1;
            
            // 更新鼠标指针位置
            if (mouse_x != last_mouse_x || mouse_y != last_mouse_y) {
                // 清除旧的鼠标指针
                clear_cursor(last_mouse_x, last_mouse_y);
                
                // 绘制新的鼠标指针
                draw_cursor(mouse_x, mouse_y);
                
                last_mouse_x = mouse_x;
                last_mouse_y = mouse_y;
            }
            
            // 处理鼠标点击
            if (mouse_buttons) {
                if (mouse_buttons & 0x01) {
                    // 左键点击
                    draw_cursor(mouse_x, mouse_y); // 重新绘制确保可见
                }
            }
            
            asm volatile("hlt");
        }
    }
}