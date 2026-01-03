ENTRY(kmain)

SECTIONS
{
    . = 0x100000;  /* 内核加载到1MB地址 */
    
    /* 代码段在最前面 */
    .text : {
        *(.text*)  /* 包含所有.text段 */
    }
    
    /* 只读数据段（字符串常量等）放在代码后面 */
    .rodata : ALIGN(4) {
        *(.rodata*)
    }
    
    /* 数据段（已初始化的全局/静态变量） */
    .data : ALIGN(4) {
        *(.data*)
    }
    
    /* BSS段（未初始化的全局/静态变量） */
    .bss : ALIGN(4) {
        *(.bss*)
        *(COMMON)  /* 包含未初始化的全局变量 */
    }
    
    /* 确保BSS段在文件末尾 */
    . = ALIGN(4096);
    
    /* 结束符号，可用于计算内核大小 */
    _end = .;
}