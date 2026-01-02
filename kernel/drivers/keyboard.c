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


#include "drivers/keyboard.h"
#include "io.h"
#include "serial.h"

// 键盘状态标志
static bool shift_pressed = false;
static bool caps_lock = false;

// 扫描码定义 (Set 1)
#define SCAN_LSHIFT 0x2A
#define SCAN_RSHIFT 0x36
#define SCAN_CAPS   0x3A

// 基础字符表 (未按下 Shift)
const char kbd_us_lower[128] = {
    0,  27, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b',
    '\t', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',
    0,  'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`', 0,
    '\\', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', 0, '*', 0, ' ', 0
};

// 按下 Shift 后的字符表
const char kbd_us_upper[128] = {
    0,  27, '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+', '\b',
    '\t', 'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', '\n',
    0,  'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', '\"', '~', 0,
    '|', 'Z', 'X', 'C', 'V', 'B', 'N', 'M', '<', '>', '?', 0, '*', 0, ' ', 0
};

/*void on_keyboard_pressed(uint8_t scancode, uint8_t final_char)
{
    // 输出到串口
    serial_puts("Key input: ");
    serial_putc(final_char);
    serial_puts("\n");
}

void keyboard_callback(interrupt_frame_t *frame) {
    uint8_t scancode = inb(0x60);

    // --- 处理按键松开 (Break Code, 0x80 以上) ---
    if (scancode & 0x80) {
        uint8_t released_scancode = scancode & 0x7F;
        if (released_scancode == SCAN_LSHIFT || released_scancode == SCAN_RSHIFT) {
            shift_pressed = false;
        }
        return;
    }

    // --- 处理按键按下 (Make Code) ---
    switch (scancode) {
        case SCAN_LSHIFT:
        case SCAN_RSHIFT:
            shift_pressed = true;
            break;
        case SCAN_CAPS:
            caps_lock = !caps_lock;
            break;
        default:
            if (scancode < 128) {
                char base_char = kbd_us_lower[scancode];
                if (base_char == 0) return; // 忽略不可打印键

                char final_char = base_char;

                // 判断是否是字母 (a-z)
                bool is_alpha = (base_char >= 'a' && base_char <= 'z');

                // 字母逻辑：Shift 与 CapsLock 互相抵消 (异或逻辑)
                if (is_alpha) {
                    if (shift_pressed ^ caps_lock) {
                        final_char = kbd_us_upper[scancode];
                    }
                } 
                // 非字母逻辑：仅受 Shift 影响 (如数字变符号)
                else {
                    if (shift_pressed) {
                        final_char = kbd_us_upper[scancode];
                    }
                }

                // 调用系统内核
                on_keyboard_pressed(scancode, final_char);
            }
            break;
    }
}

void keyboard_init() {
    register_interrupt_handler(0x21, keyboard_callback);
    // 取消 PIC 屏蔽 IRQ 1
    uint8_t mask = inb(0x21);
    outb(0x21, mask & 0xFD);
}*/