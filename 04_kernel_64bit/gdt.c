#include "include/gdt.h"
#include "include/tss.h"
#include "include/serial.h"
#include <stdint.h>
#include "include/vga.h"

void gdt_init(void) {
    // If needed, you can implement GDT initialization here
    // But we're using the bootloader's GDT
    serial_print("GDT: Using bootloader GDT\n");
}

void gdt_reload(void) {
    // Reload segment registers
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
    serial_print("GDT: Reloaded segment registers\n");
}

void gdt_set_tss(uint64_t tss_addr, uint32_t tss_size) {
    serial_print("GDT: Setting TSS in bootloader GDT...\n");
    
    // Get the current GDT base address
    struct {
        uint16_t limit;
        uint64_t base __attribute__((packed));
    } gdt_ptr;
    
    __asm__ volatile (
        "sgdt %0\n"
        : "=m"(gdt_ptr)
        : : "memory"
    );
    
    uint64_t gdt_addr = gdt_ptr.base;
    
    serial_print("GDT: Bootloader GDT at 0x");
    serial_print_hex(gdt_addr);
    serial_print(" (limit: 0x");
    serial_print_hex(gdt_ptr.limit);
    serial_print(")\n");
    
    // TSS descriptor is at offset 0x38 (2 entries: low and high)
    uint64_t* tss_desc_low = (uint64_t*)(gdt_addr + 0x38);
    uint64_t* tss_desc_high = (uint64_t*)(gdt_addr + 0x40);
    
    serial_print("GDT: TSS descriptor at GDT+0x38\n");
    serial_print("GDT: TSS base=0x");
    serial_print_hex(tss_addr);
    serial_print(" size=0x");
    serial_print_hex(tss_size);
    serial_print("\n");
    
    // Build the TSS descriptor (64-bit TSS)
    uint64_t desc_low = 0;
    uint64_t desc_high = 0;
    
    uint64_t base = tss_addr;
    uint64_t limit = tss_size;
    
    // ============================================================
    // FIX: Make sure the TSS limit includes the I/O Permission Bitmap
    // The limit should be at least TSS_TOTAL_SIZE - 1
    // ============================================================
    if (limit < 0x67) {
        limit = 0x67;  // Minimum TSS size for 64-bit
    }
    
    // Low 64 bits
    desc_low |= (limit & 0xFFFF);                    // Limit[15:0]
    desc_low |= ((base & 0xFFFFFF) << 16);           // Base[23:0]
    desc_low |= ((uint64_t)0x89 << 40);              // Access byte (64-bit TSS, present, DPL=0)
    desc_low |= ((limit >> 16) & 0xF) << 48;         // Limit[19:16]
    // Flags: G=0, D=0, L=0, AVL=0 - all zeros at bits 52-55
    desc_low |= ((base >> 24) & 0xFF) << 56;         // Base[31:24]
    
    // High 64 bits
    desc_high = (base >> 32) & 0xFFFFFFFF;           // Base[63:32] in bits 0–31
    
    // Write the descriptor
    *tss_desc_low = desc_low;
    *tss_desc_high = desc_high;
    
    serial_print("GDT: TSS descriptor low=0x");
    serial_print_hex(desc_low);
    serial_print("\n");
    serial_print("GDT: TSS descriptor high=0x");
    serial_print_hex(desc_high);
    serial_print("\n");
    
    // Verify the descriptor was written correctly
    serial_print("GDT: Reading back TSS descriptor...\n");
    serial_print("  Low: 0x");
    serial_print_hex(*tss_desc_low);
    serial_print("\n  High: 0x");
    serial_print_hex(*tss_desc_high);
    serial_print("\n");
    
    serial_print("GDT: TSS descriptor set successfully\n");
}

