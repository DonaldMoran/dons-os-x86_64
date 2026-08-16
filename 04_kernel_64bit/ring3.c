#include "include/ring3.h"
#include "include/vga.h"
#include "include/serial.h"
#include "include/vmm.h"
#include "include/pmm.h"
#include "include/gdt.h"
#include <stdint.h>

extern uint64_t jump_to_user_mode(uint64_t entry, uint64_t stack,
                                  uint64_t arg1, uint64_t arg2);

/*
 * ring3_enter now RETURNS a uint64_t exit code.
 */
uint64_t ring3_enter(uint64_t entry, uint64_t stack,
                     uint64_t arg1 __attribute__((unused)),
                     uint64_t arg2 __attribute__((unused)))
{
    entry &= ~0x3ULL;   // clear CPL bits
    stack &= ~0xFULL;   // align stack

    serial_print("R3: entry=0x");
    serial_print_hex(entry);
    serial_print(" stack=0x");
    serial_print_hex(stack);
    serial_print("\n");

    // Call the ASM trampoline that iretq's into user mode
    uint64_t status = jump_to_user_mode(entry, stack, 0, 0);

    serial_print("R3: returned from usermode with status=");
    serial_print_hex(status);
    serial_print("\n");

    return status;
}

void create_user_process(void (*entry)(void*), void* arg) {
    serial_print("In create user");
    (void)entry;
    (void)arg;
    
    uint64_t user_stack = USER_STACK_BASE;
    uint64_t phys_stack = pmm_alloc_page();
    if (!phys_stack) return;
    vmm_map_page(user_stack, phys_stack, PT_PRESENT | PT_WRITE | PT_USER);
    
    uint64_t user_code_base = USER_CODE_BASE;
    uint64_t phys_code = pmm_alloc_page();
    if (!phys_code) return;
    vmm_map_page(user_code_base, phys_code, PT_PRESENT | PT_WRITE | PT_USER | PT_EXEC);
    
    uint8_t* code_ptr = (uint8_t*)user_code_base;
    code_ptr[0] = 0xEB;
    code_ptr[1] = 0xFE;
    
    uint64_t user_stack_top = user_stack + 4096 - 16;
    user_stack_top &= ~0xFULL;
    
    vmm_dump_page_table(0x400000);
    vmm_dump_page_table(user_stack_top);
    
    uint64_t status = ring3_enter(user_code_base, user_stack_top, (uint64_t)arg, 0);

    serial_print("User process exited with status=");
    serial_print_hex(status);
    serial_print("\n");
}

void simple_user_test(void) {
    volatile uint16_t* vga = (volatile uint16_t*)0xB8000;
    vga[0] = 0x0F55;
    while(1) __asm__ volatile ("hlt");
}

void user_test(void* arg) {
    (void)arg;
    vga_print("USER: This should never be called directly!\n");
}

void user_test2(void* arg) {
    (void)arg;
    vga_print("USER2: This should never be called directly!\n");
}
