#include <sys/stat.h>
#include <errno.h>
#include <reent.h>
#include <stddef.h>
#include <stdint.h>

#define SYS_WRITE 1
#define SYS_READ  3
#define SYS_BRK   10
#define SYS_EXIT  2

// 1. Force strict uint64_t inputs to lock down correct 64-bit AMD64 System V register assignment
static inline int64_t syscall(uint64_t nr, uint64_t a0, uint64_t a1, uint64_t a2) {
    int64_t ret;
    __asm__ volatile (
        "syscall"
        : "=a"(ret)
        : "a"(nr), "D"(a0), "S"(a1), "d"(a2)
        : "rcx", "r11", "memory"
    );
    return ret;
}

_ssize_t _write_r(struct _reent *r, int fd, const void *buf, size_t count) {
    (void)r;
    return (_ssize_t)syscall(SYS_WRITE, (uint64_t)fd, (uint64_t)buf, (uint64_t)count);
}

_ssize_t _read_r(struct _reent *r, int fd, void *buf, size_t count) {
    (void)r;
    return (_ssize_t)syscall(SYS_READ, (uint64_t)fd, (uint64_t)buf, (uint64_t)count);
}

void *_sbrk_r(struct _reent *r, ptrdiff_t incr) {
    (void)r;
    return (void*)syscall(SYS_BRK, (uint64_t)incr, 0, 0);
}

void _exit_r(struct _reent *r, int status) {
    (void)r;
    syscall(SYS_EXIT, (uint64_t)status, 0, 0);
    while (1) __asm__ volatile("hlt");
}

void _exit(int status) {
    _exit_r(_impure_ptr, status);
}

// Minimal stubs required to resolve standard C structural linkages smoothly
int _close_r(struct _reent *r, int fd) { (void)r; (void)fd; return -1; }
int _fstat_r(struct _reent *r, int fd, struct stat *st) { (void)r; (void)fd; (void)st; return -1; }
int _isatty_r(struct _reent *r, int fd) { (void)r; (void)fd; return 1; }
off_t _lseek_r(struct _reent *r, int fd, off_t offset, int whence) { (void)r; (void)fd; (void)offset; (void)whence; return -1; }
int _open_r(struct _reent *r, const char *path, int flags, int mode) { (void)r; (void)path; (void)flags; (void)mode; return -1; }
int _kill_r(struct _reent *r, int pid, int sig) { (void)r; (void)pid; (void)sig; return -1; }
int _getpid_r(struct _reent *r) { (void)r; return 1; }
