// syscalls.c - Picolibc syscall stubs using YOUR kernel's syscall numbers
#include <stdint.h>
#include <stddef.h>
#include <sys/stat.h>
#include <errno.h>

// Your syscall numbers (from syscall.h)
#define SYS_WRITE 1
#define SYS_EXIT  2
#define SYS_READ  3

// Syscall wrapper (matches your kernel's calling convention)
static inline long syscall(long num, long arg1, long arg2, long arg3) {
    long ret;
    __asm__ volatile (
        "syscall"
        : "=a"(ret)
        : "a"(num), "D"(arg1), "S"(arg2), "d"(arg3)
        : "rcx", "r11", "memory"
    );
    return ret;
}

// Picolibc required stubs
int _write(int file, char *ptr, int len) {
    (void)file;  // Silence unused parameter warning
    return (int)syscall(SYS_WRITE, file, (long)ptr, len);
}

void _exit(int status) {
    syscall(SYS_EXIT, status, 0, 0);
    while(1) __asm__("hlt");
}

// Heap management - uses your user memory layout (starts at 0x8000000000)
void *_sbrk(ptrdiff_t incr) {
    extern char _end;  // Defined in user_linker.ld
    static char *heap_end = &_end;
    char *prev_heap_end = heap_end;
    
    // Your user space starts at 0x8000000000, heap limit at 0x8001000000 (16MB)
    // This matches your user_linker.ld base address
    char *heap_limit = (char*)0x8001000000ULL;
    
    if (heap_end + incr > heap_limit) {
        errno = ENOMEM;
        return (void*)-1;
    }
    
    heap_end += incr;
    return (void*)prev_heap_end;
}

// Other required stubs - all with (void) casts to silence warnings
int _close(int file) {
    (void)file;
    return -1; 
}

int _fstat(int file, struct stat *st) {
    (void)file;
    st->st_mode = S_IFCHR; 
    return 0; 
}

int _isatty(int file) {
    (void)file;
    return 1; 
}

int _lseek(int file, int ptr, int dir) {
    (void)file;
    (void)ptr;
    (void)dir;
    return 0; 
}

int _read(int file, char *ptr, int len) {
    (void)file;
    return (int)syscall(SYS_READ, file, (long)ptr, len);
}

int _kill(int pid, int sig) {
    (void)pid;
    (void)sig;
    errno = EINVAL; 
    return -1; 
}

int _getpid(void) { 
    return 1; 
}

int _times(void *buf) {
    (void)buf;
    return -1; 
}
