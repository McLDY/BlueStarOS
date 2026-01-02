/*
                   _ooOoo_
                  o8888888o
                  88" . "88
                  (| -_- |)
                  O\  =  /O
               ____/`---'\____
             .'  \\|     |//  `.
            /  \\|||  :  |||//  \
           /  _||||| -:- |||||-  \
           |   | \\\  -  /// |   |
           | \_|  ''\---/''  |   |
           \  .-\__  `-`  ___/-. /
         ___`. .'  /--.--\  `. . __
      ."" '<  `.___\_<|>_/___.'  >'"".
     | | :  `- \`.;`\ _ /`;.`/ - ` : | |
     \  \ `-.   \_ __\ /__ _/   .-` /  /
======`-.____`-.___\_____/___.-`____.-'======
                   `=---='
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
            佛祖保佑       永无BUG
*/

#include "string.h"
#include "stdbool.h"

// ============ 内存操作函数 ============
void* memset(void* dest, int ch, size_t count) {
    unsigned char* p = dest;
    while(count--) {
        *p++ = (unsigned char)ch;
    }
    return dest;
}

void* memcpy(void* dest, const void* src, size_t count) {
    unsigned char* d = dest;
    const unsigned char* s = src;
    while(count--) {
        *d++ = *s++;
    }
    return dest;
}

int memcmp(const void* ptr1, const void* ptr2, size_t count) {
    const unsigned char* p1 = ptr1;
    const unsigned char* p2 = ptr2;
    while(count--) {
        if (*p1 != *p2) {
            return *p1 - *p2;
        }
        p1++;
        p2++;
    }
    return 0;
}

// ============ 字符串函数 ============
size_t strlen(const char* str) {
    size_t len = 0;
    while(str[len]) {
        len++;
    }
    return len;
}

char* strcpy(char* dest, const char* src) {
    char* d = dest;
    while((*d++ = *src++));
    return dest;
}

char* strncpy(char* dest, const char* src, size_t n) {
    char* d = dest;
    while (n-- && (*d++ = *src++));
    while (n--) *d++ = '\0';
    return dest;
}

char* strcat(char* dest, const char* src) {
    char* d = dest;
    while(*d) d++;
    while((*d++ = *src++));
    return dest;
}

char* strncat(char* dest, const char* src, size_t n) {
    char* d = dest;
    while(*d) d++;
    while (n-- && (*d++ = *src++));
    *d = '\0';
    return dest;
}

int strcmp(const char* str1, const char* str2) {
    while(*str1 && (*str1 == *str2)) {
        str1++;
        str2++;
    }
    return *(const unsigned char*)str1 - *(const unsigned char*)str2;
}

int strncmp(const char* str1, const char* str2, size_t n) {
    if (n == 0) return 0;
    
    while (n-- && *str1 && (*str1 == *str2)) {
        str1++;
        str2++;
    }
    
    if (n == (size_t)-1) {
        return 0;
    }
    
    return *(const unsigned char*)str1 - *(const unsigned char*)str2;
}

const char* strchr(const char* str, int ch) {
    while(*str) {
        if (*str == (char)ch) {
            return str;
        }
        str++;
    }
    return NULL;
}

const char* strrchr(const char* str, int ch) {
    const char* last = NULL;
    while (*str) {
        if (*str == (char)ch) {
            last = str;
        }
        str++;
    }
    return last;
}

// ============ 数字转字符串函数 ============
static char* reverse_string(char* str, size_t length) {
    char* start = str;
    char* end = str + length - 1;
    char tmp;
    
    while (start < end) {
        tmp = *start;
        *start++ = *end;
        *end-- = tmp;
    }
    
    return str;
}

char* itoa(int value, char* str, int base) {
    char* ptr = str;
    bool negative = false;
    
    // 处理0
    if (value == 0) {
        *ptr++ = '0';
        *ptr = '\0';
        return str;
    }
    
    // 处理负数（只对十进制）
    if (value < 0 && base == 10) {
        negative = true;
        value = -value;
    }
    
    // 转换数字（反向）
    while (value != 0) {
        int rem = value % base;
        *ptr++ = (rem > 9) ? (rem - 10) + 'a' : rem + '0';
        value = value / base;
    }
    
    // 添加负号
    if (negative) {
        *ptr++ = '-';
    }
    
    *ptr = '\0';
    
    // 反转字符串
    return reverse_string(str, ptr - str);
}

char* utoa(unsigned int value, char* str, int base) {
    char* ptr = str;
    
    // 处理0
    if (value == 0) {
        *ptr++ = '0';
        *ptr = '\0';
        return str;
    }
    
    // 转换数字（反向）
    while (value != 0) {
        unsigned int rem = value % base;
        *ptr++ = (rem > 9) ? (rem - 10) + 'a' : rem + '0';
        value = value / base;
    }
    
    *ptr = '\0';
    
    // 反转字符串
    return reverse_string(str, ptr - str);
}

// 简化版本，暂不实现长整型
char* lltoa(long long value, char* str, int base) {
    return itoa((int)value, str, base);
}

char* ulltoa(unsigned long long value, char* str, int base) {
    return utoa((unsigned int)value, str, base);
}

// ============ 字符串转数字函数 ============
int atoi(const char* str) {
    int result = 0;
    int sign = 1;
    
    // 跳过空白字符
    while (*str == ' ' || *str == '\t' || *str == '\n') {
        str++;
    }
    
    // 处理符号
    if (*str == '-') {
        sign = -1;
        str++;
    } else if (*str == '+') {
        str++;
    }
    
    // 转换数字
    while (*str >= '0' && *str <= '9') {
        result = result * 10 + (*str - '0');
        str++;
    }
    
    return sign * result;
}

long atol(const char* str) {
    return (long)atoi(str);
}

long long atoll(const char* str) {
    return (long long)atoi(str);
}

// ============ 简单的 sprintf 实现 ============
// 支持基本格式：%d, %u, %x, %s, %c
#include <stdarg.h>

int sprintf(char* str, const char* format, ...) {
    va_list args;
    va_start(args, format);
    
    char* p = str;
    const char* f = format;
    char buffer[32];
    
    while (*f) {
        if (*f == '%') {
            f++;
            
            switch (*f) {
                case 'd': {
                    int value = va_arg(args, int);
                    itoa(value, buffer, 10);
                    char* b = buffer;
                    while (*b) *p++ = *b++;
                    break;
                }
                case 'u': {
                    unsigned int value = va_arg(args, unsigned int);
                    utoa(value, buffer, 10);
                    char* b = buffer;
                    while (*b) *p++ = *b++;
                    break;
                }
                case 'x': {
                    unsigned int value = va_arg(args, unsigned int);
                    utoa(value, buffer, 16);
                    char* b = buffer;
                    while (*b) *p++ = *b++;
                    break;
                }
                case 's': {
                    char* s = va_arg(args, char*);
                    while (*s) *p++ = *s++;
                    break;
                }
                case 'c': {
                    char c = (char)va_arg(args, int);
                    *p++ = c;
                    break;
                }
                case '%': {
                    *p++ = '%';
                    break;
                }
                default:
                    *p++ = '%';
                    *p++ = *f;
                    break;
            }
            f++;
        } else {
            *p++ = *f++;
        }
    }
    
    *p = '\0';
    va_end(args);
    
    return p - str;
}