#ifndef SYSCALL_H
#define SYSCALL_H

#include <stddef.h>

#define SYS_WRITE 1
#define SYS_EXIT  2
#define SYS_READ  3
#define SYS_BRK   10
#define SYS_ARCH_SET_FS 11

#define SYS_PROCLIST    20

/* Register the custom hardware reboot system call index macro */
#define SYS_REBOOT      25

/*
 * File syscalls.  SYS_OPEN and SYS_CLOSE are implemented in
 * user_syscall.c (they take a user pointer and a mode); the numbers
 * are listed here for reference and for the dispatcher.
 */
#define SYS_OPEN   4
#define SYS_CLOSE  6
#define SYS_UNLINK 7

long sys_write(int fd, const void* buf, size_t count);
void sys_exit(int status);
long sys_read(int fd, void* buf, size_t count);
void* sys_brk(long inc);
void sys_arch_set_fs(void* base);

/*
 * Kernel-side handlers.  Called by the dispatcher and, for
 * SYS_UNLINK, by the kernel shell if desired.
 */
long sys_open(const char* path, int flags);
long sys_close(int fd);
long sys_unlink(const char* path);

#endif
