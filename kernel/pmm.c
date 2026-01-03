#include "kernel.h"
#include "stdint.h"

static uint8_t *bitmap;
static uint64_t total_pages;
static uint64_t free_pages;
static uint64_t bitmap_size;

#define SET_BIT(i) (bitmap[(i) / 8] |= (1 << ((i) % 8)))
#define CLEAR_BIT(i) (bitmap[(i) / 8] &= ~(1 << ((i) % 8)))
#define TEST_BIT(i) (bitmap[(i) / 8] & (1 << ((i) % 8)))

static bool pmm_is_address_valid(uint64_t addr)
{
    if (addr == 0 || addr == 0xFFFFFFFFFFFFFFFF)
        return false;
    if (addr > 0x0000008000000000)
        return false;
    return true;
}

void pmm_init(void *mmap, size_t mmap_size, size_t desc_size)
{
    if (desc_size == 0)
    {
        serial_puts("FATAL: desc_size is ZERO!\n");
        while (1);
    }

    uint64_t max_addr = 0;
    size_t desc_count = mmap_size / desc_size;
    uint8_t *mmap_ptr = (uint8_t *)mmap;

    for (size_t i = 0; i < desc_count; i++)
    {
        efi_mem_desc_t *d = (efi_mem_desc_t *)(mmap_ptr + (i * desc_size));
        uint64_t end = d->physical_start + (d->number_of_pages * 4096);
        if (end > max_addr)
            max_addr = end;
    }

    total_pages = max_addr / 4096;
    bitmap_size = (total_pages + 7) / 8;
    bitmap = NULL;

    for (size_t i = 0; i < desc_count; i++)
    {
        efi_mem_desc_t *d = (efi_mem_desc_t *)(mmap_ptr + (i * desc_size));
        // 类型 7: EfiConventionalMemory
        if (d->type == 7 && (d->number_of_pages * 4096) >= bitmap_size)
        {
            if (d->physical_start >= 0x1000000)
            {
                bitmap = (uint8_t *)d->physical_start;
                break;
            }
        }
    }

    memset(bitmap, 0xFF, bitmap_size);
    free_pages = 0;

    for (size_t i = 0; i < desc_count; i++)
    {
        efi_mem_desc_t *d = (efi_mem_desc_t *)(mmap_ptr + (i * desc_size));
        // 释放常规内存、启动服务代码/数据区
        if (d->type == 7 || d->type == 3 || d->type == 4)
        {
            uint64_t start_page = d->physical_start / 4096;
            uint64_t end_page = start_page + d->number_of_pages;
            if (end_page > total_pages)
                end_page = total_pages;
            
            if (start_page >= end_page)
                continue;
            
            // 优化：批量处理完整字节
            uint64_t start_byte = start_page / 8;
            uint64_t end_byte = (end_page - 1) / 8;
            
            // 处理开头不完整的字节
            if (start_page % 8 != 0)
            {
                uint8_t mask = 0xFF << (start_page % 8);
                bitmap[start_byte] &= ~mask;
                start_byte++;
            }
            
            // 批量处理完整字节
            if (start_byte < end_byte)
            {
                memset(&bitmap[start_byte], 0x00, end_byte - start_byte);
            }
            
            // 处理结尾不完整的字节
            if (end_byte == start_byte - 1) // 只有部分字节需要处理
            {
                uint8_t mask = 0xFF >> (8 - (end_page % 8));
                if (end_page % 8 != 0)
                    bitmap[end_byte] &= ~mask;
            }
            
            // 更新空闲页计数
            free_pages += (end_page - start_page);
        }
    }

    // 保护前 16MB 内核区
    uint64_t kernel_safety_pages = 0x1000000 / 4096;
    for (uint64_t i = 0; i < kernel_safety_pages && i < total_pages; i++)
    {
        if (!TEST_BIT(i))
        {
            SET_BIT(i);
            free_pages--;
        }
    }

    // 保护位图自身
    uint64_t bitmap_start_page = (uint64_t)bitmap / 4096;
    uint64_t bitmap_pages = (bitmap_size + 4095) / 4096;
    for (uint64_t i = 0; i < bitmap_pages; i++)
    {
        if (bitmap_start_page + i < total_pages && !TEST_BIT(bitmap_start_page + i))
        {
            SET_BIT(bitmap_start_page + i);
            free_pages--;
        }
    }
}

