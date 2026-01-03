#include "cstd.h"
#include "serial.h"
#include "io.h"

// 初始化串口
void serial_init(uint16_t port) {
    outb(port + 1, 0x00);    // 禁用所有中断
    outb(port + 3, 0x80);    // 启用DLAB（设置波特率除数）
    outb(port + 0, 0x03);    // 设置除数为3（低位字节）38400波特率
    outb(port + 1, 0x00);    //                  （高位字节）
    outb(port + 3, 0x03);    // 8位，无奇偶校验，1个停止位
    outb(port + 2, 0xC7);    // 启用FIFO，清空，14字节阈值
    outb(port + 4, 0x0B);    // 启用IRQ，RTS/DSR设置
    serial_putc('\0');       // 输出空字符防止神秘bug
}

// 检查串口是否可接收数据
int serial_received_port(uint16_t port) {
    return inb(port + 5) & 1;
}

// 接收字符
char serial_getc_port(uint16_t port) {
    while (!serial_received_port(port));
    return inb(port);
}

// 检查串口是否可以发送数据
int serial_is_transmit_empty_port(uint16_t port) {
    return inb(port + 5) & 0x20;
}

// 发送字符
void serial_putc_port(uint16_t port, char c) {
    while (!serial_is_transmit_empty_port(port));
    outb(port, c);
}

// 发送字符串
void serial_puts_port(uint16_t port, const char* s) {
    while (*s) {
        if (*s == '\n') {
            serial_putc_port(port, '\r');
        }
        serial_putc_port(port, *s++);
    }
}

// 发送8位十六进制数
void serial_puthex8_port(uint16_t port, uint8_t value) {
    const char* hex = "0123456789ABCDEF";
    serial_putc_port(port, hex[(value >> 4) & 0x0F]);
    serial_putc_port(port, hex[value & 0x0F]);
}

// 发送16位十六进制数
void serial_puthex16_port(uint16_t port, uint16_t value) {
    serial_puthex8_port(port, (value >> 8) & 0xFF);
    serial_puthex8_port(port, value & 0xFF);
}

// 发送32位十六进制数
void serial_puthex32_port(uint16_t port, uint32_t value) {
    serial_puthex16_port(port, (value >> 16) & 0xFFFF);
    serial_puthex16_port(port, value & 0xFFFF);
}

// 发送64位十六进制数
void serial_puthex64_port(uint16_t port, uint64_t value) {
    serial_puthex32_port(port, (value >> 32) & 0xFFFFFFFF);
    serial_puthex32_port(port, value & 0xFFFFFFFF);
}

// 发送32位十进制数
void serial_putdec32_port(uint16_t port, uint32_t value) {
    if (value == 0) {
        serial_putc_port(port, '0');
        return;
    }
    
    char buffer[32];
    char* ptr = buffer + 31;
    *ptr = '\0';
    
    while (value > 0) {
        *--ptr = '0' + (value % 10);
        value /= 10;
    }
    
    serial_puts_port(port, ptr);
}

// 发送64位十进制数
void serial_putdec64_port(uint16_t port, uint64_t value) {
    if (value == 0) {
        serial_putc_port(port, '0');
        return;
    }
    
    char buffer[64];
    char* ptr = buffer + 63;
    *ptr = '\0';
    
    while (value > 0) {
        *--ptr = '0' + (value % 10);
        value /= 10;
    }
    
    serial_puts_port(port, ptr);
}
