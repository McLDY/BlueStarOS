#ifndef IDT_H
#define IDT_H

#include <stdint.h>

// 64位 IDT 描述符结构
struct idt_entry {
    uint16_t isr_low;    // 地址低16位 [0..15]
    uint16_t kernel_cs;  // 内核代码段选择子 (通常是 0x08)
    uint8_t  ist;        // 中断栈表索引 (通常设为 0)
    uint8_t  attributes; // 类型与权限位 (通常是 0x8E)
    uint16_t isr_mid;    // 地址中16位 [16..31]
    uint32_t isr_high;   // 地址高32位 [32..63]
    uint32_t reserved;   // 保留位，必须为 0
} __attribute__((packed));

// IDTR 寄存器结构
struct idt_ptr {
    uint16_t limit;      // IDT 表的大小减 1
    uint64_t base;       // IDT 表的起始线性地址
} __attribute__((packed));

// 中断上下文帧 (用于 idt_handler 参数)
typedef struct {
    uint64_t r15, r14, r13, r12, r11, r10, r9, r8;
    uint64_t rbp, rdi, rsi, rdx, rcx, rbx, rax;
    uint64_t int_no, error_code;
    uint64_t rip, cs, rflags, rsp, ss;
} __attribute__((packed)) interrupt_frame_t;

typedef void (*interrupt_handler_t)(interrupt_frame_t*);

// 函数声明
void idt_init();
void register_interrupt_handler(uint8_t n, interrupt_handler_t handler);
void send_eoi(int int_no);

#endif // IDT_H