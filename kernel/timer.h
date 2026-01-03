#ifndef TIMER_H
#define TIMER_H

#include "cstd.h"
#include "idt.h"

// PIT 硬件基本频率
#define PIT_BASE_FREQUENCY 1193182

// 初始化 PIT 定时器，设置每秒中断次数 (Hz)
void timer_init(uint32_t frequency);

// 获取当前滴答
uint64_t timer_get_ticks(void);

// 毫秒级延迟函数
void sleep_ms(uint32_t ms);

#endif