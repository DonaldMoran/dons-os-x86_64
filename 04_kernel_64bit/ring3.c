#include <stdint.h>
#include "include/serial.h"

extern void jump_to_user_mode(uint64_t entry, uint64_t stack);

void ring3_enter(uint64_t entry, uint64_t stack) {
    serial_print("R3: Entering user mode at 0x");
    serial_print_hex(entry);
    serial_print(" with stack 0x");
    serial_print_hex(stack);
    serial_print("\n");
    
    // This should never return
    jump_to_user_mode(entry, stack);
    
    // If it does, hang
    while(1) __asm__ volatile("hlt");
}
