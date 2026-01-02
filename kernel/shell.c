#include "shell.h"
#include "serial.h"
//#include "drivers/fs/fat16.h"
#include <stdarg.h>  // 使用系统的 stdarg.h

// 简单的字符串函数实现
static void shell_memcpy(void *dest, const void *src, size_t n) {
    char *d = dest;
    const char *s = src;
    while (n--) *d++ = *s++;
}

static size_t shell_strlen(const char *s) {
    size_t len = 0;
    while (*s++) len++;
    return len;
}

static char *shell_strcpy(char *dest, const char *src) {
    char *d = dest;
    while ((*d++ = *src++));
    return dest;
}

static int shell_strcmp(const char *s1, const char *s2) {
    while (*s1 && (*s1 == *s2)) {
        s1++;
        s2++;
    }
    return *(unsigned char *)s1 - *(unsigned char *)s2;
}

// 数字转字符串函数
static void shell_itoa(int32_t value, char* str, int base) {
    char* ptr = str;
    char* ptr1 = str;
    char tmp_char;
    int32_t tmp_value;
    
    // 处理0
    if (value == 0) {
        *ptr++ = '0';
        *ptr = '\0';
        return;
    }
    
    // 处理负数
    if (value < 0 && base == 10) {
        *ptr++ = '-';
        value = -value;
    }
    
    // 转换数字
    while (value != 0) {
        tmp_value = value % base;
        *ptr++ = (tmp_value < 10) ? (tmp_value + '0') : (tmp_value - 10 + 'a');
        value /= base;
    }
    
    *ptr = '\0';
    
    // 反转字符串
    ptr--;
    while (ptr1 < ptr) {
        tmp_char = *ptr;
        *ptr-- = *ptr1;
        *ptr1++ = tmp_char;
    }
}

static void shell_utoa(uint32_t value, char* str, int base) {
    char* ptr = str;
    char* ptr1 = str;
    char tmp_char;
    uint32_t tmp_value;
    
    // 处理0
    if (value == 0) {
        *ptr++ = '0';
        *ptr = '\0';
        return;
    }
    
    // 转换数字
    while (value != 0) {
        tmp_value = value % base;
        *ptr++ = (tmp_value < 10) ? (tmp_value + '0') : (tmp_value - 10 + 'a');
        value /= base;
    }
    
    *ptr = '\0';
    
    // 反转字符串
    ptr--;
    while (ptr1 < ptr) {
        tmp_char = *ptr;
        *ptr-- = *ptr1;
        *ptr1++ = tmp_char;
    }
}

static void shell_utoa_hex(uint32_t value, char* str, int uppercase) {
    char* hex_digits = uppercase ? "0123456789ABCDEF" : "0123456789abcdef";
    char* ptr = str;
    
    // 处理0
    if (value == 0) {
        *ptr++ = '0';
        *ptr = '\0';
        return;
    }
    
    // 从最高位开始转换
    for (int i = 28; i >= 0; i -= 4) {
        uint8_t digit = (value >> i) & 0xF;
        if (digit != 0 || ptr != str) {
            *ptr++ = hex_digits[digit];
        }
    }
    
    // 如果全是0，确保至少有一个0
    if (ptr == str) {
        *ptr++ = '0';
    }
    
    *ptr = '\0';
}

// 使用我们的字符串函数
#define strcpy shell_strcpy
#define strcmp shell_strcmp
#define strlen shell_strlen

// Shell状态实例
static shell_state_t g_shell;
static term_output_func g_term_output = NULL;

// 字符串转无符号长整型（简易实现）
uint32_t shell_strtoul(const char *str, char **endptr, int base) {
    uint32_t result = 0;
    int digit;
    
    // 跳过空白字符
    while (*str == ' ' || *str == '\t') {
        str++;
    }
    
    // 确定进制
    if (base == 0) {
        if (*str == '0') {
            if (*(str + 1) == 'x' || *(str + 1) == 'X') {
                base = 16;
                str += 2;
            } else {
                base = 8;
                str++;
            }
        } else {
            base = 10;
        }
    }
    
    // 转换数字
    while (*str) {
        if (*str >= '0' && *str <= '9') {
            digit = *str - '0';
        } else if (*str >= 'a' && *str <= 'f') {
            digit = *str - 'a' + 10;
        } else if (*str >= 'A' && *str <= 'F') {
            digit = *str - 'A' + 10;
        } else {
            break;
        }
        
        if (digit >= base) {
            break;
        }
        
        result = result * base + digit;
        str++;
    }
    
    if (endptr) {
        *endptr = (char *)str;
    }
    
    return result;
}

