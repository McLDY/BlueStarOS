// string.c
#include "string.h"
#include "memory.h"
#include <stddef.h>

/*
// 简单的内存设置
void* memset(void* ptr, int value, size_t num) {
    unsigned char* p = (unsigned char*)ptr;
    while (num--) {
        *p++ = (unsigned char)value;
    }
    return ptr;
}

// 内存复制
void* memcpy(void* dest, const void* src, size_t num) {
    unsigned char* d = (unsigned char*)dest;
    const unsigned char* s = (const unsigned char*)src;
    
    while (num--) {
        *d++ = *s++;
    }
    
    return dest;
}

// 内存比较
int memcmp(const void* ptr1, const void* ptr2, size_t num) {
    const unsigned char* p1 = (unsigned char*)ptr1;
    const unsigned char* p2 = (unsigned char*)ptr2;
    
    while (num--) {
        if (*p1 != *p2) {
            return (*p1 < *p2) ? -1 : 1;
        }
        p1++;
        p2++;
    }
    
    return 0;
}
*/

// 移动内存（处理重叠）
void* memmove(void* dest, const void* src, size_t num) {
    unsigned char* d = (unsigned char*)dest;
    const unsigned char* s = (const unsigned char*)src;
    
    if (d < s) {
        // 从前往后复制
        while (num--) {
            *d++ = *s++;
        }
    } else {
        // 从后往前复制
        d += num;
        s += num;
        while (num--) {
            *--d = *--s;
        }
    }
    
    return dest;
}

// 字符串长度
size_t strlen(const char* str) {
    size_t len = 0;
    while (str[len]) {
        len++;
    }
    return len;
}

// 字符串复制
char* strcpy(char* dest, const char* src) {
    char* d = dest;
    while ((*d++ = *src++));
    return dest;
}

// 字符串复制（带长度限制）
char* strncpy(char* dest, const char* src, size_t n) {
    char* d = dest;
    while (n > 0 && *src) {
        *d++ = *src++;
        n--;
    }
    while (n > 0) {
        *d++ = '\0';
        n--;
    }
    return dest;
}

// 字符串比较
int strcmp(const char* s1, const char* s2) {
    while (*s1 && (*s1 == *s2)) {
        s1++;
        s2++;
    }
    return *(unsigned char*)s1 - *(unsigned char*)s2;
}

// 字符串比较（带长度限制）
int strncmp(const char* s1, const char* s2, size_t n) {
    while (n > 0 && *s1 && (*s1 == *s2)) {
        s1++;
        s2++;
        n--;
    }
    if (n == 0) return 0;
    return *(unsigned char*)s1 - *(unsigned char*)s2;
}

// 字符串连接
char* strcat(char* dest, const char* src) {
    char* d = dest;
    while (*d) d++;
    while ((*d++ = *src++));
    return dest;
}

// 字符串连接（带长度限制）
char* strncat(char* dest, const char* src, size_t n) {
    char* d = dest;
    while (*d) d++;
    while (n > 0 && *src) {
        *d++ = *src++;
        n--;
    }
    *d = '\0';
    return dest;
}

// 查找字符
char* strchr(const char* s, int c) {
    while (*s) {
        if (*s == (char)c) {
            return (char*)s;
        }
        s++;
    }
    return NULL;
}

// 反向查找字符
char* strrchr(const char* s, int c) {
    const char* last = NULL;
    while (*s) {
        if (*s == (char)c) {
            last = s;
        }
        s++;
    }
    return (char*)last;
}

// 查找子串
char* strstr(const char* haystack, const char* needle) {
    if (!*needle) return (char*)haystack;
    
    for (; *haystack; haystack++) {
        const char* h = haystack;
        const char* n = needle;
        
        while (*h && *n && (*h == *n)) {
            h++;
            n++;
        }
        
        if (!*n) return (char*)haystack;
    }
    
    return NULL;
}

// 计算字符串开头有多少字符在accept中
size_t strspn(const char* s, const char* accept) {
    size_t count = 0;
    while (*s && strchr(accept, *s)) {
        count++;
        s++;
    }
    return count;
}

// 计算字符串开头有多少字符不在reject中
size_t strcspn(const char* s, const char* reject) {
    size_t count = 0;
    while (*s && !strchr(reject, *s)) {
        count++;
        s++;
    }
    return count;
}

