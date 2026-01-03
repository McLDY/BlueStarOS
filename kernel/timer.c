#include "kernel.h"

#define PIT_CHANNEL0 0x40
#define PIT_COMMAND  0x43
#define PIT_FREQ     1193182

static volatile uint64_t timer_ticks = 0;

// 时钟中断具体处理逻辑
void timer_callback(interrupt_frame_t* frame) {
    outb(0x20, 0x20);
    timer_ticks++;
}

// 初始化 PIT
void timer_init(uint32_t frequency) {
    // 注册到 IDT (IRQ0 对应中断向量 32)
    register_interrupt_handler(32, timer_callback);

    // 计算分频值
    uint32_t divisor = PIT_FREQ / frequency;

    // 设置 PIT 模式: 通道0, 左右字节访问, 模式3(方波), 二进制
    outb(PIT_COMMAND, 0x36);

    // 写入分频值
    outb(PIT_CHANNEL0, (uint8_t)(divisor & 0xFF));
    outb(PIT_CHANNEL0, (uint8_t)((divisor >> 8) & 0xFF));
    
    serial_puts("PIT: Timer initialized at ");
    serial_putdec64(frequency);
    serial_puts(" Hz\n");
}

// 获取当前滴答
uint64_t timer_get_ticks(void) {
    return timer_ticks;
}

// 毫秒级延迟函数
void sleep_ms(uint32_t ms) {
    uint64_t target = timer_ticks + ms;
    while (timer_ticks < target) {
        asm volatile("hlt"); // 挂起 CPU 等待中断，避免空转过热
    }
}