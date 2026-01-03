#include "drivers/pic.h"
#include "io.h"

#define PIC1_COMMAND 0x20
#define PIC1_DATA    0x21
#define PIC2_COMMAND 0xA0
#define PIC2_DATA    0xA1

#define ICW1_INIT    0x10
#define ICW1_ICW4    0x01
#define PIC_EOI      0x20

// 重映射 PIC 向量号
void pic_remap(uint8_t offset1, uint8_t offset2)
{
    // 保存原有的屏蔽位
    uint8_t m1 = inb(PIC1_DATA);
    uint8_t m2 = inb(PIC2_DATA);

    // 发送初始化命令 ICW1
    outb(PIC1_COMMAND, ICW1_INIT | ICW1_ICW4);
    io_wait();
    outb(PIC2_COMMAND, ICW1_INIT | ICW1_ICW4);
    io_wait();

    // 设置主片和从片的向量偏移量 ICW2
    outb(PIC1_DATA, offset1);
    io_wait();
    outb(PIC2_DATA, offset2);
    io_wait();

    // 设置主从片级联方式 ICW3
    outb(PIC1_DATA, 4); // 主片 IRQ2 连接从片
    io_wait();
    outb(PIC2_DATA, 2); // 从片标识号为 2
    io_wait();

    // 设置 8086 模式 ICW4
    outb(PIC1_DATA, 0x01); // 这里的 0x01 即 ICW4_8086
    io_wait();
    outb(PIC2_DATA, 0x01);
    io_wait();

    // 默认屏蔽所有硬件中断，后续通过特定函数开启
    outb(PIC1_DATA, 0xFF);
    outb(PIC2_DATA, 0xFF);
}

// 开启特定的 IRQ
void pic_enable_irq(uint8_t irq) {
    uint16_t port = (irq < 8) ? 0x21 : 0xA1;
    uint8_t mask = inb(port);
    outb(port, mask & ~(1 << (irq % 8)));
}

// 发送中断结束信号 EOI
void pic_send_eoi(uint8_t irq)
{
    if (irq >= 8)
    {
        outb(PIC2_COMMAND, PIC_EOI);
    }
    outb(PIC1_COMMAND, PIC_EOI);
}
