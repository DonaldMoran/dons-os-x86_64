ENTRY(_start)

SECTIONS
{
    /* User program base address: must match USER_CODE_BASE */
    . = 0x0000008000000000;

    .text : {
        *(.text*)
    }

    .rodata : {
        *(.rodata*)
    }

    .data : {
        *(.data*)
    }

    .bss : {
        *(.bss*)
        *(COMMON)
    }

    /* Align end to page boundary */
    . = ALIGN(0x1000);
}
