#include "include/syscall.h"
#include "include/vga.h"
#include "include/serial.h"
#include "include/vmm.h"
#include <stddef.h>
#include <stdint.h>

long sys_write(uint32_t fd, const char* buf, size_t count) {
    serial_print("sys_write: fd=");
    serial_print_dec(fd);
    serial_print(" buf=0x");
    serial_print_hex((uint64_t)buf);
    serial_print(" count=");
    serial_print_dec(count);
    serial_print("\n");

    // Dump page table for the user buffer
    vmm_dump_page_table((uint64_t)buf);

    if (fd == 1 || fd == 2) {
        for (size_t i = 0; i < count && buf[i]; i++) {
            vga_putc(buf[i]);
            serial_putc(buf[i]);
        }
        return count;
    }
    return -1;
}

void sys_exit(int status) {
    serial_print("Exit: ");
    serial_print_dec(status);
    serial_print("\n");
    while(1) __asm__("hlt");
}


