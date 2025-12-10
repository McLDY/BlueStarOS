#include <stdint.h>
#include "pic.h"
#include "io.h"

// 初始化PIC
void pic_init(void) {
    // 保存当前掩码
    uint8_t mask1 = inb(PIC1_DATA);
    uint8_t mask2 = inb(PIC2_DATA);
    
    // 初始化主PIC
    outb(PIC1_COMMAND, 0x11); // ICW1: 初始化，边缘触发，级联
    outb(PIC1_DATA, 0x20);    // ICW2: 主PIC中断向量偏移0x20
    outb(PIC1_DATA, 0x04);    // ICW3: 主PIC的IRQ2连接从PIC
    outb(PIC1_DATA, 0x01);    // ICW4: 8086模式
    
    // 初始化从PIC
    outb(PIC2_COMMAND, 0x11); // ICW1: 初始化，边缘触发，级联
    outb(PIC2_DATA, 0x28);    // ICW2: 从PIC中断向量偏移0x28
    outb(PIC2_DATA, 0x02);    // ICW3: 从PIC连接到主PIC的IRQ2
    outb(PIC2_DATA, 0x01);    // ICW4: 8086模式
    
    // 恢复保存的掩码
    outb(PIC1_DATA, mask1);
    outb(PIC2_DATA, mask2);
}

// 发送EOI（中断结束）信号
void pic_send_eoi(uint8_t irq) {
    if (irq >= 8) {
        outb(PIC2_COMMAND, PIC_EOI);
    }
    outb(PIC1_COMMAND, PIC_EOI);
}

// 启用IRQ线
void pic_enable_irq(uint8_t irq) {
    uint16_t port;
    uint8_t value;
    
    if (irq < 8) {
        port = PIC1_DATA;
    } else {
        port = PIC2_DATA;
        irq -= 8;
    }
    
    value = inb(port) & ~(1 << irq);
    outb(port, value);
}

// 禁用IRQ线
void pic_disable_irq(uint8_t irq) {
    uint16_t port;
    uint8_t value;
    
    if (irq < 8) {
        port = PIC1_DATA;
    } else {
        port = PIC2_DATA;
        irq -= 8;
    }
    
    value = inb(port) | (1 << irq);
    outb(port, value);
}