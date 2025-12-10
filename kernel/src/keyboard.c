#include <stdint.h>
#include <stddef.h>
#include "keyboard.h"
#include "io.h"

// PS/2键盘端口
#define KEYBOARD_DATA_PORT 0x60
#define KEYBOARD_STATUS_PORT 0x64
#define KEYBOARD_COMMAND_PORT 0x64

// 键盘缓冲区大小
#define KEYBOARD_BUFFER_SIZE 128

// 键盘缓冲区
static uint8_t keyboard_buffer[KEYBOARD_BUFFER_SIZE];
static int keyboard_buffer_start = 0;
static int keyboard_buffer_end = 0;

// 当前状态
static uint8_t shift_pressed = 0;
static uint8_t ctrl_pressed = 0;
static uint8_t alt_pressed = 0;
static uint8_t caps_lock = 0;

// 扫描码到ASCII的映射表（无Shift）
static const char scan_code_table[] = {
    0, 0, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b',
    '\t', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',
    0, 'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`',
    0, '\\', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', 0,
    '*', 0, ' ', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, '7', '8', '9', '-',
    '4', '5', '6', '+', '1', '2', '3', '0', '.'
};

// 扫描码到ASCII的映射表（有Shift）
static const char scan_code_table_shift[] = {
    0, 0, '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+', '\b',
    '\t', 'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', '\n',
    0, 'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', '"', '~',
    0, '|', 'Z', 'X', 'C', 'V', 'B', 'N', 'M', '<', '>', '?', 0,
    '*', 0, ' ', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, '7', '8', '9', '-',
    '4', '5', '6', '+', '1', '2', '3', '0', '.'
};

// 等待键盘控制器准备好
static void keyboard_wait_write(void) {
    uint32_t timeout = 100000;
    while (timeout--) {
        if ((inb(KEYBOARD_STATUS_PORT) & 0x02) == 0) {
            return;
        }
    }
}

// 等待键盘有数据可读
static void keyboard_wait_read(void) {
    uint32_t timeout = 100000;
    while (timeout--) {
        if (inb(KEYBOARD_STATUS_PORT) & 0x01) {
            return;
        }
    }
}

// 发送命令到键盘控制器
static void keyboard_send_command(uint8_t command) {
    keyboard_wait_write();
    outb(KEYBOARD_COMMAND_PORT, command);
}

// 发送数据到键盘
static void keyboard_send_data(uint8_t data) {
    keyboard_wait_write();
    outb(KEYBOARD_DATA_PORT, data);
}

// 初始化键盘
void keyboard_init(void) {
    // 禁用键盘
    keyboard_send_command(0xAD);
    
    // 清空输出缓冲区
    keyboard_wait_read();
    inb(KEYBOARD_DATA_PORT);
    
    // 设置键盘控制器配置字节
    keyboard_send_command(0x20); // 读取配置字节
    keyboard_wait_read();
    uint8_t config = inb(KEYBOARD_DATA_PORT);
    config |= 0x01; // 启用第一个PS/2端口中断
    config &= ~0x10; // 禁用键盘时钟
    config &= ~0x20; // 禁用鼠标时钟
    
    // 写回配置字节
    keyboard_send_command(0x60);
    keyboard_wait_write();
    outb(KEYBOARD_DATA_PORT, config);
    
    // 启用键盘
    keyboard_send_command(0xAE);
    
    // 设置键盘扫描码集 (set 2)
    keyboard_send_data(0xF0);
    keyboard_send_data(0x02);
    
    // 启用键盘中断
    keyboard_send_data(0xF4);
    
    // 初始化缓冲区
    keyboard_buffer_start = 0;
    keyboard_buffer_end = 0;
    
    shift_pressed = 0;
    ctrl_pressed = 0;
    alt_pressed = 0;
    caps_lock = 0;
}

// 键盘中断处理函数
void keyboard_handler(void) {
    // 读取扫描码
    uint8_t scancode = inb(KEYBOARD_DATA_PORT);
    
    // 检查是否是键释放（最高位为1）
    int key_released = (scancode & 0x80);
    scancode &= 0x7F; // 清除释放标志
    
    // 更新修饰键状态
    switch (scancode) {
        case KEY_LSHIFT:
        case KEY_RSHIFT:
            shift_pressed = !key_released;
            break;
        case KEY_LCTRL:
            ctrl_pressed = !key_released;
            break;
        case KEY_LALT:
            alt_pressed = !key_released;
            break;
        case KEY_CAPS:
            if (!key_released) {
                caps_lock = !caps_lock;
            }
            break;
        default:
            // 如果不是修饰键，且是按键按下事件，保存到缓冲区
            if (!key_released) {
                int next = (keyboard_buffer_end + 1) % KEYBOARD_BUFFER_SIZE;
                if (next != keyboard_buffer_start) {
                    keyboard_buffer[keyboard_buffer_end] = scancode;
                    keyboard_buffer_end = next;
                }
            }
            break;
    }
    
    // 确认中断处理完成
    outb(0x20, 0x20);
}

// 获取按键（非阻塞）
int get_key(void) {
    if (keyboard_buffer_start == keyboard_buffer_end) {
        return 0; // 没有按键
    }
    
    int key = keyboard_buffer[keyboard_buffer_start];
    keyboard_buffer_start = (keyboard_buffer_start + 1) % KEYBOARD_BUFFER_SIZE;
    return key;
}

// 检查是否有按键可用
int keyboard_available(void) {
    return keyboard_buffer_start != keyboard_buffer_end;
}

// 获取按键的ASCII字符
char get_ascii_char(int scancode) {
    if (scancode < 0 || scancode >= sizeof(scan_code_table)) {
        return 0;
    }
    
    char c;
    int shift_active = shift_pressed ^ caps_lock; // 考虑Caps Lock
    
    if (scancode >= 'a' - 0x20 && scancode <= 'z' - 0x20) {
        // 字母键，考虑Caps Lock
        if (shift_active) {
            c = scan_code_table_shift[scancode];
        } else {
            c = scan_code_table[scancode];
        }
        if (caps_lock && !shift_pressed) {
            // Caps Lock单独处理（不改变Shift状态时）
            if (c >= 'a' && c <= 'z') c -= 32;
            else if (c >= 'A' && c <= 'Z') c += 32;
        }
    } else {
        // 其他键
        c = shift_active ? scan_code_table_shift[scancode] : scan_code_table[scancode];
    }
    
    return c;
}