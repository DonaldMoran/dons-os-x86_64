#include "include/syscall.h"
#include "include/serial.h"
#include "include/vga.h"
#include <stddef.h>

long sys_write(int fd, const char* buf, size_t count) {
    (void)fd;
    (void)buf;
    (void)count;
    serial_print("sys_write called from kernel\n");
    return count;
}

void sys_exit(int status) {
    (void)status;
    serial_print("sys_exit called from kernel\n");
    while(1) __asm__ volatile("hlt");
}

long sys_read(int fd, void* buf, size_t count) {
    (void)fd;
    (void)buf;
    (void)count;
    serial_print("sys_read called from kernel\n");
    return 0;
}