// 辅助函数：格式化数字（处理宽度和对齐）
static void shell_format_number(char* dest, const char* num_str, 
                                int left_align, int width, int zero_pad, 
                                int negative) {
    int len = 0;
    while (num_str[len]) len++;
    if (negative) len++;  // 负号占用一个位置
    
    if (width > len) {
        int padding = width - len;
        if (left_align) {
            if (negative) *dest++ = '-';
            for (int i = 0; num_str[i]; i++) *dest++ = num_str[i];
            for (int i = 0; i < padding; i++) *dest++ = ' ';
        } else {
            char pad_char = zero_pad ? '0' : ' ';
            for (int i = 0; i < padding; i++) *dest++ = pad_char;
            if (negative) *dest++ = '-';
            for (int i = 0; num_str[i]; i++) *dest++ = num_str[i];
        }
    } else {
        if (negative) *dest++ = '-';
        for (int i = 0; num_str[i]; i++) *dest++ = num_str[i];
    }
    
    *dest = '\0';
}

// 完整的格式化输出函数
void shell_printf(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    
    char buffer[256];
    char num_buffer[32];
    char* buf_ptr = buffer;
    const char* p = fmt;
    
    while (*p && (buf_ptr - buffer) < 255) {
        if (*p != '%') {
            *buf_ptr++ = *p++;
            continue;
        }
        
        p++;  // 跳过 %
        
        // 解析格式标志
        int left_align = 0;
        int width = 0;
        int zero_pad = 0;
        
        // 解析标志
        while (*p == '-' || *p == '0') {
            if (*p == '-') left_align = 1;
            if (*p == '0') zero_pad = 1;
            p++;
        }
        
        // 解析宽度
        while (*p >= '0' && *p <= '9') {
            width = width * 10 + (*p - '0');
            p++;
        }
        
        // 解析类型
        switch (*p) {
            case 'd': {
                int32_t value = va_arg(args, int32_t);
                
                // 处理负数
                int negative = 0;
                if (value < 0) {
                    negative = 1;
                    value = -value;
                }
                
                shell_utoa((uint32_t)value, num_buffer, 10);
                shell_format_number(buf_ptr, num_buffer, left_align, width, zero_pad, negative);
                buf_ptr += strlen(buf_ptr);
                break;
            }
            
            case 'u': {
                uint32_t value = va_arg(args, uint32_t);
                shell_utoa(value, num_buffer, 10);
                shell_format_number(buf_ptr, num_buffer, left_align, width, zero_pad, 0);
                buf_ptr += strlen(buf_ptr);
                break;
            }
            
            case 'x':
            case 'X': {
                uint32_t value = va_arg(args, uint32_t);
                shell_utoa_hex(value, num_buffer, (*p == 'X'));
                shell_format_number(buf_ptr, num_buffer, left_align, width, zero_pad, 0);
                buf_ptr += strlen(buf_ptr);
                break;
            }
            
            case 's': {
                const char* str = va_arg(args, const char*);
                if (!str) str = "(null)";
                
                int len = 0;
                while (str[len]) len++;
                
                if (width > len) {
                    int padding = width - len;
                    if (left_align) {
                        for (int i = 0; i < len; i++) *buf_ptr++ = str[i];
                        for (int i = 0; i < padding; i++) *buf_ptr++ = ' ';
                    } else {
                        for (int i = 0; i < padding; i++) *buf_ptr++ = ' ';
                        for (int i = 0; i < len; i++) *buf_ptr++ = str[i];
                    }
                } else {
                    for (int i = 0; i < len; i++) *buf_ptr++ = str[i];
                }
                break;
            }
            
            case 'c': {
                char c = (char)va_arg(args, int);
                *buf_ptr++ = c;
                break;
            }
            
            case '%': {
                *buf_ptr++ = '%';
                break;
            }
            
            default: {
                *buf_ptr++ = '%';
                *buf_ptr++ = *p;
                break;
            }
        }
        
        p++;
    }
    
    *buf_ptr = '\0';
    va_end(args);
    shell_print(buffer);
}

// 声明所有命令处理函数
static void cmd_help(int argc, char *argv[]);
static void cmd_clear(int argc, char *argv[]);
static void cmd_echo(int argc, char *argv[]);
static void cmd_info(int argc, char *argv[]);
static void cmd_hexdump(int argc, char *argv[]);
static void cmd_reboot(int argc, char *argv[]);
static void cmd_shutdown(int argc, char *argv[]);
static void cmd_history(int argc, char *argv[]);
static void cmd_list_dir(int argc, char *argv[]);  // 添加声明

