#include <stddef.h>
#include "include/tss.h"
#include "include/gdt.h"
#include "include/vga.h"
#include "include/serial.h"

#define TSS_PHYS 0x5000

static int tss_loaded = 0;

void tss_init(void) {
    serial_print("=== TSS INIT START ===\n");
    vga_print("TSS: Init start\n");
    
    tss_t* tss = (tss_t*)TSS_PHYS;
    serial_print("TSS: Zeroing TSS at 0x");
    serial_print_hex((uint64_t)tss);
    serial_print("\n");
    
    uint64_t* tss_ptr = (uint64_t*)tss;
    for (size_t i = 0; i < sizeof(tss_t) / 8; i++) {
        tss_ptr[i] = 0;
    }
    serial_print("TSS: Zeroed\n");
    
    tss->rsp0 = 0xFFFFFFFF80400000;
    serial_print("TSS: rsp0 = 0x");
    serial_print_hex(tss->rsp0);
    serial_print("\n");
    
    tss->iopb_base = sizeof(tss_t);
    serial_print("TSS: iopb_base = 0x");
    serial_print_hex(tss->iopb_base);
    serial_print("\n");
    
    serial_print("TSS: Setting GDT TSS descriptor...\n");
    gdt_set_tss((uint64_t)tss, sizeof(tss_t) - 1);
    
    if (!tss_loaded) {
        serial_print("TSS: Loading TR with selector 0x38...\n");
        vga_print("TSS: Loading TR...\n");
        
        __asm__ volatile (
            "cli\n\t"
            "ltr %%ax\n\t"
            "sti"
            : 
            : "a" (0x38)
            : "memory"
        );
        
        tss_loaded = 1;
        serial_print("TSS: TR loaded\n");
    }
    
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
    tss_t* tss = (tss_t*)TSS_PHYS;
    tss->rsp0 = stack;
}
