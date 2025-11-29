#include "efi_utils.h"
#include "efi.h"

// 全局变量
EFI_HANDLE gImageHandle = NULL;
static void *gKernelBase = NULL;
static UINTN gKernelSize = 0;

// 设置图像句柄
void set_image_handle(EFI_HANDLE handle) {
    gImageHandle = handle;
}

// 打印字符串
void print_string(CHAR16 *str) {
    gST->ConOut->OutputString(gST->ConOut, str);
}

// 打印错误信息
void print_error(CHAR16 *message, EFI_STATUS status) {
    print_string(message);
    print_string(L": ");
    
    CHAR16 buffer[32];
    for (int i = 0; i < 16; i++) {
        UINT8 nibble = (status >> (60 - i * 4)) & 0xF;
        buffer[i] = (nibble < 10) ? (L'0' + nibble) : (L'A' + nibble - 10);
    }
    buffer[16] = L'\r';
    buffer[17] = L'\n';
    buffer[18] = L'\0';
    
    print_string(buffer);
}

// 打开根目录
EFI_STATUS open_root_directory(EFI_FILE_PROTOCOL **Root) {
    EFI_STATUS status;
    EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *FileSystem;
    EFI_GUID FileSystemGuid = {0x0964e5b22, 0x6459, 0x11d2, {0x8e, 0x39, 0x00, 0xa0, 0xc9, 0x69, 0x72, 0x3b}};
    
    print_string(L"Locating file system protocol...\r\n");
    
    // 定位文件系统协议
    status = gST->BootServices->LocateProtocol(&FileSystemGuid, NULL, (VOID**)&FileSystem);
    if (EFI_ERROR(status)) {
        print_string(L"Failed to locate file system protocol\r\n");
     
    }
    
    print_string(L"File system protocol found, opening volume...\r\n");
    
    // 打开根目录
    status = FileSystem->OpenVolume(FileSystem, Root);
    if (EFI_ERROR(status)) {
        print_string(L"Failed to open root directory\r\n");
     
    }
    
    print_string(L"Root directory opened successfully\r\n");
    return EFI_SUCCESS;
}

// 加载文件
EFI_STATUS load_file(EFI_FILE_PROTOCOL *Root, CHAR16 *FileName, void **Buffer, UINTN *FileSize) {
    EFI_STATUS status;
    EFI_FILE_PROTOCOL *File;
    EFI_FILE_INFO *FileInfo;
    EFI_GUID FileInfoGuid = EFI_FILE_INFO_ID;
    UINTN InfoSize = 0;

    
    // 打开文件
    status = Root->Open(Root, &File, FileName, EFI_FILE_MODE_READ, 0);
    if (EFI_ERROR(status)) {
        print_string(L"[X] Open file: ");
        print_string(FileName);
        print_string(L"\r\n");
     
    } else {
        print_string(L"[OK] Open file: ");
        print_string(FileName);
        print_string(L"\r\n");
    }

    // 获取文件信息
    status = File->GetInfo(File, &FileInfoGuid, &InfoSize, NULL);
    if (status != EFI_BUFFER_TOO_SMALL) {
        print_string(L"[X] Get file info size\r\n");
        File->Close(File);
     
    } else {
        print_string(L"[OK] Get file info size\r\n");
    }
    // 分配内存存储文件信息
    status = gST->BootServices->AllocatePool(EfiLoaderData, InfoSize, (VOID**)&FileInfo);
    if (EFI_ERROR(status)) {
        print_string(L"[X] Allocate memory for file info\r\n");
        File->Close(File);
     
    } else {
        print_string(L"[OK] Allocate memory for file info\r\n");
    }
    
    // 获取文件信息
    status = File->GetInfo(File, &FileInfoGuid, &InfoSize, FileInfo);
    if (EFI_ERROR(status)) {
        print_string(L"[X] Get file info\r\n");
        gST->BootServices->FreePool(FileInfo);
        File->Close(File);
     
    } else {
        print_string(L"[OK] Get file info\r\n");
    }
    
    *FileSize = FileInfo->FileSize;

    // 分配内存存储文件内容
    status = gST->BootServices->AllocatePool(EfiLoaderData, *FileSize, Buffer);
    if (EFI_ERROR(status)) {
        print_string(L"[X] Allocate memory for file\r\n");
        gST->BootServices->FreePool(FileInfo);
        File->Close(File);
     
    } else {
        print_string(L"[OK] Allocate memory for file\r\n");
    }

    // 读取文件内容
    UINTN ReadSize = *FileSize;
    status = File->Read(File, &ReadSize, *Buffer);
    if (EFI_ERROR(status)) {
        print_string(L"[X] Read file\r\n");
        gST->BootServices->FreePool(*Buffer);
        gST->BootServices->FreePool(FileInfo);
        File->Close(File);
    }
    
    if (ReadSize != *FileSize) {
        print_string(L"[!] Read size doesn't match file size\r\n");
    }
    
    print_string(L"[OK] Read file\r\n");
    
    // 清理
    gST->BootServices->FreePool(FileInfo);
    File->Close(File);
    
    return EFI_SUCCESS;
}

