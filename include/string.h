// string.h
#ifndef STRING_H
#define STRING_H

#include <stddef.h>

// 字符串函数
size_t strlen(const char* str);
char* strcpy(char* dest, const char* src);
char* strcat(char* dest, const char* src);
int strcmp(const char* str1, const char* str2);
const char* strchr(const char* str, int ch);
const char* strrchr(const char* str, int ch);  // 新增
int strncmp(const char* str1, const char* str2, size_t n);  // 新增
char* strncpy(char* dest, const char* src, size_t n);  // 新增
char* strncat(char* dest, const char* src, size_t n);  // 新增

// 内存函数
int memcmp(const void* ptr1, const void* ptr2, size_t count);
void* memcpy(void* dest, const void* src, size_t count);
void* memset(void* dest, int ch, size_t count);

// 格式化输出
int sprintf(char* str, const char* format, ...);

// 新增：数字转字符串函数
char* itoa(int value, char* str, int base);
char* utoa(unsigned int value, char* str, int base);
char* lltoa(long long value, char* str, int base);
char* ulltoa(unsigned long long value, char* str, int base);

// 新增：字符串转数字函数
int atoi(const char* str);
long atol(const char* str);
long long atoll(const char* str);

#endif // STRING_H