// 查找第一个出现在accept中的字符
char* strpbrk(const char* s, const char* accept) {
    while (*s) {
        if (strchr(accept, *s)) {
            return (char*)s;
        }
        s++;
    }
    return NULL;
}

// 字符串复制（分配内存）
char* strdup(const char* s) {
    size_t len = strlen(s) + 1;
    char* new = malloc(len);
    if (new) {
        memcpy(new, s, len);
    }
    return new;
}

// 字符串分割
static char* strtok_saved = NULL;

char* strtok(char* str, const char* delim) {
    if (str) {
        strtok_saved = str;
    } else if (!strtok_saved) {
        return NULL;
    }
    
    // 跳过前导分隔符
    strtok_saved += strspn(strtok_saved, delim);
    if (!*strtok_saved) {
        strtok_saved = NULL;
        return NULL;
    }
    
    char* token = strtok_saved;
    strtok_saved = strpbrk(strtok_saved, delim);
    
    if (strtok_saved) {
        *strtok_saved = '\0';
        strtok_saved++;
    } else {
        strtok_saved = NULL;
    }
    
    return token;
}

// 整数转字符串
char* itoa(int value, char* str, int base) {
    if (base < 2 || base > 36) {
        *str = '\0';
        return str;
    }
    
    char* ptr = str;
    char* ptr1 = str;
    char tmp_char;
    int tmp_value;
    int is_negative = 0;
    
    if (value < 0 && base == 10) {
        is_negative = 1;
        value = -value;
    }
    
    do {
        tmp_value = value;
        value /= base;
        int digit = tmp_value - value * base;
        *ptr++ = (digit < 10) ? ('0' + digit) : ('a' + digit - 10);
    } while (value);
    
    if (is_negative) {
        *ptr++ = '-';
    }
    
    *ptr-- = '\0';
    
    // 反转字符串
    while (ptr1 < ptr) {
        tmp_char = *ptr;
        *ptr-- = *ptr1;
        *ptr1++ = tmp_char;
    }
    
    return str;
}

// 无符号整数转字符串
char* utoa(unsigned int value, char* str, int base) {
    if (base < 2 || base > 36) {
        *str = '\0';
        return str;
    }
    
    char* ptr = str;
    char* ptr1 = str;
    char tmp_char;
    unsigned int tmp_value;
    
    do {
        tmp_value = value;
        value /= base;
        unsigned int digit = tmp_value - value * base;
        *ptr++ = (digit < 10) ? ('0' + digit) : ('a' + digit - 10);
    } while (value);
    
    *ptr-- = '\0';
    
    // 反转字符串
    while (ptr1 < ptr) {
        tmp_char = *ptr;
        *ptr-- = *ptr1;
        *ptr1++ = tmp_char;
    }
    
    return str;
}

// 64位整数转字符串
char* lltoa(long long value, char* str, int base) {
    if (base < 2 || base > 36) {
        *str = '\0';
        return str;
    }
    
    char* ptr = str;
    char* ptr1 = str;
    char tmp_char;
    long long tmp_value;
    int is_negative = 0;
    
    if (value < 0 && base == 10) {
        is_negative = 1;
        value = -value;
    }
    
    do {
        tmp_value = value;
        value /= base;
        long long digit = tmp_value - value * base;
        *ptr++ = (digit < 10) ? ('0' + digit) : ('a' + digit - 10);
    } while (value);
    
    if (is_negative) {
        *ptr++ = '-';
    }
    
    *ptr-- = '\0';
    
    // 反转字符串
    while (ptr1 < ptr) {
        tmp_char = *ptr;
        *ptr-- = *ptr1;
        *ptr1++ = tmp_char;
    }
    
    return str;
}

// 64位无符号整数转字符串
char* ulltoa(unsigned long long value, char* str, int base) {
    if (base < 2 || base > 36) {
        *str = '\0';
        return str;
    }
    
    char* ptr = str;
    char* ptr1 = str;
    char tmp_char;
    unsigned long long tmp_value;
    
    do {
        tmp_value = value;
        value /= base;
        unsigned long long digit = tmp_value - value * base;
        *ptr++ = (digit < 10) ? ('0' + digit) : ('a' + digit - 10);
    } while (value);
    
    *ptr-- = '\0';
    
    // 反转字符串
    while (ptr1 < ptr) {
        tmp_char = *ptr;
        *ptr-- = *ptr1;
        *ptr1++ = tmp_char;
    }
    
    return str;
}