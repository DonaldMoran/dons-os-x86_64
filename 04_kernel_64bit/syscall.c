#include "include/syscall.h"
#include "include/serial.h"
#include "include/vga.h"
#include <stddef.h>

// These are stubs for kernel-side syscalls (the actual handling is in user_syscall.c)
long sys_write(int fd, const void* buf, size_t count) {
    (void)fd;
    (void)buf;
    (void)count;
    serial_print("sys_write (kernel stub) called\n");
    return count;
}

void sys_exit(int status) {
    (void)status;
    serial_print("sys_exit (kernel stub) called\n");
    while(1) __asm__ volatile("hlt");
}

long sys_read(int fd, void* buf, size_t count) {
    (void)fd;
    (void)buf;
    (void)count;
    serial_print("sys_read (kernel stub) called\n");
    return 0;
}

void* sys_brk(long inc) {
    (void)inc;
    serial_print("sys_brk (kernel stub) called\n");
    return (void*)-1;
}

void sys_arch_set_fs(void* base) {
    (void)base;
    serial_print("sys_arch_set_fs (kernel stub) called\n");
}
