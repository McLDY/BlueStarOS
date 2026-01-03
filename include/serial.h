#ifndef SERIAL_H
#define SERIAL_H

#include "cstd.h"

// 串口端口定义
#define COM1 0x3F8
#define COM2 0x2F8
#define COM3 0x3E8
#define COM4 0x2E8

// 默认串口端口
#define DEFAULT_SERIAL_PORT COM1

// 串口寄存器偏移
#define SERIAL_DATA_PORT(base)          (base)
#define SERIAL_FIFO_COMMAND_PORT(base)  (base + 2)
#define SERIAL_LINE_COMMAND_PORT(base)  (base + 3)
#define SERIAL_MODEM_COMMAND_PORT(base) (base + 4)
#define SERIAL_LINE_STATUS_PORT(base)   (base + 5)

// 基础函数（需要指定端口）
void serial_init(uint16_t port);
void serial_putc_port(uint16_t port, char c);
void serial_puts_port(uint16_t port, const char* s);
void serial_puthex8_port(uint16_t port, uint8_t value);
void serial_puthex16_port(uint16_t port, uint16_t value);
void serial_puthex32_port(uint16_t port, uint32_t value);
void serial_puthex64_port(uint16_t port, uint64_t value);
void serial_putdec32_port(uint16_t port, uint32_t value);
void serial_putdec64_port(uint16_t port, uint64_t value);
char serial_getc_port(uint16_t port);
int serial_received_port(uint16_t port);
int serial_is_transmit_empty_port(uint16_t port);

// 默认端口函数（使用DEFAULT_SERIAL_PORT）
static inline void serial_putc(char c) {
    serial_putc_port(DEFAULT_SERIAL_PORT, c);
}

static inline void serial_puts(const char* s) {
    serial_puts_port(DEFAULT_SERIAL_PORT, s);
}

static inline void serial_puthex8(uint8_t value) {
    serial_puthex8_port(DEFAULT_SERIAL_PORT, value);
}

static inline void serial_puthex16(uint16_t value) {
    serial_puthex16_port(DEFAULT_SERIAL_PORT, value);
}

static inline void serial_puthex32(uint32_t value) {
    serial_puthex32_port(DEFAULT_SERIAL_PORT, value);
}

static inline void serial_puthex64(uint64_t value) {
    serial_puthex64_port(DEFAULT_SERIAL_PORT, value);
}

static inline void serial_putdec32(uint32_t value) {
    serial_putdec32_port(DEFAULT_SERIAL_PORT, value);
}

static inline void serial_putdec64(uint64_t value) {
    serial_putdec64_port(DEFAULT_SERIAL_PORT, value);
}

static inline char serial_getc(void) {
    return serial_getc_port(DEFAULT_SERIAL_PORT);
}

static inline int serial_received(void) {
    return serial_received_port(DEFAULT_SERIAL_PORT);
}

static inline int serial_is_transmit_empty(void) {
    return serial_is_transmit_empty_port(DEFAULT_SERIAL_PORT);
}

// 简化的串口输出宏（使用默认端口）
#define SERIAL_OUT(s) serial_puts(s)
#define SERIAL_OUT_CHAR(c) serial_putc(c)
#define SERIAL_OUT_HEX8(v) serial_puthex8(v)
#define SERIAL_OUT_HEX16(v) serial_puthex16(v)
#define SERIAL_OUT_HEX32(v) serial_puthex32(v)
#define SERIAL_OUT_HEX64(v) serial_puthex64(v)
#define SERIAL_OUT_DEC32(v) serial_putdec32(v)
#define SERIAL_OUT_DEC64(v) serial_putdec64(v)

static int serial_is_transmit_empty(void);
static int serial_is_transmit_empty(void);
void serial_putc(char c);
void serial_puts(const char *s);
void serial_init(uint16_t port);

#endif // SERIAL_H