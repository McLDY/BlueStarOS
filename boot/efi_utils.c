#include "bootloader.h"
#include "serial.h"

// 简单的memset实现，避免头文件冲突
void *memset(void *s, int c, unsigned long n) {
    unsigned char *p = s;
    while (n--) {
        *p++ = (unsigned char)c;
    }
    return s;
}

#ifndef EFI_PAGE_SIZE
#define EFI_PAGE_SIZE 4096
#endif
#define KERNEL_STACK_SIZE (64 * 1024)
#define PAGES_FOR_STACK (KERNEL_STACK_SIZE / 4096)

// 全局变量
EFI_HANDLE gImageHandle = NULL;
static void *gKernelBase = NULL;
static UINTN gKernelSize = 0;

// 设置图像句柄
void set_image_handle(EFI_HANDLE handle)
{
    gImageHandle = handle;
}

// 打印字符串
void print_string(CHAR16 *str)
{
    gST->ConOut->OutputString(gST->ConOut, str);
}

// 打印错误信息
void print_error(CHAR16 *message, EFI_STATUS status)
{
    print_string(message);
    print_string(L": ");

    CHAR16 buffer[32];
    for (int i = 0; i < 16; i++)
    {
        UINT8 nibble = (status >> (60 - i * 4)) & 0xF;
        buffer[i] = (nibble < 10) ? (L'0' + nibble) : (L'A' + nibble - 10);
    }
    buffer[16] = L'\r';
    buffer[17] = L'\n';
    buffer[18] = L'\0';

    print_string(buffer);
}

// 简单的宽字符字符串格式化函数
UINTN wsprintf(CHAR16 *buffer, const CHAR16 *format, ...)
{
    UINTN count = 0;
    const CHAR16 *p = format;

    // 获取可变参数
    UINTN *arg_ptr = (UINTN *)(&format + 1);

    while (*p)
    {
        if (*p == L'%')
        {
            p++;
            switch (*p)
            {
            case L'd':
            {
                // 处理整数参数
                UINT32 value = (UINT32)*arg_ptr++;
                UINT32 temp = value;
                UINT32 digits = 0;

                // 计算数字位数
                do
                {
                    digits++;
                    temp /= 10;
                } while (temp > 0);

                // 处理值为0的情况
                if (digits == 0)
                    digits = 1;

                // 写入数字
                temp = value;
                for (UINT32 i = 0; i < digits; i++)
                {
                    buffer[count + digits - 1 - i] = L'0' + (temp % 10);
                    temp /= 10;
                }
                count += digits;
                break;
            }
            case L'x':
            {
                // 处理十六进制参数
                UINT32 value = (UINT32)*arg_ptr++;
                UINT32 temp = value;
                UINT32 digits = 0;

                // 计算十六进制位数
                do
                {
                    digits++;
                    temp >>= 4;
                } while (temp > 0);

                // 处理值为0的情况
                if (digits == 0)
                    digits = 1;

                // 写入十六进制数字
                temp = value;
                for (UINT32 i = 0; i < digits; i++)
                {
                    UINT8 nibble = (temp >> ((digits - 1 - i) * 4)) & 0xF;
                    buffer[count + i] = (nibble < 10) ? (L'0' + nibble) : (L'A' + nibble - 10);
                }
                count += digits;
                break;
            }
            default:
                buffer[count++] = L'%';
                buffer[count++] = *p;
                break;
            }
            p++;
        }
        else
        {
            buffer[count++] = *p++;
        }
    }

    buffer[count] = L'\0';
    return count;
}

// 打开根目录
EFI_STATUS open_root_directory(EFI_FILE_PROTOCOL **Root)
{
    EFI_STATUS status;
    EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *FileSystem;
    EFI_GUID FileSystemGuid = {0x0964e5b22, 0x6459, 0x11d2, {0x8e, 0x39, 0x00, 0xa0, 0xc9, 0x69, 0x72, 0x3b}};

    // 定位文件系统协议
    status = gST->BootServices->LocateProtocol(&FileSystemGuid, NULL, (VOID **)&FileSystem);
    if (EFI_ERROR(status))
    {
        print_string(L"[X] Locate file system protocol\r\n");
    }

    print_string(L"[OK] Found file system protocol\r\n");

    // 打开根目录
    status = FileSystem->OpenVolume(FileSystem, Root);
    if (EFI_ERROR(status))
    {
        print_string(L"[X] Open root directory\r\n");
    }

    print_string(L"[OK] Open root directory\r\n");
    return EFI_SUCCESS;
}

