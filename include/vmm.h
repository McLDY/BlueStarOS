#ifndef VMM_H
#define VMM_H

#include "stdint.h"

#define PAGE_SIZE 4096

// 页表项标志位
#define PTE_PRESENT  (1ULL << 0)
#define PTE_WRITABLE (1ULL << 1)
#define PTE_USER     (1ULL << 2)
#define PTE_NX       (1ULL << 63)

// 地址掩码，用于提取物理地址
#define PTE_ADDR_MASK 0x000FFFFFFFFFF000ULL

// 虚拟地址索引提取
#define PML4_IDX(addr) (((addr) >> 39) & 0x1FF)
#define PDPT_IDX(addr) (((addr) >> 30) & 0x1FF)
#define PD_IDX(addr)   (((addr) >> 21) & 0x1FF)
#define PT_IDX(addr)   (((addr) >> 12) & 0x1FF)

typedef uint64_t pt_entry_t;

void vmm_init();
void vmm_map(pt_entry_t* pml4, uint64_t virt, uint64_t phys, uint64_t flags);
void vmm_switch_table(pt_entry_t* pml4);

#endif // VMM_H