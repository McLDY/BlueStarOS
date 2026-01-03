#ifndef EFI_UTILS_H
#define EFI_UTILS_H

#include "efi.h"

// 工具函数声明
//void* memset(void* ptr, int value, UINTN num);
void set_image_handle(EFI_HANDLE handle);
void print_string(CHAR16 *str);
void print_error(CHAR16 *message, EFI_STATUS status);
void print_memory_map(void);
EFI_STATUS load_kernel(void);
EFI_STATUS boot_kernel(void);
void *find_kernel(void);

// 文件系统相关函数
EFI_STATUS open_root_directory(EFI_FILE_PROTOCOL **Root);
EFI_STATUS load_file(EFI_FILE_PROTOCOL *Root, CHAR16 *FileName, void **Buffer, UINTN *FileSize);

#endif