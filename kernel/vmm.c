#include "vmm.h"
#include "kernel.h"

extern boot_params_t kernel_params;

// 辅助函数：获取下一级页表，不存在则分配
static pt_entry_t* get_next_level(pt_entry_t* entry, uint64_t flags) {
    if (*entry & PTE_PRESENT) {
        return (pt_entry_t*)(*entry & PTE_ADDR_MASK);
    }
    
    // 分配新页表并清零
    void* new_table = pmm_alloc_zpage();
    if (!new_table) return NULL;
    
    // 设置当前项指向新表，增加 Present 和 Writable 属性
    *entry = (uint64_t)new_table | PTE_PRESENT | flags;
    return (pt_entry_t*)new_table;
}

void vmm_map(pt_entry_t* pml4, uint64_t virt, uint64_t phys, uint64_t flags) {
    // 逐级深入寻找或创建页表
    pt_entry_t* pdpt = get_next_level(&pml4[PML4_IDX(virt)], flags);
    pt_entry_t* pd   = get_next_level(&pdpt[PDPT_IDX(virt)], flags);
    pt_entry_t* pt   = get_next_level(&pd[PD_IDX(virt)], flags);
    
    // 在最后一级填写物理页地址
    pt[PT_IDX(virt)] = (phys & PTE_ADDR_MASK) | flags | PTE_PRESENT;
    
    // 刷新 TLB
    asm volatile("invlpg (%0)" : : "r"(virt) : "memory");
}

void vmm_switch_table(pt_entry_t* pml4) {
    // 加载 PML4 地址到 CR3 寄存器
    asm volatile("mov %0, %%cr3" : : "r"(pml4) : "memory");
}

void vmm_init() {
    pt_entry_t* kernel_pml4 = (pt_entry_t*)pmm_alloc_zpage();
    
    // 标识映射前 1GB (包含内核代码、数据、栈)
    for (uint64_t addr = 0; addr < 0x40000000; addr += PAGE_SIZE) {
        vmm_map(kernel_pml4, addr, addr, PTE_WRITABLE);
    }

    // 显存区域标识映射 (防止开启分页后黑屏)
    // TODO: 添加显存地址超过1GB的处理方案
    uint64_t fb_base = kernel_params.framebuffer_addr;
    uint64_t fb_size = kernel_params.framebuffer_size;
    for (uint64_t addr = fb_base; addr < fb_base + fb_size; addr += PAGE_SIZE) {
        vmm_map(kernel_pml4, addr, addr, PTE_WRITABLE);
    }
    
    vmm_switch_table(kernel_pml4);
}