// 加载文件
EFI_STATUS load_file(EFI_FILE_PROTOCOL *Root, CHAR16 *FileName, void **Buffer, UINTN *FileSize)
{
    EFI_STATUS status;
    EFI_FILE_PROTOCOL *File;
    EFI_FILE_INFO *FileInfo;
    EFI_GUID FileInfoGuid = EFI_FILE_INFO_ID;
    UINTN InfoSize = 0;

    // 打开文件
    status = Root->Open(Root, &File, FileName, EFI_FILE_MODE_READ, 0);
    if (EFI_ERROR(status))
    {
        print_string(L"[X] Open file: ");
        print_string(FileName);
        print_string(L"\r\n");
    }
    else
    {
        print_string(L"[OK] Open file: ");
        print_string(FileName);
        print_string(L"\r\n");
    }

    // 获取文件信息
    status = File->GetInfo(File, &FileInfoGuid, &InfoSize, NULL);
    if (status != EFI_BUFFER_TOO_SMALL)
    {
        print_string(L"[X] Get file info size\r\n");
        File->Close(File);
    }
    else
    {
        print_string(L"[OK] Get file info size\r\n");
    }
    // 分配内存存储文件信息
    status = gST->BootServices->AllocatePool(EfiLoaderData, InfoSize, (VOID **)&FileInfo);
    if (EFI_ERROR(status))
    {
        print_string(L"[X] Allocate memory for file info\r\n");
        File->Close(File);
    }
    else
    {
        print_string(L"[OK] Allocate memory for file info\r\n");
    }

    // 获取文件信息
    status = File->GetInfo(File, &FileInfoGuid, &InfoSize, FileInfo);
    if (EFI_ERROR(status))
    {
        print_string(L"[X] Get file info\r\n");
        gST->BootServices->FreePool(FileInfo);
        File->Close(File);
    }
    else
    {
        print_string(L"[OK] Get file info\r\n");
    }

    *FileSize = FileInfo->FileSize;

    // 分配内存存储文件内容
    status = gST->BootServices->AllocatePool(EfiLoaderData, *FileSize, Buffer);
    if (EFI_ERROR(status))
    {
        print_string(L"[X] Allocate memory for file\r\n");
        gST->BootServices->FreePool(FileInfo);
        File->Close(File);
    }
    else
    {
        print_string(L"[OK] Allocate memory for file\r\n");
    }

    // 读取文件内容
    UINTN ReadSize = *FileSize;
    status = File->Read(File, &ReadSize, *Buffer);
    if (EFI_ERROR(status))
    {
        print_string(L"[X] Read file\r\n");
        gST->BootServices->FreePool(*Buffer);
        gST->BootServices->FreePool(FileInfo);
        File->Close(File);
    }

    if (ReadSize != *FileSize)
    {
        print_string(L"[!] Read size doesn't match file size\r\n");
    }

    print_string(L"[OK] Read file\r\n");

    // 清理
    gST->BootServices->FreePool(FileInfo);
    File->Close(File);

    return EFI_SUCCESS;
}

