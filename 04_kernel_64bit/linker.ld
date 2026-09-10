ENTRY(_start)
 
SECTIONS
{
    /* --- LOW MEMORY TSS SECTION FIRST --- */
    . = 0x5000;
 
    .tss (NOLOAD) : {
        *(.data.tss)
    }
 
    /* --- NOW SWITCH TO HIGHER-HALF KERNEL BASE --- */
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
 
    /* Your original clean section */
    .userelf : {
        *(.userelf)
    }
 
    .bss : {
        *(.bss*)
        *(COMMON)
        . = ALIGN(4096);
        . += 8192;
        . = ALIGN(4096);
    }

    /* Traps Newlib's metadata tables to prevent them from corrupting .userelf offsets */
    /DISCARD/ : {
        *(.comment)
        *(.note.*)
        *(.eh_frame)
        *(.rela.*)
    }
}
