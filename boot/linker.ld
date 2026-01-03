ENTRY(efi_main)

SECTIONS
{
    . = 0x200000; 
    
    .text : {
        *(.text)
        *(.text.*)
    }
    
    .data : {
        *(.data)
        *(.data.*)
        *(.rodata)
        *(.rodata.*)
    }
    
    .bss : {
        *(.bss)
        *(COMMON)
    }
    
    /DISCARD/ : {
        *(.comment)
        *(.note.*)
    }
}