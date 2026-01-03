#ifndef EFI_TYPES_H
#define EFI_TYPES_H

// UEFI 基础类型定义
typedef unsigned char uint8_t;
typedef unsigned short uint16_t;
typedef unsigned int uint32_t;
typedef unsigned long long uint64_t;

// 有符号类型定义
typedef signed char int8_t;
typedef signed short int16_t;
typedef signed int int32_t;
typedef signed long long int64_t;

// UEFI 特定类型
typedef uint8_t BOOLEAN;

// 在 64 位 UEFI 中，UINTN 应该是 64 位
typedef int64_t INTN;
typedef uint64_t UINTN;
typedef int64_t INT64;
typedef uint64_t UINT64;
typedef int32_t INT32;
typedef uint32_t UINT32;
typedef int16_t INT16;
typedef uint16_t UINT16;
typedef int8_t INT8;
typedef uint8_t UINT8;
typedef uint16_t CHAR16;
typedef uint8_t CHAR8;

// 物理地址和虚拟地址类型
typedef UINT64 EFI_PHYSICAL_ADDRESS;
typedef UINT64 EFI_VIRTUAL_ADDRESS;

// VOID 类型
typedef void VOID;

// NULL 定义
#ifndef NULL
#ifdef __cplusplus
#define NULL 0
#else
#define NULL ((void*)0)
#endif
#endif

// EFIAPI 调用约定
#ifdef _MSC_VER
#define EFIAPI __cdecl
#else
#define EFIAPI __attribute__((ms_abi))
#endif

// EFI 状态码
typedef UINT64 EFI_STATUS;

#define EFI_SUCCESS 0
#define EFI_LOAD_ERROR (1 | (1ULL << 63))
#define EFI_INVALID_PARAMETER (2 | (1ULL << 63))
#define EFI_UNSUPPORTED (3 | (1ULL << 63))
#define EFI_BAD_BUFFER_SIZE (4 | (1ULL << 63))
#define EFI_BUFFER_TOO_SMALL (5 | (1ULL << 63))
#define EFI_NOT_READY (6 | (1ULL << 63))
#define EFI_DEVICE_ERROR (7 | (1ULL << 63))
#define EFI_WRITE_PROTECTED (8 | (1ULL << 63))
#define EFI_OUT_OF_RESOURCES (9 | (1ULL << 63))
#define EFI_VOLUME_CORRUPTED (10 | (1ULL << 63))
#define EFI_VOLUME_FULL (11 | (1ULL << 63))
#define EFI_NO_MEDIA (12 | (1ULL << 63))
#define EFI_MEDIA_CHANGED (13 | (1ULL << 63))
#define EFI_NOT_FOUND (14 | (1ULL << 63))
#define EFI_ACCESS_DENIED (15 | (1ULL << 63))
#define EFI_NO_RESPONSE (16 | (1ULL << 63))
#define EFI_NO_MAPPING (17 | (1ULL << 63))
#define EFI_TIMEOUT (18 | (1ULL << 63))
#define EFI_NOT_STARTED (19 | (1ULL << 63))
#define EFI_ALREADY_STARTED (20 | (1ULL << 63))
#define EFI_ABORTED (21 | (1ULL << 63))
#define EFI_ICMP_ERROR (22 | (1ULL << 63))
#define EFI_TFTP_ERROR (23 | (1ULL << 63))
#define EFI_PROTOCOL_ERROR (24 | (1ULL << 63))
#define EFI_INCOMPATIBLE_VERSION (25 | (1ULL << 63))
#define EFI_SECURITY_VIOLATION (26 | (1ULL << 63))
#define EFI_CRC_ERROR (27 | (1ULL << 63))
#define EFI_END_OF_MEDIA (28 | (1ULL << 63))
#define EFI_END_OF_FILE (31 | (1ULL << 63))
#define EFI_INVALID_LANGUAGE (32 | (1ULL << 63))
#define EFI_COMPROMISED_DATA (33 | (1ULL << 63))

#define EFI_ERROR(status) (((INTN)(status)) < 0)

#endif