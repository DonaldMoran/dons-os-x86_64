#include <stddef.h>
#include "include/tss.h"
#include "include/gdt.h"
#include "include/vga.h"
#include "include/serial.h"
#include "include/vmm.h"

// ============================================================
// Use a fixed pointer to physical address 0x5000
// ============================================================
static tss_t* tss = (tss_t*)TSS_PHYS_ADDR;
static uint8_t* iomap = (uint8_t*)(TSS_PHYS_ADDR + sizeof(tss_t));
// ============================================================

// Kernel stack in normal kernel memory
// static uint64_t kernel_stack[4096] __attribute__((aligned(16)));
// Kernel stack in normal kernel memory
uint64_t kernel_stack[4096] __attribute__((aligned(16)));
// Export the top-of-stack symbol
uint64_t *kernel_stack_top = &kernel_stack[4096];


static int tss_loaded = 0;

void tss_init(void) {
    serial_print("=== TSS INIT START ===\n");
    vga_print("TSS: Init start\n");
    
    serial_print("TSS: vmm_get_phys(0x5000) = 0x");
    serial_print_hex(vmm_get_phys(0x5000));
    serial_print("\n");
    
    // ============================================================
    // DEBUG: Test access to 0x5000
    // ============================================================
    serial_print("TSS: probing 0x5000...\n");
    volatile uint64_t *probe = (uint64_t *)0x5000;
    uint64_t tmp = *probe;
    serial_print("TSS: read ok\n");
    *probe = tmp;
    serial_print("TSS: write ok\n");
    // ============================================================

    // ============================================================
    // DEBUG: Check tss pointer value
    // ============================================================
    serial_print("TSS: tss pointer = 0x");
    serial_print_hex((uint64_t)tss);
    serial_print("\n");
    serial_print("TSS: sizeof(tss_t) = 0x");
    serial_print_hex(sizeof(tss_t));
    serial_print("\n");
    serial_print("TSS: TSS_TOTAL_SIZE = 0x");
    serial_print_hex(TSS_TOTAL_SIZE);
    serial_print("\n");
    serial_print("TSS: iomap pointer = 0x");
    serial_print_hex((uint64_t)iomap);
    serial_print("\n");
    // ============================================================

    // ============================================================
    // Zero the entire TSS region (TSS + I/O Permission Bitmap)
    // ============================================================
    
    serial_print("TSS: zeroing range [0x");
    serial_print_hex((uint64_t)tss);
    serial_print(" .. 0x");
    serial_print_hex((uint64_t)tss + TSS_TOTAL_SIZE);
    serial_print(")\n");

    // Zero everything - TSS + I/O Permission Bitmap
    uint8_t* buffer_ptr = (uint8_t*)tss;
    for (size_t i = 0; i < TSS_TOTAL_SIZE; i++) {
        buffer_ptr[i] = 0;
        // Print progress every 1024 bytes
        if (i % 1024 == 0 && i > 0) {
            serial_print("TSS: zeroed 0x");
            serial_print_hex(i);
            serial_print(" bytes\n");
        }
    }
    serial_print("TSS: Zeroing loop completed\n");
    // ============================================================
    
    // ============================================================
    // Set up I/O Permission Bitmap (all ports allowed)
    // ============================================================
    iomap[TSS_IOMAP_SIZE] = 0xFF;
    serial_print("TSS: I/O Permission Bitmap terminator set at 0x");
    serial_print_hex((uint64_t)&iomap[TSS_IOMAP_SIZE]);
    serial_print("\n");
    // ============================================================
    
    // ============================================================
    // Set RSP0 to top of kernel_stack
    // ============================================================
    serial_print("TSS: Setting rsp0...\n");
    uint64_t kernel_stack_top = (uint64_t)&kernel_stack[4096];
    tss->rsp0 = kernel_stack_top;
    serial_print("TSS: rsp0 = 0x");
    serial_print_hex(tss->rsp0);
    serial_print("\n");
    // ============================================================
    
    // ============================================================
    // Set I/O bitmap base = end of TSS
    // ============================================================
    serial_print("TSS: Setting iopb_base...\n");
    tss->iopb_base = sizeof(tss_t);
    serial_print("TSS: iopb_base = 0x");
    serial_print_hex(tss->iopb_base);
    serial_print("\n");
    // ============================================================
    
    // ============================================================
    // Point GDT TSS descriptor at our TSS
    // ============================================================
    serial_print("TSS: Calling gdt_set_tss...\n");
    gdt_set_tss((uint64_t)tss, TSS_TOTAL_SIZE - 1);
    serial_print("TSS: gdt_set_tss completed\n");
    // ============================================================
    
    // ============================================================
    // Load TSS with ltr
    // ============================================================
    if (!tss_loaded) {
        serial_print("TSS: Loading TR with selector 0x38...\n");
        vga_print("TSS: Loading TR...\n");
        serial_print("TSS: about to execute ltr 0x38\n");
        __asm__ volatile (
            "cli\n\t"
            "ltr %%ax\n\t"
            "sti"
            :
            : "a"(0x38)
            : "memory"
        );
        serial_print("TSS: ltr completed successfully\n");
        tss_loaded = 1;
        serial_print("TSS: TR loaded\n");
    }
    // ============================================================
    
    // ============================================================
    // Verify TR register
    // ============================================================
    uint16_t tr;
    __asm__ volatile (
        "str %%ax"
        : "=a"(tr)
        :
        : "memory"
    );

    serial_print("TSS: TR register = 0x");
    serial_print_hex(tr);
    serial_print("\n");
    // ============================================================
    
    gdt_debug_print();

    if (tr == 0x38) {
        serial_print("TSS: TR loaded correctly!\n");
        vga_print("TSS: TR=0x38 OK\n");
    } else {
        serial_print("TSS: ERROR - TR not loaded correctly!\n");
        vga_print("TSS: TR load FAILED\n");
    }

    serial_print("=== TSS INIT COMPLETE ===\n");
    vga_print("TSS: Initialized\n");
}

void tss_set_kernel_stack(uint64_t stack) {
    tss->rsp0 = stack;
}