void *pmm_alloc_page()
{
    uint64_t start_search = 0x1000000 / 4096;
    uint64_t start_byte = start_search / 8;
    uint64_t end_byte = total_pages / 8;
    
    // 优化：先按字节搜索，再按位搜索
    for (uint64_t i = start_byte; i < end_byte; i++)
    {
        if (bitmap[i] != 0xFF) // 该字节中存在空闲位
        {
            // 找到第一个空闲位
            for (uint8_t j = 0; j < 8; j++)
            {
                uint64_t page_index = i * 8 + j;
                if (page_index >= total_pages) break;
                if (!TEST_BIT(page_index))
                {
                    uint64_t addr = page_index * 4096;
                    if (!pmm_is_address_valid(addr))
                        continue;
                    SET_BIT(page_index);
                    free_pages--;
                    return (void *)addr;
                }
            }
        }
    }
    
    // 处理剩余的位
    uint64_t last_byte = total_pages / 8;
    for (uint64_t i = last_byte * 8; i < total_pages; i++)
    {
        if (!TEST_BIT(i))
        {
            uint64_t addr = i * 4096;
            if (!pmm_is_address_valid(addr))
                continue;
            SET_BIT(i);
            free_pages--;
            return (void *)addr;
        }
    }
    
    return NULL;
}

// 分配并清零页面，用于页表创建
void *pmm_alloc_zpage()
{
    void *addr = pmm_alloc_page();
    if (addr)
        memset(addr, 0, 4096);
    return addr;
}

// 分配多块连续物理页，用于双缓冲等大内存需求
void *pmm_alloc_blocks(size_t count)
{
    if (count == 0) return NULL;
    if (count > free_pages) return NULL;
    
    uint64_t start_search = 0x1000000 / 4096;
    uint64_t consecutive = 0;
    uint64_t start_index = 0;

    // 优化：使用快速位搜索
    for (uint64_t i = start_search; i < total_pages; i++)
    {
        if (!TEST_BIT(i))
        {
            if (consecutive == 0)
                start_index = i;
            consecutive++;
            
            if (consecutive == count)
            {
                // 批量设置位
                uint64_t end_index = start_index + count;
                uint64_t start_byte = start_index / 8;
                uint64_t end_byte = (end_index - 1) / 8;
                
                // 处理开头不完整的字节
                if (start_index % 8 != 0)
                {
                    uint8_t mask = 0xFF << (start_index % 8);
                    bitmap[start_byte] |= mask;
                    start_byte++;
                }
                
                // 批量处理完整字节
                if (start_byte < end_byte)
                {
                    memset(&bitmap[start_byte], 0xFF, end_byte - start_byte);
                }
                
                // 处理结尾不完整的字节
                if (end_index % 8 != 0)
                {
                    uint8_t mask = 0xFF >> (8 - (end_index % 8));
                    bitmap[end_byte] |= mask;
                }
                
                free_pages -= count;
                return (void *)(start_index * 4096);
            }
        }
        else
        {
            consecutive = 0;
        }
    }
    return NULL;
}

void pmm_free_page(void *addr)
{
    uint64_t page_index = (uint64_t)addr / 4096;
    if (page_index >= (0x1000000 / 4096) && page_index < total_pages)
    {
        if (TEST_BIT(page_index))
        {
            CLEAR_BIT(page_index);
            free_pages++;
        }
    }
}

// 释放多块连续物理页
void pmm_free_blocks(void *addr, size_t count)
{
    uint64_t start_index = (uint64_t)addr / 4096;
    for (uint64_t i = 0; i < count; i++)
    {
        pmm_free_page((void *)((start_index + i) * 4096));
    }
}