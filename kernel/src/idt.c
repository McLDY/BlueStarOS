#include <stdint.h>
#include "idt.h"
#include "io.h"
#include "keyboard.h"
#include "mouse.h"

// IDT
static idt_entry_t idt[256];
static idt_ptr_t idtp;

// 外部汇编函数
extern void idt_load(idt_ptr_t*);

// 默认中断处理函数
static void default_handler(void) {
    // 发送EOI
    outb(0x20, 0x20);
}

// IRQ处理函数
static void irq0_handler(void);  // 定时器
static void irq1_handler(void);  // 键盘
static void irq12_handler(void); // 鼠标

// 设置IDT条目
void idt_set_gate(uint8_t num, uint32_t base, uint16_t sel, uint8_t flags) {
    idt[num].offset_low = base & 0xFFFF;
    idt[num].offset_high = (base >> 16) & 0xFFFF;
    idt[num].selector = sel;
    idt[num].zero = 0;
    idt[num].type_attr = flags;
}

// 初始化IDT
void idt_init(void) {
    idtp.limit = (sizeof(idt_entry_t) * 256) - 1;
    idtp.base = (uint32_t)&idt;
    
    // 清空IDT
    for (int i = 0; i < 256; i++) {
        idt_set_gate(i, (uint32_t)default_handler, 0x08, 0x8E);
    }
    
    // 设置IRQ处理程序
    // IRQ0 - 定时器
    idt_set_gate(32, (uint32_t)irq0_handler, 0x08, 0x8E);
    // IRQ1 - 键盘
    idt_set_gate(33, (uint32_t)irq1_handler, 0x08, 0x8E);
    // IRQ12 - 鼠标
    idt_set_gate(44, (uint32_t)irq12_handler, 0x08, 0x8E);
    
    // 加载IDT
    idt_load(&idtp);
}

// IRQ0处理函数（定时器）
static void irq0_handler(void) {
    // 定时器中断处理
    outb(0x20, 0x20); // 发送EOI
}

// IRQ1处理函数（键盘）
static void irq1_handler(void) {
    keyboard_handler();
}

// IRQ12处理函数（鼠标）
static void irq12_handler(void) {
    mouse_handler();
}