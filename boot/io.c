#include "cstd.h"
#include "io.h"

// 输出字节
void outb(unsigned short port, unsigned char val) {
    asm volatile("outb %0, %1" : : "a"(val), "dN"(port));
}

// 输入字节
unsigned char inb(unsigned short port) {
    unsigned char ret;
    asm volatile("inb %1, %0" : "=a"(ret) : "dN"(port));
    return ret;
}

// IO等待
void io_wait(void) {
    asm volatile("outb %%al, $0x80" : : "a"(0));
}
