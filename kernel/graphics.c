#include "graphics.h"
#include "kernel.h"

// 全局帧缓冲区信息指针
boot_params_t *g_framebuffer = NULL;


// 初始化图形系统
void graphics_init(boot_params_t *fb_info) {
    g_framebuffer = fb_info;
}

// 绘制单个像素
void put_pixel(uint32_t x, uint32_t y, uint32_t color) {
    // 快速边界检查
    if (!g_framebuffer || x >= g_framebuffer->framebuffer_width || y >= g_framebuffer->framebuffer_height) {
        return;
    }

    uint32_t *fb = (uint32_t*)g_framebuffer->framebuffer_addr;
    // 使用每行像素跨度 (Pitch / 4) 进行索引
    fb[y * (g_framebuffer->framebuffer_pitch >> 2) + x] = color;
}

// 清空屏幕
void clear_screen(uint32_t color) {
    if (!g_framebuffer || !g_framebuffer->framebuffer_addr) return;

    uint32_t *fb = (uint32_t*)g_framebuffer->framebuffer_addr;
    uint32_t pitch_in_pixels = g_framebuffer->framebuffer_pitch >> 2;
    uint32_t total_pixels = pitch_in_pixels * g_framebuffer->framebuffer_height;

    // 线性填充，编译器通常会将其优化为高效的 memset 变体
    for (uint32_t i = 0; i < total_pixels; i++) {
        fb[i] = color;
    }
}

// 绘制矩形
void draw_rect(uint32_t x, uint32_t y, uint32_t width, uint32_t height, uint32_t color) {
    if (!g_framebuffer) return;

    uint32_t screen_w = g_framebuffer->framebuffer_width;
    uint32_t screen_h = g_framebuffer->framebuffer_height;

    if (x >= screen_w || y >= screen_h) return;

    uint32_t end_x = x + width;
    uint32_t end_y = y + height;

    if (end_x > screen_w) end_x = screen_w;
    if (end_y > screen_h) end_y = screen_h;

    uint32_t *fb = (uint32_t*)g_framebuffer->framebuffer_addr;
    uint32_t pitch_pixels = g_framebuffer->framebuffer_pitch >> 2;

    for (uint32_t curr_y = y; curr_y < end_y; curr_y++) {
        uint32_t row_start = curr_y * pitch_pixels;
        for (uint32_t curr_x = x; curr_x < end_x; curr_x++) {
            fb[row_start + curr_x] = color;
        }
    }
}

// 通过串口输出调试信息
void print_fb_info() {
    if (!g_framebuffer) return;

    serial_puts("--- Framebuffer Debug Info ---\n");
    serial_puts("Address: 0x"); serial_puthex64(g_framebuffer->framebuffer_addr);
    serial_puts("\nResolution: "); 
    serial_putdec64(g_framebuffer->framebuffer_width);
    serial_puts("x");
    serial_putdec64(g_framebuffer->framebuffer_height);
    serial_puts("\nPitch (Bytes): ");
    serial_putdec64(g_framebuffer->framebuffer_pitch);
    serial_puts("\nBPP: ");
    serial_putdec64(g_framebuffer->framebuffer_bpp);
    serial_puts("\n----------------------------\n");
}