// shell.h
#ifndef SHELL_H
#define SHELL_H

#include "kernel.h"
#include "serial.h"

#define MAX_CMD_LEN 256
#define MAX_ARGS 16
#define MAX_CMDS 32
#define MAX_HISTORY 10      // 最多保存10条历史记录

// 命令处理函数类型
typedef void (*cmd_handler_t)(int argc, char *argv[]);

// 命令结构体
typedef struct {
    const char *name;        // 命令名
    const char *description; // 命令描述
    cmd_handler_t handler;   // 处理函数
} command_t;

// Shell状态
typedef struct {
    char input_buffer[MAX_CMD_LEN];  // 输入缓冲区
    uint32_t buffer_pos;             // 缓冲区当前位置
    
    // 历史记录相关
    char history[MAX_HISTORY][MAX_CMD_LEN];  // 历史命令
    uint32_t history_count;                  // 历史命令数量
    uint32_t history_index;                  // 当前显示的历史索引
    uint32_t history_pos;                    // 历史记录写入位置
    
    // 转义序列处理
    uint8_t escape_state;                    // 0=普通, 1=ESC, 2=ESC[
} shell_state_t;

// 图形终端输出回调函数类型
typedef void (*term_output_func)(const char*);

// Shell核心函数
void shell_init(void);
void shell_process_char(char c);
void shell_execute_command(const char *cmd_line);
void shell_print_prompt(void);
void shell_print(const char *str);
void shell_printf(const char* fmt, ...);  // 添加 printf 声明
void shell_set_term_output(term_output_func func);

// 历史记录函数
void shell_save_to_history(const char *cmd);
void shell_show_history_up(void);
void shell_show_history_down(void);
void shell_clear_line(void);
void shell_refresh_line(void);

// 工具函数
uint32_t shell_strtoul(const char *str, char **endptr, int base);

#endif // SHELL_H