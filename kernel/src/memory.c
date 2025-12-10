// memory.c
#include "memory.h"

static uint8_t heap[16 * 1024 * 1024] __attribute__((aligned(8)));
static size_t heap_used = 0;

void* malloc(size_t size) {
    if (heap_used + size > sizeof(heap)) {
        return NULL;
    }
    
    void* ptr = &heap[heap_used];
    heap_used += size;
    return ptr;
}

void free(void* ptr) {
    // 简化：不做实际的释放
    (void)ptr;
}

void* memset(void* ptr, int value, size_t num) {
    uint8_t* p = (uint8_t*)ptr;
    while (num--) {
        *p++ = (uint8_t)value;
    }
    return ptr;
}

void* memcpy(void* dest, const void* src, size_t num) {
    uint8_t* d = (uint8_t*)dest;
    const uint8_t* s = (const uint8_t*)src;
    
    while (num--) {
        *d++ = *s++;
    }
    
    return dest;
}

int memcmp(const void* ptr1, const void* ptr2, size_t num) {
    const uint8_t* p1 = (const uint8_t*)ptr1;
    const uint8_t* p2 = (const uint8_t*)ptr2;
    
    while (num--) {
        if (*p1 != *p2) {
            return (*p1 < *p2) ? -1 : 1;
        }
        p1++;
        p2++;
    }
    
    return 0;
}