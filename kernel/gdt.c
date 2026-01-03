#include "gdt.h"
#include "cstd.h"

struct gdt_entry gdt[3];
struct gdt_ptr gdtr;

void gdt_set_gate(int num, uint32_t base, uint32_t limit, uint8_t access, uint8_t gran) {
    gdt[num].base_low    = (base & 0xFFFF);
    gdt[num].base_middle = (base >> 16) & 0xFF;
    gdt[num].base_high   = (base >> 24) & 0xFF;
    gdt[num].limit_low   = (limit & 0xFFFF);
    gdt[num].granularity = ((limit >> 16) & 0x0F) | (gran & 0xF0);
    gdt[num].access      = access;
}

void gdt_init() {
    gdtr.limit = (sizeof(struct gdt_entry) * 3) - 1;
    gdtr.base  = (uint64_t)&gdt;

    gdt_set_gate(0, 0, 0, 0, 0);                // Null
    gdt_set_gate(1, 0, 0xFFFFFFFF, 0x9A, 0xA0); // Kernel Code: 64-bit, Ring 0
    gdt_set_gate(2, 0, 0xFFFFFFFF, 0x92, 0x00); // Kernel Data

    asm volatile(
        "lgdt %0\n\t"
        "push $0x08\n\t"
        "lea 1f(%%rip), %%rax\n\t"
        "push %%rax\n\t"
        "lretq\n\t"
        "1:\n\t"
        "mov $0x10, %%ax\n\t"
        "mov %%ax, %%ds\n\t"
        "mov %%ax, %%es\n\t"
        "mov %%ax, %%ss\n\t"
        : : "m"(gdtr) : "rax"
    );
}