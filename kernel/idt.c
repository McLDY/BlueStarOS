#include "idt.h"
#include "io.h"
#include "serial.h"

#define PIC1_COMMAND 0x20
#define PIC2_COMMAND 0xA0
#define PIC_EOI      0x20

// 声明外部汇编桩表（由 interrupt.asm 提供）
extern void* isr_stub_table[];

// 16字节对齐是 64 位 IDT 的硬件要求
__attribute__((aligned(0x10))) static struct idt_entry idt[256];
static struct idt_ptr idtr;

// 私有处理函数表
static interrupt_handler_t interrupt_handlers[256];

// 注册函数
void register_interrupt_handler(uint8_t n, interrupt_handler_t handler) {
    interrupt_handlers[n] = handler;
}


// 设置 IDT 表项
void idt_set_gate(uint8_t vector, void* isr, uint8_t flags) {
    uint64_t addr = (uint64_t)isr;

    idt[vector].isr_low    = addr & 0xFFFF;             // 偏移量低 16 位
    idt[vector].kernel_cs  = 0x08;                      // GDT 中的内核代码段选择子
    idt[vector].ist        = 0;                         // 中断栈表索引 (0 表示不使用)
    idt[vector].attributes = flags;                     // 权限和类型
    idt[vector].isr_mid    = (addr >> 16) & 0xFFFF;     // 偏移量中 16 位
    idt[vector].isr_high   = (addr >> 32) & 0xFFFFFFFF; // 偏移量高 32 位
    idt[vector].reserved   = 0;                         // 必须置 0
}

void print_reg(const char* name, uint64_t val) {
    serial_puts(name);
    serial_puts(": ");
    serial_puthex64(val);
    serial_puts("  ");
}

// 统一分发器 (由 interrupt.asm 调用)
void idt_handler(interrupt_frame_t *frame) {
    interrupt_handler_t handler = interrupt_handlers[frame->int_no];

    if (handler != 0) {
        handler(frame);
    } else {
        // 发生未处理的中断/异常
        if (frame->int_no < 32) {
            serial_puts("\n================ EXCEPTION DUMP ================\n");
            serial_puts("EXCEPTION: ");
            serial_putdec64(frame->int_no);

            if (frame->int_no == 14) {
                uint64_t cr2;
                asm volatile("mov %%cr2, %0" : "=r"(cr2));
                serial_puts(" (PAGE FAULT)");
                serial_puts("\nFaulting Address (CR2): 0x");
                serial_puthex64(cr2);
            }
            
            serial_puts("\nError Code: ");
            serial_puthex64(frame->error_code);
            serial_puts("\n\n--- General Purpose Registers ---\n");

            // 打印通用寄存器 (取决于你的 interrupt_frame_t 成员定义)
            print_reg("RAX", frame->rax); print_reg("RBX", frame->rbx); print_reg("RCX", frame->rcx); 
            serial_puts("\n");
            print_reg("RDX", frame->rdx); print_reg("RSI", frame->rsi); print_reg("RDI", frame->rdi);
            serial_puts("\n");
            print_reg("RBP", frame->rbp); print_reg("R8 ", frame->r8);  print_reg("R9 ", frame->r9);
            serial_puts("\n");
            print_reg("R10", frame->r10); print_reg("R11", frame->r11); print_reg("R12", frame->r12);
            serial_puts("\n");
            print_reg("R13", frame->r13); print_reg("R14", frame->r14); print_reg("R15", frame->r15);
            
            serial_puts("\n\n--- CPU State ---\n");
            print_reg("RIP", frame->rip);    print_reg("CS ", frame->cs);
            serial_puts("\n");
            print_reg("RFLAGS", frame->rflags); print_reg("RSP", frame->rsp); print_reg("SS ", frame->ss);
            
            serial_puts("\n================================================\n");
            
            // 异常发生后，通常内核无法继续运行，进入死循环
            while(1) { asm("hlt"); }
        }
    }

    send_eoi(frame->int_no);
}

// 发送EOI信号
void send_eoi(int int_no) {
    // 只有硬件中断 (32-47) 需要发送 EOI
    if (int_no >= 32 && int_no <= 47) {
        // 如果是从片中断 (IRQ8-15)，也要发给从片
        if (int_no >= 40) {
            outb(0xA0, 0x20);
        }
        // 所有的硬件中断都要发给主片
        outb(0x20, 0x20);
    }
}

// 初始化 IDT 并加载到 CPU
void idt_init() {
    // 将 256 个向量全部指向对应的汇编桩
    for (int i = 0; i < 256; i++) {
        // 0x8E: P=1, DPL=00, Type=1110 (64-bit Interrupt Gate)
        idt_set_gate(i, isr_stub_table[i], 0x8E);
    }

    // 设置 IDTR 寄存器的值
    idtr.limit = sizeof(idt) - 1;
    idtr.base  = (uint64_t)&idt;

    // 加载 IDTR
    asm volatile ("lidt %0" : : "m"(idtr));
}