// 加载内核
// 加载内核到1MB地址
EFI_STATUS load_kernel(void)
{
    EFI_STATUS status;
    EFI_FILE_PROTOCOL *Root;

    // 打开根目录
    status = open_root_directory(&Root);
    if (EFI_ERROR(status))
    {
        print_string(L"[X] Open root directory\r\n");
        return status;
    }

    print_string(L"[OK] Open root directory\r\n");

    // 先获取文件大小
    void *temp_buffer = NULL;
    UINTN file_size = 0;

    // 加载内核文件到临时缓冲区
    status = load_file(Root, L"kernel.bin", &temp_buffer, &file_size);
    if (EFI_ERROR(status))
    {
        print_string(L"[X] Load kernel file\r\n");
        return status;
    }

    print_string(L"[OK] Load kernel file to temp buffer\r\n");

    // 计算需要分配的页面数
    UINTN pages = (file_size + EFI_PAGE_SIZE - 1) / EFI_PAGE_SIZE;

    // 定义1MB地址 (0x100000)
    EFI_PHYSICAL_ADDRESS kernel_address = 0x100000;

    // 在1MB地址处分配内存
    status = gST->BootServices->AllocatePages(AllocateAddress, EfiLoaderData, pages, &kernel_address);
    if (EFI_ERROR(status))
    {
        print_string(L"[X] Allocate memory at 1MB\r\n");
        print_error(L"AllocatePages error", status);

        // 尝试分配任意地址
        kernel_address = 0x100000;
        status = gST->BootServices->AllocatePages(AllocateAnyPages, EfiLoaderData, pages, &kernel_address);
        if (EFI_ERROR(status))
        {
            print_string(L"[X] Failed to allocate memory for kernel\r\n");
            gST->BootServices->FreePool(temp_buffer);
            return status;
        }
        print_string(L"[OK] Allocated memory at dynamic address\r\n");
    }
    else
    {
        print_string(L"[OK] Allocated memory at 1MB (0x100000)\r\n");
    }

    // 将内核复制到1MB地址
    gKernelBase = (void *)(UINT64)kernel_address;
    gKernelSize = file_size;

    // 复制数据
    gST->BootServices->CopyMem(gKernelBase, temp_buffer, file_size);

    // 释放临时缓冲区
    gST->BootServices->FreePool(temp_buffer);

    // 打印内核地址信息
    print_string(L"[->] Kernel loaded at: ");
    CHAR16 addr_msg[64];
    wsprintf(addr_msg, L"0x%x%08x\r\n",
             (UINT32)((UINT64)gKernelBase >> 32),
             (UINT32)((UINT64)gKernelBase));
    print_string(addr_msg);

    return EFI_SUCCESS;
}