void gdt_debug_print(void) {
    // Get GDT base
    struct {
        uint16_t limit;
        uint64_t base __attribute__((packed));
    } gdt_ptr;
    
    __asm__ volatile (
        "sgdt %0\n"
        : "=m"(gdt_ptr)
        : : "memory"
    );
    
    serial_print("\n=== GDT DEBUG ===\n");
    serial_print("GDT Base: 0x");
    serial_print_hex(gdt_ptr.base);
    serial_print(" Limit: 0x");
    serial_print_hex(gdt_ptr.limit);
    serial_print("\n");
    
    // Print ALL GDT entries
    uint64_t* gdt = (uint64_t*)gdt_ptr.base;
    
    for (int i = 0; i <= 8; i++) {
        int offset = i * 8;
        
        serial_print("GDT+0x");
        serial_print_hex(offset);
        serial_print(" (selector 0x");
        serial_print_hex(offset);
        serial_print("): 0x");
        serial_print_hex(gdt[i]);
        
        // Decode the entry
        uint8_t access = (gdt[i] >> 40) & 0xFF;
        uint8_t flags = (gdt[i] >> 52) & 0xF;
        uint8_t dpl = (access >> 5) & 0x3;
        uint8_t type = (access >> 1) & 0x7;
        bool present = (access >> 7) & 0x1;
        bool is_system = !((access >> 4) & 0x1);
        bool is_code = (access >> 3) & 0x1;
        bool is_64bit = (flags & 0x2) ? true : false;
        bool is_user = (dpl == 3);
        
        serial_print(" [");
        if (!present) {
            serial_print("Not Present");
        } else if (is_system) {
            if (type == 9) serial_print("TSS");
            else serial_print("System");
        } else if (is_code) {
            serial_print("Code");
        } else {
            serial_print("Data");
        }
        
        if (present) {
            serial_print(" DPL=");
            serial_print_dec(dpl);
            if (is_64bit) serial_print(" 64-bit");
            if (is_user) serial_print(" USER");
        }
        serial_print("]\n");
    }
    
    serial_print("=== END GDT DEBUG ===\n\n");
}

void gdt_fix_user_segments(void) {
    vga_print("GDT: Fixing user segments for Ring 3...\n");

    struct {
        uint16_t limit;
        uint64_t base __attribute__((packed));
    } gdt_ptr;

    __asm__ volatile ("sgdt %0" : "=m"(gdt_ptr));

    uint64_t* gdt = (uint64_t*)gdt_ptr.base;

    uint64_t old_user_code = gdt[GDT_USER_CODE / 8];
    uint64_t old_user_data = gdt[GDT_USER_DATA / 8];

    serial_print("GDT: Old user code at 0x");
    serial_print_hex(GDT_USER_CODE);
    serial_print(" = 0x");
    serial_print_hex(old_user_code);
    serial_print("\n");

    serial_print("GDT: Old user data at 0x");
    serial_print_hex(GDT_USER_DATA);
    serial_print(" = 0x");
    serial_print_hex(old_user_data);
    serial_print("\n");

    // User data segment at 0x28: 64-bit data, DPL=3, Present, Writable
    gdt[GDT_USER_DATA / 8] = 0x00CFF2000000FFFFULL;

    // User code segment at 0x30: 64-bit code, DPL=3, Present, Executable, Readable
    gdt[GDT_USER_CODE / 8] = 0x00AFFA000000FFFFULL;

    serial_print("GDT: New user data at 0x");
    serial_print_hex(GDT_USER_DATA);
    serial_print(" = 0x");
    serial_print_hex(gdt[GDT_USER_DATA / 8]);
    serial_print("\n");

    serial_print("GDT: New user code at 0x");
    serial_print_hex(GDT_USER_CODE);
    serial_print(" = 0x");
    serial_print_hex(gdt[GDT_USER_CODE / 8]);
    serial_print("\n");

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
    vga_print("GDT: User segments fixed for Ring 3\n");
}


void gdt_dump_entry(int index) {
    // Get current GDT pointer
    struct {
        uint16_t limit;
        uint64_t base __attribute__((packed));
    } gdt_ptr;

    __asm__ volatile ("sgdt %0" : "=m"(gdt_ptr));

    uint64_t *gdt = (uint64_t *)gdt_ptr.base;
    int max_entries = (gdt_ptr.limit + 1) / 8;  // each entry is 8 bytes

    if (index < 0 || index >= max_entries) {
        vga_print("Invalid GDT index\n");
        return;
    }

    uint64_t desc = gdt[index];

    vga_print("GDT[");
    vga_print_dec_cur(index);
    vga_print("] = 0x");
    vga_print_hex_cur(desc);
    vga_print("\n");
}
