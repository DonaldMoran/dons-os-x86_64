#ifndef GDT_H
#define GDT_H

#include <stdint.h>
#include <stdbool.h>

// GDT selectors (match the bootloader GDT, adjusted for SYSRET)
#define GDT_NULL        0x00
#define GDT_CODE32      0x08
#define GDT_DATA        0x10
#define GDT_CODE64      0x18
#define GDT_DATA64      0x20

// For SYSRET:
//   U = 0x20
//   SYSRET CS = U + 16 = 0x30  (user code)
//   SYSRET SS = U + 8  = 0x28  (user data)
#define GDT_USER_DATA   0x28
#define GDT_USER_CODE   0x30

#define GDT_TSS         0x38

// Function prototypes
void gdt_init(void);
void gdt_reload(void);
void gdt_set_tss(uint64_t tss_addr, uint32_t tss_size);
void gdt_debug_print(void);
void gdt_fix_user_segments(void);  // ADDED: Fix user segments to 64-bit
void gdt_dump_entry(int index);


#endif // GDT_H
