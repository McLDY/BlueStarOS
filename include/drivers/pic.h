#ifndef PIC_H
#define PIC_H

#include <stdint.h>

#define PIC1          0x20      // 主片命令端口
#define PIC1_DATA     0x21      // 主片数据端口
#define PIC2          0xA0      // 从片命令端口
#define PIC2_DATA     0xA1      // 从片数据端口

#define ICW1_ICW4     0x01      // ICW4 (控制字) 标志
#define ICW1_INIT     0x10      // 初始化标志

void pic_remap(uint8_t offset1, uint8_t offset2);


#endif // PIC_H