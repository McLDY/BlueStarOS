#ifndef PMM_H
#define PMM_H

#include "cstd.h"

// UEFI 内存描述符结构
#pragma pack(push, 1)
typedef struct {
    uint32_t type;
    uint32_t reserved;
    uint64_t physical_start;
    uint64_t virtual_start;
    uint64_t number_of_pages;
    uint64_t attribute;
} efi_mem_desc_t;
#pragma pack(pop)

// PMM 核心接口
void pmm_init(void *mmap, size_t mmap_size, size_t desc_size);
void *pmm_alloc_page();
void *pmm_alloc_zpage();
void *pmm_alloc_blocks(size_t count);
void pmm_free_page(void *addr);
uint64_t pmm_get_total_memory();

#endif // PMM_H