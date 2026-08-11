#include "include/ring3.h"
#include "include/vga.h"
#include "include/serial.h"
#include "include/vmm.h"
#include "include/pmm.h"
#include "include/gdt.h"
#include <stdint.h>

extern void jump_to_user_mode(uint64_t entry, uint64_t stack);

__attribute__((noreturn)) void ring3_enter(uint64_t entry, uint64_t stack, uint64_t arg1, uint64_t arg2) {
    (void)arg1; (void)arg2;
    
    serial_print("=== RING3 ENTRY ===\n");
    serial_print("Ring3: entry=0x");
    serial_print_hex(entry);
    serial_print(" stack=0x");
    serial_print_hex(stack);
    serial_print("\n");
    
    vga_print("Ring3: Entering...\n");
    
    serial_print("Ring3: Disabling interrupts and calling jump_to_user_mode\n");
    
    // Disable interrupts before entering user mode
    __asm__ volatile ("cli");
    
    // Call the assembly function directly - arguments in RDI, RSI
    jump_to_user_mode(entry, stack);
    
    // Should never reach here
    while(1) __asm__("hlt");
}

void create_user_process(void (*entry)(void*), void* arg) {
    (void)entry;
    (void)arg;
    
    serial_print("=== CREATING USER PROCESS ===\n");
    vga_print("Creating user process...\n");
    
    uint64_t user_stack = USER_STACK_BASE;
    uint64_t phys_stack = pmm_alloc_page();
    if (!phys_stack) {
        serial_print("Failed to allocate user stack\n");
        return;
    }
    vmm_map_page(user_stack, phys_stack, 0x7);
    serial_print("User stack at 0x");
    serial_print_hex(user_stack);
    serial_print(" (phys 0x");
    serial_print_hex(phys_stack);
    serial_print(")\n");
    
    uint64_t user_code_base = USER_CODE_BASE;
    uint64_t phys_code = pmm_alloc_page();
    if (!phys_code) {
        serial_print("Failed to allocate user code page\n");
        return;
    }
    vmm_map_page(user_code_base, phys_code, 0xF);
    serial_print("User code at 0x");
    serial_print_hex(user_code_base);
    serial_print(" (phys 0x");
    serial_print_hex(phys_code);
    serial_print(")\n");
    
    // Write the user code as bytes: EB FE = jmp $ (infinite loop)
    uint8_t* code_ptr = (uint8_t*)user_code_base;
    code_ptr[0] = 0xEB;  // jmp rel8
    code_ptr[1] = 0xFE;  // -2 (jump to itself)
    serial_print("User code written: EB FE\n");
    
    // Optionally add more code for a better test
    // For example, write a syscall to exit or print
    // But EB FE is enough to prove user mode works!
    
    gdt_debug_print();
    
    uint64_t user_stack_top = user_stack + 4096 - 16;
    
    serial_print("create_user_process: About to enter ring3\n");
    serial_print("  Entry: 0x");
    serial_print_hex(user_code_base);
    serial_print("  Stack: 0x");
    serial_print_hex(user_stack_top);
    serial_print("\n");
    
    ring3_enter(user_code_base, user_stack_top, (uint64_t)arg, 0);
}

// Add this to ring3.c - it's just a simple function that runs in user mode
void simple_user_test(void) {
    // Write a single character to VGA
    volatile uint16_t* vga = (volatile uint16_t*)0xB8000;
    vga[0] = 0x0F55;  // 'U' at position 0
    
    // Loop forever
    while(1) {
        __asm__ volatile ("hlt");
    }
}

void user_test(void* arg) {
    (void)arg;
    vga_print("USER: This should never be called directly!\n");
}

void user_test2(void* arg) {
    (void)arg;
    vga_print("USER2: This should never be called directly!\n");
}
