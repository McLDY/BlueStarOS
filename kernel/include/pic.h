#ifndef PIC_H
#define PIC_H

#include <stdint.h>

// PIC端口
#define PIC1_COMMAND 0x20
#define PIC1_DATA 0x21
#define PIC2_COMMAND 0xA0
#define PIC2_DATA 0xA1

// PIC命令
#define PIC_EOI 0x20  // 中断结束命令

// 初始化PIC
void pic_init(void);

// 发送EOI（中断结束）信号
void pic_send_eoi(uint8_t irq);

// 启用IRQ线
void pic_enable_irq(uint8_t irq);

// 禁用IRQ线
void pic_disable_irq(uint8_t irq);

#endif