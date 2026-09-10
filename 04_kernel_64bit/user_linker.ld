OUTPUT_FORMAT("elf64-x86-64")
ENTRY(_start)

SECTIONS
{
    . = 0x8000000000;

    .text : ALIGN(4096) {
        *(.text .text.*)
        *(.rodata .rodata.*)
        *(.data .data.*)
    }

    .bss : ALIGN(4096) {
        __bss_start = .;
        *(.bss .bss.*)
        *(COMMON)
        __bss_end = .;
    }

    /DISCARD/ : {
        *(.comment)
        *(.note.*)
        *(.eh_frame)
    }
}
