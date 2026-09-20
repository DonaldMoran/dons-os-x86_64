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

/*
 * SYS_EXEC (8) — spawn a new process from an ELF on the FAT volume.
 *
 * Spawn semantics, not execve.  Creates a new process, loads the
 * named ELF into it, queues it, and returns its pid.  The calling
 * process is untouched and continues running.
 *
 * arg0: const char* path (user pointer, NUL-terminated, "0:/NAME.EXT")
 * returns: pid (>0) on success, -1 on failure
 */
#define SYS_EXEC 8

/*
 * SYS_WAITPID (9) — wait for a child to exit.
 *
 * arg0: long pid   (child pid, or (long)-1 to wait for any child)
 * arg1: int* status  (user pointer to receive the child's exit status;
 *                     may be NULL)
 * arg2: int options  (bit 0 set = WNOHANG: return 0 if no child has
 *                     exited yet)
 * returns: the reaped child's pid, 0 if WNOHANG and no child exited,
 *          -1 on error
 */
#define SYS_WAITPID 9
#define WNOHANG 1

long sys_write(int fd, const void* buf, size_t count);
void sys_exit(int status);
long sys_read(int fd, void* buf, size_t count);
void* sys_brk(long inc);
void sys_arch_set_fs(void* base);

/*
 * Kernel-side handlers.  Called by the dispatcher.
 */
long sys_open(const char* path, int flags);
long sys_close(int fd);
long sys_unlink(const char* path);
long sys_exec(const char* user_path);
long sys_waitpid(long pid, int* user_status, int options);

#endif
