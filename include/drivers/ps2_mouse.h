#ifndef PS2_MOUSE_H
#define PS2_MOUSE_H

#include <stdint.h>
#include "idt.h"

// PS/2 端口
#define MOUSE_DATA_PORT      0x60
#define MOUSE_STATUS_REG     0x64
#define MOUSE_COMMAND_REG    0x64

// 鼠标状态位定义 (Byte 0)
#define MOUSE_LEFT_BUTTON   0x01
#define MOUSE_RIGHT_BUTTON  0x02
#define MOUSE_MIDDLE_BUTTON 0x04
#define MOUSE_X_SIGN        0x10  // 1 = 负, 0 = 正
#define MOUSE_Y_SIGN        0x20  // 1 = 负, 0 = 正
#define MOUSE_X_OVERFLOW    0x40
#define MOUSE_Y_OVERFLOW    0x80

typedef struct {
    int32_t x;
    int32_t y;
    uint8_t left_button;
    uint8_t right_button;
    uint8_t middle_button;
} mouse_state_t;

static const char* mouse_cursor[] = {
    "X               ",
    "XX              ",
    "X.X             ",
    "X..X            ",
    "X...X           ",
    "X....X          ",
    "X.....X         ",
    "X......X        ",
    "X.......X       ",
    "X........X      ",
    "X.........X     ",
    "X..........X    ",
    "X...........X   ",
    "X............X  ",
    "X.......XXXXXX  ",
    "X..X....X       ",
    "X.X X....X      ",
    "XX  X....X      ",
    "X    X....X     ",
    "     X....X     ",
    "      X....X    ",
    "      X..XX     ",
    "       XX       ",
    "                "
};

// 初始化鼠标
void mouse_init(void);

// 获取当前鼠标状态
mouse_state_t* get_mouse_state(void);
void draw_mouse_cursor(int x, int y);
void save_mouse_background(int x, int y);
void restore_mouse_background();

#endif // PS2MOUSE_H