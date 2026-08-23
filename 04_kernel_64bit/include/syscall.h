#ifndef SYSCALL_H
#define SYSCALL_H

#include <stddef.h>

// Syscall numbers
#define SYS_WRITE 1
#define SYS_EXIT  2
#define SYS_READ  3

// Function prototypes
long sys_write(int fd, const char* buf, size_t count);
void sys_exit(int status);
long sys_read(int fd, void* buf, size_t count);

#endif
