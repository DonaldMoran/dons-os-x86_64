#include <sys/reent.h>

extern struct _reent *_impure_ptr;
extern struct _reent _impure_data;
extern void __sinit(struct _reent *);

static void diag_write(const char *s, unsigned long len) {
    register long rax __asm__("rax") = 1;
    register long rdi __asm__("rdi") = 1;
    register const char *rsi __asm__("rsi") = s;
    register unsigned long rdx __asm__("rdx") = len;
    __asm__ volatile (
        "syscall"
        : "+r"(rax)
        : "r"(rdi), "r"(rsi), "r"(rdx)
        : "rcx", "r11", "memory"
    );
}

void donsdos_newlib_init(void) {
    //~ diag_write("A", 1);
    //~ diag_write("B", 1);
    //~ diag_write("C", 1);
    _impure_ptr = &_impure_data;
    //~ diag_write("D", 1);
    __sinit(_impure_ptr);
    //~ diag_write("E", 1);
}