// 加载内核
EFI_STATUS load_kernel(void) {
    EFI_STATUS status;
    EFI_FILE_PROTOCOL *Root;
    
    // 打开根目录
    status = open_root_directory(&Root);
    if (EFI_ERROR(status)) {
        print_string(L"[X] Open root directory\r\n");
    } else {
        print_string(L"[OK] Open root directory\r\n");
    }
    
    // 加载内核文件
    status = load_file(Root, L"kernel.bin", &gKernelBase, &gKernelSize);
    if (EFI_ERROR(status)) {
        print_string(L"[X] Load kernel file\r\n");
    } else {
        print_string(L"[OK] Load kernel file\r\n");
    }

    
    return EFI_SUCCESS;
}

// 启动内核
EFI_STATUS boot_kernel(void) {    
    if (gKernelBase == NULL) {
        print_string(L"[X] Kernel not loaded\r\n");
        return EFI_NOT_READY;
    }
    
    // 获取内存映射
    EFI_MEMORY_DESCRIPTOR* memory_map = NULL;
    UINTN memory_map_size = 0;
    UINTN map_key;
    UINTN descriptor_size;
    UINT32 descriptor_version;
    
    // 第一次调用获取所需大小
    EFI_STATUS status = gST->BootServices->GetMemoryMap(&memory_map_size, memory_map, &map_key, // <-- memory_map 是 NULL
                                          &descriptor_size, &descriptor_version);
    if (status != EFI_BUFFER_TOO_SMALL) {
        print_error(L"[X] Get memory map size", status);
     
    } else {
        print_string(L"[OK] Get memory map size\r\n");
    }
    
    // 分配内存
    status = gST->BootServices->AllocatePool(EfiLoaderData, memory_map_size, (VOID**)&memory_map);
    if (EFI_ERROR(status)) {
        print_error(L"[X] Allocate memory for memory map", status);
    } else {
        print_string(L"[OK] Allocate memory for memory map\r\n");
    }
    
    // 获取实际内存映射
    status = gST->BootServices->GetMemoryMap(&memory_map_size, memory_map, &map_key,
                           &descriptor_size, &descriptor_version);
    if (EFI_ERROR(status)) {
        return status;
    }
    
    // 退出引导服务
    status = gST->BootServices->ExitBootServices(gImageHandle, map_key);
    if (EFI_ERROR(status)) {
        return status;
    }
    
    // 跳转到内核
    void (*kernel_entry)(void) = (void(*)())gKernelBase;
    kernel_entry();
    
    // 不应该到达这里
    while (1);

    return EFI_SUCCESS;
}

// 查找内核
void *find_kernel(void) {
    print_string(gKernelBase);
    print_string("\r\n");
    return gKernelBase;
}
