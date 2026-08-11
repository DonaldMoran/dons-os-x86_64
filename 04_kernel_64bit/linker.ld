ENTRY(_start)

SECTIONS
{
    /* Higher-half kernel */
    . = 0xFFFFFFFF80100000;
    
    .text : {
        *(.text*)
    }
    
    .rodata : {
        *(.rodata*)
    }
    
    .data : {
        *(.data*)
    }
    
    /* TSS goes in low memory */
    .tss 0x5000 (NOLOAD) : {
        *(.data.tss)
    }
    
    .bss : {
        *(.bss*)
        *(COMMON)
    }
}
