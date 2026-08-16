ENTRY(_start)

SECTIONS
{
    /* User program base address */
    . = 0x400000;

    /* Single contiguous PT_LOAD segment */
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
