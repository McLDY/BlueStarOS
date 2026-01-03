#ifndef GRAPHICS_H
#define GRAPHICS_H

#include "kernel.h"


#define COLOR_BLACK     0x00000000
#define COLOR_WHITE     0x00FFFFFF
#define COLOR_RED       0x00FF0000
#define COLOR_GREEN     0x0000FF00
#define COLOR_BLUE      0x000000FF
#define COLOR_CYAN      0x0000FFFF
#define COLOR_MAGENTA   0x00FF00FF
#define COLOR_YELLOW    0x0000FFFF

// 全局帧缓冲信息指针
extern boot_params_t *g_framebuffer;

// 函数声明
void graphics_init(boot_params_t *fb_info);
void put_pixel(uint32_t x, uint32_t y, uint32_t color);
void clear_screen(uint32_t color);
void draw_rect(uint32_t x, uint32_t y, uint32_t width, uint32_t height, uint32_t color);
void draw_test_square(void);
void print_fb_info();

#endif // GRAPHICS_H