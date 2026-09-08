#ifndef SYSCALL_H
#define SYSCALL_H

#include <stddef.h>

#define SYS_WRITE 1
#define SYS_EXIT  2
#define SYS_READ  3
#define SYS_BRK   10
#define SYS_ARCH_SET_FS 11

#define SYS_PROCLIST    20 

// Register the custom hardware reboot system call index macro
#define SYS_REBOOT      25

long sys_write(int fd, const void* buf, size_t count);
void sys_exit(int status);
long sys_read(int fd, void* buf, size_t count);
void* sys_brk(long inc);
void sys_arch_set_fs(void* base);

void sys_proclist(void);

// Add the matching userland function prototype wrapper
void sys_reboot(void);

#endif
