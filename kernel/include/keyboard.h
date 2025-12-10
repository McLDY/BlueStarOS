#ifndef KEYBOARD_H
#define KEYBOARD_H

#include <stdint.h>

// 键盘初始化
void keyboard_init(void);

// 键盘中断处理函数
void keyboard_handler(void);

// 获取按键（非阻塞）
int get_key(void);

// 检查是否有按键可用
int keyboard_available(void);

// 获取按键的ASCII字符（如果有）
char get_ascii_char(int scancode);

// 特殊键定义
#define KEY_ESC     0x01
#define KEY_BACKSPACE 0x0E
#define KEY_TAB     0x0F
#define KEY_ENTER   0x1C
#define KEY_LCTRL   0x1D
#define KEY_LSHIFT  0x2A
#define KEY_RSHIFT  0x36
#define KEY_LALT    0x38
#define KEY_CAPS    0x3A
#define KEY_F1      0x3B
#define KEY_F2      0x3C
#define KEY_F3      0x3D
#define KEY_F4      0x3E
#define KEY_F5      0x3F
#define KEY_F6      0x40
#define KEY_F7      0x41
#define KEY_F8      0x42
#define KEY_F9      0x43
#define KEY_F10     0x44
#define KEY_F11     0x57
#define KEY_F12     0x58
#define KEY_UP      0x48
#define KEY_DOWN    0x50
#define KEY_LEFT    0x4B
#define KEY_RIGHT   0x4D
#define KEY_DELETE  0x53

#endif