#include "include/syscall.h"
#include "include/vga.h"
#include "include/serial.h"
#include <stddef.h>
#include <stdint.h>

// System call dispatcher - called from assembly
long syscall_dispatcher(uint64_t syscall_num, uint64_t arg1, uint64_t arg2, 
                         uint64_t arg3, uint64_t arg4, uint64_t arg5) {
    long ret = -1;
    
    (void)arg4;  // Suppress unused parameter warning
    (void)arg5;  // Suppress unused parameter warning
    
    switch (syscall_num) {
        case SYS_WRITE:
            ret = sys_write((uint32_t)arg1, (const char*)arg2, (size_t)arg3);
            break;
        case SYS_EXIT:
            sys_exit((int)arg1);
            ret = 0; // Won't return
            break;
        default:
            serial_print("Unknown syscall: ");
            serial_print_hex(syscall_num);  // Use existing serial_print_hex
            serial_print("\n");
            ret = -1;
            break;
    }
    
    return ret;
}

// SYS_WRITE implementation
long sys_write(uint32_t fd, const char* buf, size_t count) {
    // Only support stdout/stderr for now
    if (fd == 1 || fd == 2) {
        // Write to VGA console
        for (size_t i = 0; i < count && buf[i]; i++) {
            vga_putc(buf[i]);
        }
        // Also write to serial for debugging using existing serial_putc
        for (size_t i = 0; i < count && buf[i]; i++) {
            serial_putc(buf[i]);
        }
        return count;
    }
    return -1;
}

// SYS_EXIT implementation
void sys_exit(int status) {
    serial_print("Process exited with status: ");
    // Use existing serial_print_hex for simplicity
    serial_print_hex((uint64_t)status);
    serial_print("\n");
    // For now, just halt
    // Later: schedule next process
    while(1) {
        __asm__ volatile("hlt");
    }
}

// Initialize syscall support
void syscall_init(void) {
    // Call the assembly initialization
    extern void syscall_init_asm(void);
    syscall_init_asm();
    
    serial_print("SYSCALL initialized\n");
    vga_print("SYSCALL support enabled\n");
}
