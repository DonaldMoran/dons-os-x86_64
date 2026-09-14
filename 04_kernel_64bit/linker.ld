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

    /*
     * _kernel_end marks the end of the kernel's virtual footprint,
     * including .bss. pmm_init uses it (via the physical address it
     * resolves to) to reserve the kernel's full in-memory range so
     * that no LOW-zone page allocation can land inside the kernel's
     * own .bss. See the comment above the reservation block in pmm.c.
     *
     * Must be emitted AFTER .bss so that it captures the full extent.
     * Note that this symbol is a linker symbol, not a C variable; from
     * C, take its address (&_kernel_end) rather than its value.
     */
    _kernel_end = .;

    /* Traps Newlib's metadata tables to prevent them from corrupting .userelf offsets */
    /DISCARD/ : {
        *(.comment)
        *(.note.*)
        *(.eh_frame)
        *(.rela.*)
    }
}