// 命令数组
static command_t g_commands[] = {
    {"help", "显示帮助信息", cmd_help},
    {"?", "显示帮助信息", cmd_help},
    {"clear", "清屏", cmd_clear},
    {"cls", "清屏", cmd_clear},
    {"echo", "回显参数", cmd_echo},
    {"info", "系统信息", cmd_info},
    {"hexdump", "内存查看: hexdump <addr> <len>", cmd_hexdump},
    {"reboot", "重启系统", cmd_reboot},
    {"shutdown", "关机", cmd_shutdown},
    {"history", "显示命令历史", cmd_history},
    {"ls", "列出目录", cmd_list_dir},
};

static const int g_command_count = sizeof(g_commands) / sizeof(g_commands[0]);

// 初始化Shell
void shell_init(void) {
    // 清空Shell状态
    g_shell.buffer_pos = 0;
    g_shell.history_count = 0;
    g_shell.history_index = 0;
    g_shell.history_pos = 0;
    g_shell.escape_state = 0;
    
    shell_print("\n");
    shell_print("========================================\n");
    shell_print("      MWOS Shell v1.0      \n");
    shell_print("========================================\n");
    shell_print("输入 'help' 查看可用命令\n");
    shell_print("支持上下键浏览历史记录 (^[[A / ^[[B)\n");
    shell_print_prompt();
}

// 设置图形终端输出回调
void shell_set_term_output(term_output_func func) {
    g_term_output = func;
}

// Shell输出
void shell_print(const char *str) {
    // 输出到串口
    serial_puts(str);
    
    // 如果设置了图形终端输出回调，也输出到图形终端
    if (g_term_output != NULL) {
        g_term_output(str);
    }
}

// 打印提示符
void shell_print_prompt(void) {
    shell_print("\nMWOS> ");
}

// 清除当前行（从光标位置到行尾）
void shell_clear_line(void) {
    // 使用ANSI转义序列清除从光标到行尾
    shell_print("\033[K");
}

// 刷新当前行（清除并重新显示）
void shell_refresh_line(void) {
    // 清除当前行
    shell_clear_line();
    
    // 回到行首（回车）
    shell_print("\r");
    
    // 重新打印提示符
    shell_print("MWOS> ");
    
    // 打印当前缓冲区内容
    if (g_shell.buffer_pos > 0) {
        char temp = g_shell.input_buffer[g_shell.buffer_pos];
        g_shell.input_buffer[g_shell.buffer_pos] = '\0';
        shell_print(g_shell.input_buffer);
        g_shell.input_buffer[g_shell.buffer_pos] = temp;
    }
}

// 保存命令到历史记录
void shell_save_to_history(const char *cmd) {
    // 跳过空命令和重复命令
    if (cmd[0] == '\0') return;
    
    // 检查是否与上一条命令相同
    if (g_shell.history_count > 0) {
        char *last_cmd = g_shell.history[(g_shell.history_pos + MAX_HISTORY - 1) % MAX_HISTORY];
        if (strcmp(cmd, last_cmd) == 0) {
            return;  // 与上一条命令相同，不保存
        }
    }
    
    // 保存命令
    strcpy(g_shell.history[g_shell.history_pos], cmd);
    g_shell.history_pos = (g_shell.history_pos + 1) % MAX_HISTORY;
    
    // 更新历史计数
    if (g_shell.history_count < MAX_HISTORY) {
        g_shell.history_count++;
    }
    
    // 重置历史索引
    g_shell.history_index = g_shell.history_count;
}

// 显示上一条历史命令
void shell_show_history_up(void) {
    if (g_shell.history_count == 0) return;
    
    // 如果当前不在历史记录中，保存当前输入
    if (g_shell.history_index == g_shell.history_count) {
        g_shell.input_buffer[g_shell.buffer_pos] = '\0';
        // 注意：这里不实际保存，只是作为临时存储
    }
    
    // 向上翻历史
    if (g_shell.history_index > 0) {
        g_shell.history_index--;
        
        // 获取历史命令
        uint32_t idx = (g_shell.history_pos + MAX_HISTORY - (g_shell.history_count - g_shell.history_index)) % MAX_HISTORY;
        const char *hist_cmd = g_shell.history[idx];
        
        // 复制到输入缓冲区
        strcpy(g_shell.input_buffer, hist_cmd);
        g_shell.buffer_pos = strlen(hist_cmd);
        
        // 刷新显示
        shell_refresh_line();
    }
}

