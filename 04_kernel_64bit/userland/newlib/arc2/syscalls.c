#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>
#include <stdint.h>

// PRESERVED: Retaining your system's original historic system call mapping vectors cleanly
#define SYS_WRITE   1
#define SYS_EXIT    2
#define SYS_READ    3
#define SYS_BRK     10
#define SYS_REBOOT  25

static inline int64_t syscall3(uint64_t nr, uint64_t a0, uint64_t a1, uint64_t a2) {
    int64_t ret;
    __asm__ volatile (
        "syscall"
        : "=a"(ret)
        : "a"(nr), "D"(a0), "S"(a1), "d"(a2)
        : "rcx", "r11", "memory"
    );
    return ret;
}

void _init(void) {}
void _fini(void) {}

ssize_t write(int fd, const void *buf, size_t count) {
    return (ssize_t)syscall3(SYS_WRITE, (uint64_t)fd, (uint64_t)buf, (uint64_t)count);
}

ssize_t read(int fd, void *buf, size_t count) {
    return (ssize_t)syscall3(SYS_READ, (uint64_t)fd, (uint64_t)buf, (uint64_t)count);
}

void exit(int status) {
    syscall3(SYS_EXIT, (uint64_t)status, 0, 0);
    while (1) {
        __asm__ volatile("hlt");
    }
}
// FIXED: Dynamic memory allocator tracker that queries your kernel's native 
// 0x8000200000 heap base location automatically to eliminate address pointer gaps.
void *sbrk(ptrdiff_t incr) {
    static int64_t heap_end_cached = 0;

    // Dynamically retrieve the current base address directly from your kernel's sys_brk(0)
    if (heap_end_cached == 0) {
        int64_t current_break = syscall3(SYS_BRK, 0, 0, 0);
        if (current_break == -1) {
            errno = ENOMEM;
            return (void *)-1;
        }
        heap_end_cached = current_break;
    }

    if (incr == 0) {
        return (void *)heap_end_cached;
    }

    int64_t extended_break = syscall3(SYS_BRK, (uint64_t)incr, 0, 0);
    if (extended_break == -1) {
        errno = ENOMEM;
        return (void *)-1;
    }

    void *previous_heap_boundary = (void *)heap_end_cached;
    heap_end_cached = extended_break; 
    return previous_heap_boundary;
}

/* ---------------------------------------------------------------------------
 * ARCHITECTURAL WRAPPERS WITH UNDERSCORES (FOR STRUCTURAL REDUNDANCY)
 * --------------------------------------------------------------------------- */

ssize_t _write(int fd, const void *buf, size_t count) { return write(fd, buf, count); }
ssize_t _read(int fd, void *buf, size_t count) { return read(fd, buf, count); }
void _exit(int status) { exit(status); }
void *_sbrk(ptrdiff_t incr) { return sbrk(incr); }

/* ---------------------------------------------------------------------------
 * DONSDOS CUSTOM EXTENSION SYSTEM VECTORS
 * --------------------------------------------------------------------------- */

void sys_reboot(void) {
    syscall3(SYS_REBOOT, 0, 0, 0);
}

/* ---------------------------------------------------------------------------
 * REQUIRED STRUCTURAL LINKS TO SATISFY LINKER SCHEMATICS
 * --------------------------------------------------------------------------- */

int close(int fd) { (void)fd; return -1; }

// FIXED: Explicitly populates the character device attribute flag (S_IFCHR)
// to verify to Newlib that stdout/stderr are active interactive console streams.
int fstat(int fd, struct stat *st) { 
    if (fd == 1 || fd == 2) {
        st->st_mode = S_IFCHR; 
        return 0; 
    }
    return -1; 
}

int isatty(int fd) { if (fd == 1 || fd == 2) return 1; return 0; }
off_t lseek(int fd, off_t offset, int whence) { (void)fd; (void)offset; (void)whence; return -1; }
int open(const char *path, int flags, int mode) { (void)path; (void)flags; (void)mode; return -1; }
int kill(int pid, int sig) { (void)pid; (void)sig; return -1; }
int getpid(void) { return 1; }

int _close(int fd) { return close(fd); }
int _fstat(int fd, struct stat *st) { return fstat(fd, st); }
int _isatty(int fd) { return isatty(fd); }
off_t _lseek(int fd, off_t offset, int whence) { return lseek(fd, offset, whence); }
int _open(const char *path, int flags, int mode) { return open(path, flags, mode); }
int _kill(int pid, int sig) { return kill(pid, sig); }
int _getpid(void) { return getpid(); }
