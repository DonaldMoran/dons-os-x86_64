OUTPUT_FORMAT("elf64-x86-64")
ENTRY(_start)

SECTIONS
{
    /* Anchor the absolute base address definition for the user space block */
    . = 0x8000000000;

    .text : ALIGN(4096) {
        *(.text .text.*)
    }

    /* Isolate relocation sections to preserve their native SHT_RELA hardware type */
    .rela.dyn : ALIGN(4096) {
        *(.rela.dyn)
        *(.rela.data .rela.data.*)
    }

    .rela.plt : ALIGN(4096) {
        *(.rela.plt)
    }

    .data : ALIGN(4096) {
        *(.rodata .rodata.*)
        *(.data .data.*)
        *(.user_data)
        *(.gnu.linkonce.d.*)
        *(.got)
        *(.got.plt)
        *(.plt)
        *(.plt.*)
        *(.dynsym)
        *(.dynstr)
    }

    /* CRITICAL FIX SECTION: Explicitly capture Newlib's common impure variables
       and lock them down into a dedicated, page-aligned 64-bit section container.
       This forces ld.lld to preserve full absolute 64-bit pointer metrics. */
    .impure : ALIGN(4096) {
        _impure_ptr = .;
        . += 8;
        _global_impure_ptr = .;
        . += 8;
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

