#ifndef MOUSE_H
#define MOUSE_H

#include <stdint.h>

// 鼠标数据包结构
typedef struct {
    uint8_t flags;
    int8_t x_movement;
    int8_t y_movement;
    int8_t z_movement; // 滚轮，如果有的话
    uint8_t left_button:1;
    uint8_t right_button:1;
    uint8_t middle_button:1;
    uint8_t always1:1;
    uint8_t x_sign:1;
    uint8_t y_sign:1;
    uint8_t x_overflow:1;
    uint8_t y_overflow:1;
} mouse_packet_t;

// 鼠标初始化
void mouse_init(void);

// 鼠标中断处理函数
void mouse_handler(void);

// 获取鼠标状态
void get_mouse_state(int *x, int *y, uint8_t *buttons);

// 重置鼠标位置
void mouse_reset_position(void);

#endif