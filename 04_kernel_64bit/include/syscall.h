#ifndef SYSCALL_H
#define SYSCALL_H

#include <stdint.h>
#include <stddef.h>

// System call numbers
#define SYS_WRITE 1
#define SYS_EXIT  60

// Function prototypes
//~ void syscall_init(void);
void syscall_handler(void);

// System call functions (for kernel use)
long sys_write(uint32_t fd, const char* buf, size_t count);
void sys_exit(int status);

#endif
