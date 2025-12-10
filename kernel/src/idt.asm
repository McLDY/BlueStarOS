section .text
global idt_load

idt_load:
    mov eax, [esp + 4]  ; 获取IDT指针
    lidt [eax]          ; 加载IDT
    sti                 ; 开启中断
    ret