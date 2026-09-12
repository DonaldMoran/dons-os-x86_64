#include <sys/reent.h>
#include <stdio.h>
#include <sys/types.h>

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
    _impure_ptr = &_impure_data;

    {
        const char *s = "HELLO_FROM_REENT_INIT\n";
        register long rax __asm__("rax") = 1;
        register long rdi __asm__("rdi") = 1;
        register const char *rsi __asm__("rsi") = s;
        register long rdx __asm__("rdx") = 22;
        __asm__ volatile (
            "syscall"
            : "+r"(rax)
            : "r"(rdi), "r"(rsi), "r"(rdx)
            : "rcx", "r11", "memory"
        );
    }

    diag_write("A1\n", 3);
    diag_write("B2\n", 3);
    diag_write("C3\n", 3);

    {
        char stackbuf[16];
        const char *msg = "STACKBUF_OK\n";
        for (int i = 0; i < 12; i++) stackbuf[i] = msg[i];
        stackbuf[12] = '\0';

        register long rax __asm__("rax") = 1;
        register long rdi __asm__("rdi") = 1;
        register const char *rsi __asm__("rsi") = stackbuf;
        register long rdx __asm__("rdx") = 12;
        __asm__ volatile (
            "syscall"
            : "+r"(rax)
            : "r"(rdi), "r"(rsi), "r"(rdx)
            : "rcx", "r11", "memory"
        );
    }

    diag_write("REENT_INIT_DONE\n", 16);

    /* Unconditionally initialize stdio. _impure_data is a fresh zeroed
       struct, so __cleanup is NULL and __sinit's work is needed. We do
       NOT check _impure_ptr->__cleanup here, because if _impure_ptr's
       static initializer was broken by an unapplied relocation, that
       dereference would fault. */
    __sinit(_impure_ptr);

    diag_write("SINIT_DONE\n", 11);
}
