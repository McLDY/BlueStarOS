#include <stdint.h>
#include <stddef.h>
#include "mouse.h"
#include "io.h"

// PS/2鼠标端口
#define MOUSE_DATA_PORT 0x60
#define MOUSE_STATUS_PORT 0x64
#define MOUSE_COMMAND_PORT 0x64

// 鼠标命令
#define MOUSE_RESET 0xFF
#define MOUSE_RESEND 0xFE
#define MOUSE_SET_DEFAULTS 0xF6
#define MOUSE_DISABLE_PACKET_STREAMING 0xF5
#define MOUSE_ENABLE_PACKET_STREAMING 0xF4
#define MOUSE_SET_SAMPLE_RATE 0xF3
#define MOUSE_SET_RESOLUTION 0xE8
#define MOUSE_GET_DEVICE_ID 0xF2
#define MOUSE_SET_SCALING_1_1 0xE6
#define MOUSE_SET_SCALING_2_1 0xE7

// 鼠标状态
static int mouse_x = 0;
static int mouse_y = 0;
static uint8_t mouse_buttons = 0;

// 鼠标数据包处理状态
static uint8_t mouse_cycle = 0;
static int8_t mouse_byte[3];

// 等待可写
static void mouse_wait_write(void) {
    uint32_t timeout = 100000;
    while (timeout--) {
        if ((inb(MOUSE_STATUS_PORT) & 0x02) == 0) {
            return;
        }
    }
}

// 等待可读
static void mouse_wait_read(void) {
    uint32_t timeout = 100000;
    while (timeout--) {
        if (inb(MOUSE_STATUS_PORT) & 0x01) {
            return;
        }
    }
}

// 向鼠标发送命令
static void mouse_write(uint8_t command) {
    // 告诉鼠标控制器我们要发送命令给鼠标
    mouse_wait_write();
    outb(MOUSE_COMMAND_PORT, 0xD4);
    mouse_wait_write();
    outb(MOUSE_DATA_PORT, command);
    
    // 等待ACK
    mouse_wait_read();
    uint8_t ack = inb(MOUSE_DATA_PORT);
    // 这里可以检查ack是否为0xFA
}

// 从鼠标读取数据
static uint8_t mouse_read(void) {
    mouse_wait_read();
    return inb(MOUSE_DATA_PORT);
}

// 初始化鼠标
void mouse_init(void) {
    uint8_t status;
    
    // 启用辅助设备（鼠标）
    mouse_wait_write();
    outb(MOUSE_COMMAND_PORT, 0xA8);
    
    // 读取配置字节
    mouse_wait_write();
    outb(MOUSE_COMMAND_PORT, 0x20);
    mouse_wait_read();
    status = inb(MOUSE_DATA_PORT);
    
    // 设置位2（启用鼠标中断）
    status |= 0x02;
    // 设置位6（启用鼠标时钟）
    status |= 0x40;
    
    // 写回配置字节
    mouse_wait_write();
    outb(MOUSE_COMMAND_PORT, 0x60);
    mouse_wait_write();
    outb(MOUSE_DATA_PORT, status);
    
    // 启用数据包流模式
    mouse_write(MOUSE_SET_DEFAULTS);
    mouse_write(MOUSE_ENABLE_PACKET_STREAMING);
    
    // 设置采样率（100采样/秒）
    mouse_write(MOUSE_SET_SAMPLE_RATE);
    mouse_write(100);
    
    // 设置分辨率（4 counts/mm）
    mouse_write(MOUSE_SET_RESOLUTION);
    mouse_write(3); // 3 = 8 counts/mm
    
    // 重置位置和状态
    mouse_x = 0;
    mouse_y = 0;
    mouse_buttons = 0;
    mouse_cycle = 0;
}

// 鼠标中断处理函数
void mouse_handler(void) {
    // 读取鼠标数据
    uint8_t data = inb(MOUSE_DATA_PORT);
    
    // 处理数据包
    switch (mouse_cycle) {
        case 0:
            // 第一个字节：状态信息
            if ((data & 0x08) == 0x08) {
                mouse_byte[0] = data;
                mouse_cycle++;
            }
            break;
        case 1:
            // 第二个字节：X移动
            mouse_byte[1] = data;
            mouse_cycle++;
            break;
        case 2:
            // 第三个字节：Y移动
            mouse_byte[2] = data;
            
            // 解析数据包
            mouse_packet_t packet;
            packet.flags = mouse_byte[0];
            
            // 解析按钮状态
            mouse_buttons = 0;
            if (mouse_byte[0] & 0x01) mouse_buttons |= 0x01; // 左键
            if (mouse_byte[0] & 0x02) mouse_buttons |= 0x02; // 右键
            if (mouse_byte[0] & 0x04) mouse_buttons |= 0x04; // 中键
            
            // 解析X移动（考虑符号位）
            packet.x_movement = mouse_byte[1];
            if (mouse_byte[0] & 0x10) {
                packet.x_movement |= 0xFFFFFF00;
            }
            
            // 解析Y移动（考虑符号位，Y方向通常与屏幕方向相反）
            packet.y_movement = mouse_byte[2];
            if (mouse_byte[0] & 0x20) {
                packet.y_movement |= 0xFFFFFF00;
            }
            
            // 更新鼠标位置（反转Y方向）
            mouse_x += packet.x_movement;
            mouse_y -= packet.y_movement; // 反转Y
            
            // 限制鼠标位置在屏幕范围内
            // 注意：这里需要g_framebuffer的宽度和高度
            // 我们将在调用时进行限制
            
            // 重置循环
            mouse_cycle = 0;
            break;
    }
    
    // 确认中断处理完成
    outb(0x20, 0x20); // 主PIC
    outb(0xA0, 0x20); // 从PIC（鼠标连接到IRQ12，在从PIC上）
}

// 获取鼠标状态
void get_mouse_state(int *x, int *y, uint8_t *buttons) {
    if (x) *x = mouse_x;
    if (y) *y = mouse_y;
    if (buttons) *buttons = mouse_buttons;
}

// 重置鼠标位置
void mouse_reset_position(void) {
    mouse_x = 0;
    mouse_y = 0;
}