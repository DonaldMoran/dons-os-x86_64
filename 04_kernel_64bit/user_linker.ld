OUTPUT_FORMAT(elf64-x86-64)
ENTRY(_start)

SECTIONS
{
    /* User program base address: must match USER_CODE_BASE */
    . = 0x0000008000000000;

    /* All sections combined into one LOAD segment */
    .text : {
        *(.text)
        *(.text.*)
    }

    /* Align .rodata to 16 bytes for better string handling */
    . = ALIGN(16);
    .rodata : {
        *(.rodata)
        *(.rodata.*)
        *(.rodata.str1.1)
        *(.rodata.str1.16)
        *(.rodata.str1.32)
    }

    .data : {
        *(.data)
        *(.data.*)
    }

    .bss : {
        *(.bss)
        *(.bss.*)
        *(COMMON)
    }

    /* Align end to page boundary for proper mapping */
    . = ALIGN(0x1000);
}