// 启动内核
EFI_STATUS boot_kernel(void)
{
    if (gKernelBase == NULL)
    {
        print_string(L"[X] Kernel not loaded\r\n");
        return EFI_NOT_READY;
    }

    // 打印内核地址
    print_string(L"[->] Kernel base address: ");

    // 使用 wsprintf 格式化打印64位地址
    CHAR16 kernel_addr_msg[64];
    wsprintf(kernel_addr_msg, L"0x%x%08x",
             (UINT32)((UINT64)gKernelBase >> 32), // 高32位
             (UINT32)((UINT64)gKernelBase));      // 低32位
    print_string(kernel_addr_msg);
    print_string(L"\r\n");

    // 打印内核大小
    CHAR16 kernel_size_msg[32];
    wsprintf(kernel_size_msg, L"[->] Kernel size: %d bytes\r\n", gKernelSize);
    print_string(kernel_size_msg);

    // 获取图形输出协议
    EFI_GRAPHICS_OUTPUT_PROTOCOL *gop = NULL;
    EFI_GUID gop_guid = EFI_GRAPHICS_OUTPUT_PROTOCOL_GUID;
    EFI_STATUS status = gST->BootServices->LocateProtocol(&gop_guid, NULL, (VOID **)&gop);

    // 内核参数结构（一定要与内核中的boot_params_t匹配!）
    #pragma pack(push, 1)
    typedef struct {
        uint64_t framebuffer_addr;
        uint32_t framebuffer_width;
        uint32_t framebuffer_height;
        uint32_t framebuffer_pitch;
        uint32_t framebuffer_bpp;
        uint64_t framebuffer_size;

        uint64_t memory_map_addr;
        uint64_t memory_map_size;
        uint64_t descriptor_size;
    } boot_params_t;
    #pragma pack(pop)

    // 在ExitBootServices之前分配内存存储参数
    boot_params_t *params = NULL;
    gST->BootServices->AllocatePool(EfiLoaderData, sizeof(boot_params_t), (VOID **)&params);
    if (params == NULL) {
        print_error(L"[X] Failed to allocate memory for params", EFI_OUT_OF_RESOURCES);
        return EFI_OUT_OF_RESOURCES;
    }
    memset(params, 0, sizeof(boot_params_t));

    if (!EFI_ERROR(status) && gop && gop->Mode)
    {
        print_string(L"[OK] Graphics output protocol found\r\n");

        // 查询所有可用的图形模式
        UINTN max_mode = gop->Mode->MaxMode;
        UINTN best_mode = gop->Mode->Mode;
        UINTN best_width = 0, best_height = 0;
        BOOLEAN found_target = false;

        print_string(L"[->] Getting graphics modes...\r\n");

        for (UINTN i = 0; i < max_mode; i++)
        {
            EFI_GRAPHICS_OUTPUT_MODE_INFORMATION *mode_info;
            UINTN size_of_info;

            EFI_STATUS status = gop->QueryMode(gop, i, &size_of_info, &mode_info);
            if (EFI_ERROR(status))
            {
                continue;
            }

            // 显示模式信息
            print_string(L"[->] Mode: ");

            // 显示模式编号
            CHAR16 mode_num[4];
            UINT32 temp = i;
            UINT32 digits = 0;
            do
            {
                digits++;
                temp /= 10;
            } while (temp > 0);
            temp = i;
            for (UINT32 j = 0; j < digits; j++)
            {
                mode_num[digits - 1 - j] = L'0' + (temp % 10);
                temp /= 10;
            }
            mode_num[digits] = L'\0';
            print_string(mode_num);

            print_string(L" Resolution: ");

            // 显示宽度
            temp = mode_info->HorizontalResolution;
            digits = 0;
            do
            {
                digits++;
                temp /= 10;
            } while (temp > 0);
            temp = mode_info->HorizontalResolution;
            for (UINT32 j = 0; j < digits; j++)
            {
                mode_num[digits - 1 - j] = L'0' + (temp % 10);
                temp /= 10;
            }
            mode_num[digits] = L'\0';
            print_string(mode_num);

            print_string(L"x");

            // 显示高度
            temp = mode_info->VerticalResolution;
            digits = 0;
            do
            {
                digits++;
                temp /= 10;
            } while (temp > 0);
            temp = mode_info->VerticalResolution;
            for (UINT32 j = 0; j < digits; j++)
            {
                mode_num[digits - 1 - j] = L'0' + (temp % 10);
                temp /= 10;
            }
            mode_num[digits] = L'\0';
            print_string(mode_num);
            print_string(L"\r\n");

            // 判断是否是1920x1080
            if (!found_target && mode_info->HorizontalResolution == 1920 &&
                mode_info->VerticalResolution == 1080)
            {
                best_mode = i;
                best_width = mode_info->HorizontalResolution;
                best_height = mode_info->VerticalResolution;
                break;
            }

            // 选择最佳方案（分辨率大于等于1024x768）
            if (!found_target && mode_info->HorizontalResolution >= 1024 &&
                mode_info->VerticalResolution >= 768)
            {
                if (mode_info->HorizontalResolution > best_width ||
                    (mode_info->HorizontalResolution == best_width &&
                     mode_info->VerticalResolution > best_height))
                {
                    best_mode = i;
                    best_width = mode_info->HorizontalResolution;
                    best_height = mode_info->VerticalResolution;
                }
            }
        }

        // 如果没有找到合适的分辨率，使用默认模式
        if (!found_target && best_width == 0)
        {
            best_mode = gop->Mode->Mode;
            best_width = gop->Mode->Info->HorizontalResolution;
            best_height = gop->Mode->Info->VerticalResolution;
            print_string(L"[->] Use default mode\r\n");
        }
        else if (found_target)
        {
            print_string(L"[->] Using 1920x1080 mode\r\n");
        }
        else
        {
            print_string(L"[->] Choose the best available mode: ");

            // 显示选定的模式编号
            CHAR16 mode_num[4];
            UINT32 temp = best_mode;
            UINT32 digits = 0;
            do
            {
                digits++;
                temp /= 10;
            } while (temp > 0);
            temp = best_mode;
            for (UINT32 j = 0; j < digits; j++)
            {
                mode_num[digits - 1 - j] = L'0' + (temp % 10);
                temp /= 10;
            }
            mode_num[digits] = L'\0';
            print_string(mode_num);

            print_string(L" Resolution: ");

            // 显示宽度
            temp = best_width;
            digits = 0;
            do
            {
                digits++;
                temp /= 10;
            } while (temp > 0);
            temp = best_width;
            for (UINT32 j = 0; j < digits; j++)
            {
                mode_num[digits - 1 - j] = L'0' + (temp % 10);
                temp /= 10;
            }
            mode_num[digits] = L'\0';
            print_string(mode_num);

            print_string(L"x");

            // 显示高度
            temp = best_height;
            digits = 0;
            do
            {
                digits++;
                temp /= 10;
            } while (temp > 0);
            temp = best_height;
            for (UINT32 j = 0; j < digits; j++)
            {
                mode_num[digits - 1 - j] = L'0' + (temp % 10);
                temp /= 10;
            }
            mode_num[digits] = L'\0';
            print_string(mode_num);

            print_string(L"\r\n");
        }

        // 设置选定的图形模式
        if (best_mode != gop->Mode->Mode)
        {
            EFI_STATUS status = gop->SetMode(gop, best_mode);
            if (EFI_ERROR(status))
            {
                print_error(L"[X] Set graphics mode", status);
                // 如果设置失败，使用当前模式
                best_mode = gop->Mode->Mode;
            }
            else
            {
                print_string(L"[OK] Set graphics mode\r\n");
            }
        }
        else
        {
            print_string(L"[->] Use current graphics mode\r\n");
        }

        // 获取当前模式信息
        EFI_GRAPHICS_OUTPUT_MODE_INFORMATION *mode_info = NULL;
        UINTN size_of_info = 0;

        status = gop->QueryMode(gop, best_mode, &size_of_info, &mode_info);
        if (!EFI_ERROR(status) && mode_info)
        {
            // 使用图形输出协议的帧缓冲信息
            params->framebuffer_addr = (uint64_t)gop->Mode->FrameBufferBase;
            params->framebuffer_width = mode_info->HorizontalResolution;
            params->framebuffer_height = mode_info->VerticalResolution;
            params->framebuffer_pitch = mode_info->PixelsPerScanLine *
                                       (mode_info->PixelFormat == PixelRedGreenBlueReserved8BitPerColor ? 4 : 2);
            params->framebuffer_bpp = mode_info->PixelFormat == PixelRedGreenBlueReserved8BitPerColor ? 32 : 16;

            print_string(L"[OK] Graphics mode configured\r\n");

            gST->BootServices->FreePool(mode_info);
        }
        else
        {
            print_error(L"[X] Failed to query graphics mode", status);
            // 如果查询失败，使用默认的VGA地址
            params->framebuffer_addr = 0xA0000;
            params->framebuffer_width = 320;
            params->framebuffer_height = 200;
            params->framebuffer_pitch = 320;
            params->framebuffer_bpp = 8;
        }
    }
    else
    {
        // 如果没有图形输出协议，使用默认的VGA地址
        params->framebuffer_addr = 0xA0000;
        params->framebuffer_width = 320;
        params->framebuffer_height = 200;
        params->framebuffer_pitch = 320;
        params->framebuffer_bpp = 8;

        print_string(L"[!] Using default VGA framebuffer\r\n");
    }

    EFI_MEMORY_DESCRIPTOR *memory_map = NULL;
    UINTN memory_map_size = 0;
    UINTN map_key;
    UINTN descriptor_size = 0;
    UINT32 descriptor_version = 0;

    memory_map_size += 2 * descriptor_size;
    status = gST->BootServices->AllocatePool(EfiLoaderData, memory_map_size, (VOID **)&memory_map);
    if (EFI_ERROR(status))
    {
        print_error(L"[X] Allocate memory for memory map", status);
        return status;
    }
    print_string(L"[OK] Allocate memory for memory map\r\n");

    status = gST->BootServices->GetMemoryMap(&memory_map_size, memory_map, &map_key,
                                             &descriptor_size, &descriptor_version);

    if (status != EFI_BUFFER_TOO_SMALL)
    {
        print_error(L"[X] Get memory map size", status);
        return status;
    }
    print_string(L"[OK] Get memory map size\r\n");

    // 填充参数
    params->memory_map_addr = (uint64_t)memory_map;
    params->memory_map_size = (uint64_t)memory_map_size;
    params->descriptor_size = (uint64_t)descriptor_size;

    while (1)
    {
        UINTN size = memory_map_size;
        status = gST->BootServices->GetMemoryMap(&size, memory_map, &map_key,
                                                 &descriptor_size, &descriptor_version);
        status = gST->BootServices->ExitBootServices(gImageHandle, map_key);
        if (status == EFI_SUCCESS)
        {
            break;
        }
        print_string(L"[!] ExitBootServices invalid parameter, retry\r\n");
    }

    // 调用内核入口点，传递参数
    void (*kernel_entry)(void *) = (void (*)(void *))gKernelBase;
    params->framebuffer_height *= 2;
    params->framebuffer_pitch *= 2;
    kernel_entry(params);

    return EFI_SUCCESS;
}

// 查找内核
void *find_kernel(void)
{
    print_string(gKernelBase);
    print_string(L"\r\n");
    return gKernelBase;
}