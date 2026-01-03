[bits 64]
extern idt_handler
section .text

; --- 1. 定义中断入口宏 ---

; 用于没有错误码的中断（大部分异常和所有硬件中断）
%macro ISR_NOERRCODE 1
global isr%1
isr%1:
    push qword 0      ; 压入伪错误码，保证栈结构一致
    push qword %1     ; 压入中断向量号
    jmp isr_common
%endmacro

; 用于有错误码的异常
%macro ISR_ERRCODE 1
global isr%1
isr%1:
    ; CPU 已经自动压入了错误码
    push qword %1     ; 压入中断向量号
    jmp isr_common
%endmacro

; --- 2. 生成 256 个入口点 ---
; 严格遵循 x86_64 异常错误码规范
%assign i 0
%rep 256
    ; 只有以下向量号由 CPU 自动压入错误码：
    ; 8 (Double Fault), 10 (Invalid TSS), 11 (Segment Not Present)
    ; 12 (Stack Segment Fault), 13 (General Protection Fault)
    ; 14 (Page Fault), 17 (Alignment Check), 30 (Security Exception)
    %if i == 8 || (i >= 10 && i <= 14) || i == 17 || i == 30
        ISR_ERRCODE i
    %else
        ISR_NOERRCODE i
    %endif
%assign i i+1
%endrep

; --- 3. 统一处理逻辑 ---
isr_common:
    ; 保存通用寄存器 (与你的 interrupt_frame_t 结构体完全对应)
    push rax
    push rbx
    push rcx
    push rdx
    push rsi
    push rdi
    push rbp
    push r8
    push r9
    push r10
    push r11
    push r12
    push r13
    push r14
    push r15
    
    ; 将当前栈指针（指向 interrupt_frame_t 的起始位置）作为第一个参数传递给 C 函数
    mov rdi, rsp 
    
    ; --- 栈对齐 (x86_64 System V ABI 要求) ---
    mov rbp, rsp
    and rsp, -16      ; 确保栈 16 字节对齐，防止某些 C 代码使用 SSE 指令时崩溃
    
    call idt_handler
    
    mov rsp, rbp      ; 恢复原始栈指针
    
    ; 恢复通用寄存器 (顺序必须与压栈严格相反)
    pop r15
    pop r14
    pop r13
    pop r12
    pop r11
    pop r10
    pop r9
    pop r8
    pop rbp
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop rbx
    pop rax
    
    ; 清理栈：弹出中断号和错误码 (2 * 8 = 16 字节)
    add rsp, 16 
    
    iretq             ; 从中断返回

; --- 4. 生成地址表 ---
section .data
align 8
global isr_stub_table
isr_stub_table:
%assign i 0
%rep 256
    dq isr%+i         ; 在 64 位模式下使用 dq (8字节)
%assign i i+1
%endrep