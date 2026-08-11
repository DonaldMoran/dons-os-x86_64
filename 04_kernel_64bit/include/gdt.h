#ifndef GDT_H
#define GDT_H

#include <stdint.h>
#include <stdbool.h>

// GDT selectors (match the bootloader GDT)
#define GDT_NULL        0x00
#define GDT_CODE32      0x08
#define GDT_DATA        0x10
#define GDT_CODE64      0x18
#define GDT_DATA64      0x20
#define GDT_USER_CODE   0x28
#define GDT_USER_DATA   0x30
#define GDT_TSS         0x38

// Function prototypes
void gdt_init(void);
void gdt_reload(void);
void gdt_set_tss(uint64_t tss_addr, uint32_t tss_size);
void gdt_debug_print(void);

#endif // GDT_H
