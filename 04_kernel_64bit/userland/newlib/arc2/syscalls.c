#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>

// PRESERVED: Retaining your system's original historic system call mapping vectors cleanly
#define SYS_WRITE   1
#define SYS_EXIT    2
#define SYS_READ    3
#define SYS_OPEN    4
#define SYS_CLOSE   6
#define SYS_UNLINK  7
#define SYS_EXEC    8
#define SYS_WAITPID 9
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

void *sbrk(ptrdiff_t incr) {
    static int64_t heap_end_cached = 0;

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

int open(const char *path, int flags, int mode) {
    return (int)syscall3(SYS_OPEN, (uint64_t)path, (uint64_t)flags, (uint64_t)mode);
}

int close(int fd) {
    return (int)syscall3(SYS_CLOSE, (uint64_t)fd, 0, 0);
}

int unlink(const char *path) {
    return (int)syscall3(SYS_UNLINK, (uint64_t)path, 0, 0);
}

/* ---------------------------------------------------------------------------
 * spawn() — dons-os-specific, NOT POSIX.
 *
 * Creates a new process from the named ELF on the FAT volume and
 * returns its pid.  The caller keeps running; the child runs whenever
 * the scheduler picks it.  Use waitpid() to wait for it.
 *
 * See apps/include/donsdos.h for the declaration.
 * --------------------------------------------------------------------------- */
int spawn(const char *path) {
    return (int)syscall3(SYS_EXEC, (uint64_t)path, 0, 0);
}

/* ---------------------------------------------------------------------------
 * waitpid() — POSIX-compatible three-argument form.
 *
 * Blocks until a matching child exits, then returns its pid and writes
 * its exit status to *status (if status is non-NULL).  With
 * options & WNOHANG, returns 0 immediately if no matching child has
 * exited yet.
 * --------------------------------------------------------------------------- */
int waitpid(int pid, int *status, int options) {
    return (int)syscall3(SYS_WAITPID,
                         (uint64_t)(int64_t)pid,
                         (uint64_t)status,
                         (uint64_t)options);
}

ssize_t _write(int fd, const void *buf, size_t count) { return write(fd, buf, count); }
ssize_t _read(int fd, void *buf, size_t count) { return read(fd, buf, count); }
void _exit(int status) { exit(status); }
void *_sbrk(ptrdiff_t incr) { return sbrk(incr); }

void sys_reboot(void) {
    fflush(NULL);
    syscall3(SYS_REBOOT, 0, 0, 0);
}

int fstat(int fd, struct stat *st) { 
    if (fd == 1 || fd == 2) {
        st->st_mode = S_IFCHR; 
        return 0; 
    }
    return -1; 
}

int isatty(int fd) { if (fd == 1 || fd == 2) return 1; return 0; }
off_t lseek(int fd, off_t offset, int whence) { (void)fd; (void)offset; (void)whence; return -1; }
int kill(int pid, int sig) { (void)pid; (void)sig; return -1; }
int getpid(void) { return 1; }

int _close(int fd) { return close(fd); }
int _fstat(int fd, struct stat *st) { return fstat(fd, st); }
int _isatty(int fd) { return isatty(fd); }
off_t _lseek(int fd, off_t offset, int whence) { return lseek(fd, offset, whence); }
int _open(const char *path, int flags, int mode) { return open(path, flags, mode); }
int _kill(int pid, int sig) { return kill(pid, sig); }
int _getpid(void) { return getpid(); }
