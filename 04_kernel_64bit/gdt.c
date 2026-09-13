#include "include/gdt.h"
#include "include/tss.h"
#include "include/serial.h"
#include <stdint.h>
#include "include/vga.h"

void gdt_init(void) {
    // Using the bootloader's GDT — nothing to build here.
}

void gdt_reload(void) {
    // Reload segment registers with the kernel code and data selectors.
    __asm__ volatile (
        "pushq $0x18\n\t"          // Kernel code selector
        "leaq 1f(%%rip), %%rax\n\t"
        "pushq %%rax\n\t"
        "lretq\n\t"
        "1:\n\t"
        "mov $0x20, %%ax\n\t"      // Kernel data selector
        "mov %%ax, %%ds\n\t"
        "mov %%ax, %%es\n\t"
        "mov %%ax, %%fs\n\t"
        "mov %%ax, %%gs\n\t"
        "mov %%ax, %%ss\n\t"
        : : : "rax", "memory"
    );
}

void gdt_set_tss(uint64_t tss_addr, uint32_t tss_size) {
    // Get the current GDT base address.
    struct {
        uint16_t limit;
        uint64_t base __attribute__((packed));
    } gdt_ptr;

    __asm__ volatile ("sgdt %0\n" : "=m"(gdt_ptr) : : "memory");

    uint64_t gdt_addr = gdt_ptr.base;

    // TSS descriptor is at offset 0x38 (two 8-byte entries: low and high).
    uint64_t* tss_desc_low  = (uint64_t*)(gdt_addr + 0x38);
    uint64_t* tss_desc_high = (uint64_t*)(gdt_addr + 0x40);

    uint64_t base  = tss_addr;
    uint64_t limit = tss_size;

    // Ensure the limit covers at least the minimum 64-bit TSS size.
    if (limit < 0x67) {
        limit = 0x67;
    }

    // Low 64 bits
    uint64_t desc_low = 0;
    desc_low |= (limit & 0xFFFF);                    // Limit[15:0]
    desc_low |= ((base & 0xFFFFFF) << 16);           // Base[23:0]
    desc_low |= ((uint64_t)0x89 << 40);              // Access: 64-bit TSS, present, DPL=0
    desc_low |= ((limit >> 16) & 0xF) << 48;         // Limit[19:16]
    desc_low |= ((base >> 24) & 0xFF) << 56;         // Base[31:24]

    // High 64 bits
    uint64_t desc_high = (base >> 32) & 0xFFFFFFFF;  // Base[63:32]

    *tss_desc_low  = desc_low;
    *tss_desc_high = desc_high;
}

void gdt_fix_user_segments(void) {
    struct {
        uint16_t limit;
        uint64_t base __attribute__((packed));
    } gdt_ptr;

    __asm__ volatile ("sgdt %0" : "=m"(gdt_ptr));

    uint64_t* gdt = (uint64_t*)gdt_ptr.base;

    // User data segment at 0x28: 64-bit data, DPL=3, Present, Writable
    gdt[GDT_USER_DATA / 8] = 0x00CFF2000000FFFFULL;

    // User code segment at 0x30: 64-bit code, DPL=3, Present, Executable, Readable
    gdt[GDT_USER_CODE / 8] = 0x00AFFA000000FFFFULL;

    // Reload data segment registers with the kernel data selector.
    __asm__ volatile (
        "mov $0x20, %%ax\n\t"
        "mov %%ax, %%ds\n\t"
        "mov %%ax, %%es\n\t"
        "mov %%ax, %%fs\n\t"
        "mov %%ax, %%gs\n\t"
        "mov %%ax, %%ss\n\t"
        : : : "rax", "memory"
    );

    serial_print("GDT: User segments fixed for Ring 3\n");
}

/* Decode and print the current GDT on demand. Called from the kernel
   shell's gdtdump command. Prints to both serial and VGA. */
void gdt_dump(void) {
    struct {
        uint16_t limit;
        uint64_t base __attribute__((packed));
    } gdt_ptr;

    __asm__ volatile ("sgdt %0" : "=m"(gdt_ptr) : : "memory");

    uint64_t* gdt = (uint64_t*)gdt_ptr.base;
    int max_entries = (gdt_ptr.limit + 1) / 8;

    serial_print("\n=== GDT DUMP ===\n");
    vga_print("=== GDT DUMP ===\n");

    serial_print("GDT base=0x"); serial_print_hex(gdt_ptr.base);
    serial_print(" limit=0x"); serial_print_hex(gdt_ptr.limit);
    serial_print(" entries="); serial_print_dec(max_entries);
    serial_print("\n");
    vga_print("GDT base=0x"); vga_print_hex_cur(gdt_ptr.base);
    vga_print(" limit=0x"); vga_print_hex_cur(gdt_ptr.limit);
    vga_print("\n");

    for (int i = 0; i < max_entries; i++) {
        uint64_t desc = gdt[i];
        uint8_t access  = (desc >> 40) & 0xFF;
        uint8_t flags   = (desc >> 52) & 0xF;
        uint8_t dpl     = (access >> 5) & 0x3;
        uint8_t type    = (access >> 1) & 0x7;
        bool present    = (access >> 7) & 0x1;
        bool is_system  = !((access >> 4) & 0x1);
        bool is_code    = (access >> 3) & 0x1;
        bool is_64bit   = (flags & 0x2) ? true : false;

        serial_print("["); serial_print_dec(i); serial_print("] 0x");
        serial_print_hex(i * 8);
        serial_print(" = 0x"); serial_print_hex(desc);
        serial_print("  ");
        vga_print("["); vga_print_dec_cur(i); vga_print("] 0x");
        vga_print_hex_cur(i * 8);
        vga_print(" = 0x"); vga_print_hex_cur(desc);
        vga_print("  ");

        const char* kind;
        if (!present) {
            kind = "NotPresent";
        } else if (is_system) {
            kind = (type == 9) ? "TSS" : "System";
        } else if (is_code) {
            kind = "Code";
        } else {
            kind = "Data";
        }

        serial_print(kind);
        vga_print(kind);

        if (present) {
            serial_print(" DPL="); serial_print_dec(dpl);
            vga_print(" DPL="); vga_print_dec_cur(dpl);
            if (is_64bit) {
                serial_print(" 64-bit");
                vga_print(" 64-bit");
            }
        }
        serial_print("\n");
        vga_print("\n");
    }

    serial_print("=== END GDT DUMP ===\n\n");
    vga_print("=== END GDT DUMP ===\n");
}
