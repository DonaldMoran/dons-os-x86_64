#ifndef GDT_H
#define GDT_H

#include <stdint.h>
#include <stdbool.h>

/*
 * GDT selectors.
 *
 * The kernel builds its own GDT in gdt.c at a higher-half .bss
 * address, and loads it in gdt_init before idt_init runs.  From that
 * point on, sgdt returns the kernel's own GDT base, not the
 * bootloader's.  Every consumer (tss_init's ltr, gdt_dump,
 * isr13_handler's descriptor walk) reads through the kernel's table.
 *
 * See MAINTENANCE.md item 3h for why this matters — the bootloader's
 * GDT lived in low memory and was reachable only through the identity
 * map.
 */

/* Descriptor layout (indices into kernel_gdt): */
#define GDT_NULL        0x00    /* [0] null                     */
#define GDT_CODE32      0x08    /* [1] code32 (legacy)          */
#define GDT_DATA        0x10    /* [2] data32 (legacy)          */
#define GDT_CODE64      0x18    /* [3] code64, DPL=0            */
#define GDT_DATA64      0x20    /* [4] data64, DPL=0            */
#define GDT_USER_DATA   0x28    /* [5] user data, DPL=3         */
#define GDT_USER_CODE   0x30    /* [6] user code, DPL=3, L=1    */
#define GDT_TSS         0x38    /* [7] TSS descriptor           */

/*
 * Selector arithmetic for SYSCALL/SYSRET, for reference:
 *
 *   STAR[47:32] = 0x18  =>  SYSCALL loads CS = 0x18, SS = 0x20.
 *   STAR[63:48] = 0x23  =>  SYSRET  loads CS = 0x33, SS = 0x2B.
 *
 * 0x33 = index 6 with RPL=3 (user code), 0x2B = index 5 with RPL=3
 * (user data).  context_switch.asm's user-frame pushes use the same
 * selectors.  The user-code and user-data descriptors must therefore
 * live at indices 6 and 5 respectively.
 */

/* Function prototypes. */
void gdt_init(void);                /* build + lgdt + reload segments */
void gdt_reload(void);              /* far jump to refresh CS, then reload data segs */
void gdt_set_tss(uint64_t tss_addr, uint32_t tss_size);
void gdt_fix_user_segments(void);   /* no-op since gdt_init; kept for callers */
void gdt_dump_entry(int index);
void gdt_dump(void);

#endif /* GDT_H */