// 显示下一条历史命令
void shell_show_history_down(void) {
    if (g_shell.history_count == 0) return;
    
    // 向下翻历史
    if (g_shell.history_index < g_shell.history_count) {
        g_shell.history_index++;
        
        if (g_shell.history_index == g_shell.history_count) {
            // 回到当前输入（如果有的话）
            g_shell.input_buffer[g_shell.buffer_pos] = '\0';
        } else {
            // 获取历史命令
            uint32_t idx = (g_shell.history_pos + MAX_HISTORY - (g_shell.history_count - g_shell.history_index)) % MAX_HISTORY;
            const char *hist_cmd = g_shell.history[idx];
            
            // 复制到输入缓冲区
            strcpy(g_shell.input_buffer, hist_cmd);
            g_shell.buffer_pos = strlen(hist_cmd);
        }
        
        // 刷新显示
        shell_refresh_line();
    }
}

// 处理单个字符输入（支持转义序列）
void shell_process_char(char c) {
    // 处理转义序列
    if (g_shell.escape_state == 1) {
        // 接收到ESC后的第一个字符
        if (c == '[') {
            g_shell.escape_state = 2;  // ESC[
        } else {
            g_shell.escape_state = 0;  // 不是有效的转义序列
        }
        return;
    } else if (g_shell.escape_state == 2) {
        // 接收到ESC[后的字符
        if (c == 'A') {
            // 上箭头键
            shell_show_history_up();
        } else if (c == 'B') {
            // 下箭头键
            shell_show_history_down();
        }
        g_shell.escape_state = 0;
        return;
    }
    
    // 正常字符处理
    switch (c) {
        case 0x1B:  // ESC键
            g_shell.escape_state = 1;
            break;
            
        case '\n':  // Enter键
        case '\r':
            if (g_shell.buffer_pos > 0) {
                g_shell.input_buffer[g_shell.buffer_pos] = '\0';
                shell_print("\n");
                
                // 保存到历史记录
                shell_save_to_history(g_shell.input_buffer);
                
                // 执行命令
                shell_execute_command(g_shell.input_buffer);
                
                // 清空缓冲区
                g_shell.buffer_pos = 0;
                
                // 重置历史索引
                g_shell.history_index = g_shell.history_count;
            } else {
                shell_print_prompt();
            }
            break;
            
        case '\b':  // Backspace键
            if (g_shell.buffer_pos > 0) {
                g_shell.buffer_pos--;
                shell_print("\b \b");  // 回退光标并清除字符
            }
            break;
            
        default:    // 普通字符
            if (g_shell.buffer_pos < MAX_CMD_LEN - 1 && c >= 32 && c <= 126) {
                g_shell.input_buffer[g_shell.buffer_pos++] = c;
                // 回显字符
                char temp[2] = {c, '\0'};
                shell_print(temp);
            }
            break;
    }
}

// 执行命令
void shell_execute_command(const char *cmd_line) {
    char *argv[MAX_ARGS];
    int argc = 0;
    char temp_line[MAX_CMD_LEN];
    char *temp_ptr = temp_line;
    
    // 复制命令行到临时缓冲区
    strcpy(temp_line, cmd_line);
    
    // 简单解析参数（按空格分割）
    argv[argc++] = temp_ptr;
    
    while (*temp_ptr && argc < MAX_ARGS) {
        if (*temp_ptr == ' ') {
            *temp_ptr = '\0';
            temp_ptr++;
            while (*temp_ptr == ' ') temp_ptr++; // 跳过多个空格
            
            if (*temp_ptr && *temp_ptr != ' ') {
                argv[argc++] = temp_ptr;
            }
        } else {
            temp_ptr++;
        }
    }
    
    if (argc == 0) {
        return;  // 空命令
    }
    
    // 查找并执行命令
    int found = 0;
    for (int i = 0; i < g_command_count; i++) {
        if (strcmp(argv[0], g_commands[i].name) == 0) {
            g_commands[i].handler(argc, argv);
            found = 1;
            break;
        }
    }
    
    if (!found) {
        shell_printf("命令未找到: '%s'\n", argv[0]);
        shell_print("输入 'help' 查看可用命令\n");
    }
    
    // 命令执行完后打印新的提示符
    shell_print_prompt();
}

// ============== 内置命令实现 ==============

void cmd_help(int argc, char *argv[]) {
    shell_printf("可用命令:\n");
    shell_printf("%s\n", "===========");
    
    for (int i = 0; i < g_command_count; i++) {
        shell_printf("  %-12s - %s\n", 
                    g_commands[i].name, 
                    g_commands[i].description);
    }
}

