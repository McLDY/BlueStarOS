// string.h
#ifndef STRING_H
#define STRING_H

#include <stddef.h>

// 内存操作函数
void* memset(void* ptr, int value, size_t num);
void* memcpy(void* dest, const void* src, size_t num);
int memcmp(const void* ptr1, const void* ptr2, size_t num);
void* memmove(void* dest, const void* src, size_t num);

// 字符串操作函数
size_t strlen(const char* str);
char* strcpy(char* dest, const char* src);
char* strncpy(char* dest, const char* src, size_t n);
int strcmp(const char* s1, const char* s2);
int strncmp(const char* s1, const char* s2, size_t n);
char* strcat(char* dest, const char* src);
char* strncat(char* dest, const char* src, size_t n);
char* strchr(const char* s, int c);
char* strrchr(const char* s, int c);
char* strstr(const char* haystack, const char* needle);
char* strdup(const char* s);
char* strtok(char* str, const char* delim);
size_t strspn(const char* s, const char* accept);
size_t strcspn(const char* s, const char* reject);
char* strpbrk(const char* s, const char* accept);

// 数字转换函数
char* itoa(int value, char* str, int base);
char* utoa(unsigned int value, char* str, int base);
char* lltoa(long long value, char* str, int base);
char* ulltoa(unsigned long long value, char* str, int base);

#endif