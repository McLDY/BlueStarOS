#ifndef FBTEXT_H
#define FBTEXT_H

#include <stdint.h>

// 字体
static const uint8_t font8x8_basic[96][8];

// 文本接口
void put_char(char c, uint32_t x, uint32_t y, uint32_t color);
void print_string(const char *str, uint32_t x, uint32_t y, uint32_t color);

#endif // FBTEXT_H