void cmd_clear(int argc, char *argv[]) {
    // 清屏序列
    shell_print("\033[2J\033[H");
}

void cmd_echo(int argc, char *argv[]) {
    for (int i = 1; i < argc; i++) {
        shell_print(argv[i]);
        if (i < argc - 1) {
            shell_print(" ");
        }
    }
    shell_print("\n");
}

void cmd_info(int argc, char *argv[]) {
    shell_printf("%s\n", "===== 系统信息 =====");
    shell_printf("内核版本: %s\n", "MWOS v1.0");
    shell_printf("命令数量: %u\n", g_command_count);
    shell_printf("历史记录: %u/%u\n", g_shell.history_count, MAX_HISTORY);
    shell_printf("%s\n", "===================");
}

// 新增history命令
void cmd_history(int argc, char *argv[]) {
    if (g_shell.history_count == 0) {
        shell_print("历史记录为空\n");
        return;
    }
    
    shell_printf("命令历史记录（最近%u条）:\n", g_shell.history_count);
    shell_printf("%s\n", "===================");
    
    for (uint32_t i = 0; i < g_shell.history_count; i++) {
        uint32_t idx = (g_shell.history_pos + MAX_HISTORY - g_shell.history_count + i) % MAX_HISTORY;
        shell_printf(" %3u: %s\n", i + 1, g_shell.history[idx]);
    }
}

void cmd_hexdump(int argc, char *argv[]) {
    if (argc < 3) {
        shell_print("用法: hexdump <地址> <长度>\n");
        shell_print("示例: hexdump 0x1000 64\n");
        return;
    }
    
    uint32_t addr = shell_strtoul(argv[1], NULL, 0);
    uint32_t len = shell_strtoul(argv[2], NULL, 0);
    
    if (len > 256) {
        len = 256;  // 限制最大长度
    }
    
    shell_printf("内存查看 @ %s, 长度: %u\n", argv[1], len);
    shell_print("（注意：内存读取功能需要实现）\n");
}

void cmd_reboot(int argc, char *argv[]) {
    shell_print("正在重启系统...\n");
    shell_print("（重启功能需要实现）\n");
}

void cmd_shutdown(int argc, char *argv[]) {
    shell_print("正在关机...\n");
    shell_print("（关机功能需要实现）\n");
}

#define MAX_FILES 50

void cmd_list_dir(int argc, char *argv[]){
    /*FileInfo file_list[MAX_FILES];
    
    //if (argc==0){
        int file_count = fat16_list("/", file_list, MAX_FILES);
        if (file_count < 0){
            shell_print("列出文件失败");
        } else {    
            for (int i = 0; i > file_count; i++) {
                shell_printf("%-12s %8u bytes ", file_list[i].name, file_list[i].size);
                
                // 显示文件属性
                if (file_list[i].is_dir) shell_print("[DIR]  ");
                if (file_list[i].is_hidden) shell_print("[HID] ");
                if (file_list[i].is_readonly) shell_print("[RO] ");
                if (file_list[i].is_system) shell_print("[SYS] ");
                
                // 显示日期时间（可选）
                if (file_list[i].modify_date != 0) {
                    // 解析FAT日期格式：yyyy yyym mmmd dddd
                    int year = (file_list[i].modify_date >> 9) + 1980;
                    int month = (file_list[i].modify_date >> 5) & 0x0F;
                    int day = file_list[i].modify_date & 0x1F;
                    shell_printf(" %04d-%02d-%02d", year, month, day);
                }
                
                shell_print("\n");
            }

        }
    }*/
    shell_printf("不支持的功能/Unsupported function");
}

/*void cmd_list_dir(int argc, char *argv[]){
    //shell_printf("\nRoot directory listing:\n");
    FileInfo file_list[32];
    int file_count = fat16_list("/", file_list, 32);
    
    if (file_count < 0) {
        shell_printf("Failed to list directory\n");
    } else {
        //shell_printf("Files found: ");
        serial_putdec64(file_count);
        shell_printf("\n");
        
        for (int i = 0; i < file_count; i++) {
            shell_printf("  ");
            shell_printf(file_list[i].name);
            if (file_list[i].is_dir) {
                shell_printf(" [DIR]");
            } else {
                shell_printf(" (");
                serial_putdec64(file_list[i].size);
                shell_printf(" bytes)");
            }
            shell_printf("\n");
        }
    }
    
}*/
