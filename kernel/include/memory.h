// memory.h
#ifndef MEMORY_H
#define MEMORY_H

#include <stddef.h>
#include <stdint.h>

// 内存分配器结构
struct memory_pool {
    uint8_t* base;
    size_t size;
    size_t used;
};

// 函数声明
void* malloc(size_t size);
void free(void* ptr);
void* memset(void* ptr, int value, size_t num);
void* memcpy(void* dest, const void* src, size_t num);
int memcmp(const void* ptr1, const void* ptr2, size_t num);

#endif