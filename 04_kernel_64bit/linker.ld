ENTRY(_start)

SECTIONS
{
    . = 0x00100000;

    .text : {
        *(.text*)
    }

    .rodata : {
        *(.rodata*)
    }

    .data : {
        *(.data*)
    }

    __bss_start = .;
    .bss : {
        *(.bss*)
        *(COMMON)
    }
    
    /* Reserve 1MB of padding so .bss can grow safely */
    . = ALIGN(4096);
    .bss_padding : {
        . = . + 0x100000;
    }
    
    __bss_end = .;
}
