OUTPUT_FORMAT("elf64-x86-64")
ENTRY(_start)

SECTIONS
{
    /* Absolute base address definition for the user space block */
    . = 0x8000000000;

    /* Combine all sections into a single, contiguous unified block 
       to prevent mcmodel=large segment pointer shifting anomalies */
    .text : ALIGN(4096) {
        *(.text .text.*)
        *(.rodata .rodata.*)
        *(.data .data.*)
        *(.user_data)
        *(.gnu.linkonce.d.*)
        *(.got)
        *(.got.plt)
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
    }
}
