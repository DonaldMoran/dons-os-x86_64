OUTPUT_FORMAT(elf64-x86-64)
ENTRY(_start)

SECTIONS
{
    /* User virtual base address */
    . = 0x8000000000;

    /* Code */
    .text : ALIGN(16)
    {
        *(.text*)
    }

    /* Read-only data (strings, consts, etc.) */
    .rodata : ALIGN(16)
    {
        *(.rodata*)
        *(.rodata.str1.1*)
        *(.rodata.str1.8*)
    }

    /* Exception frames (needed for clang) */
    .eh_frame : ALIGN(16)
    {
        *(.eh_frame*)
    }

    /* Writable data */
    .data : ALIGN(16)
    {
        *(.data*)
    }

    /* Zero‑fill section */
    .bss : ALIGN(16)
    {
        *(.bss*)
        *(COMMON)
        _end = .;  /* IMPORTANT: For sbrk to know where heap starts */
    }
}
