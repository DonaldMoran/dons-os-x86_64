#include <sys/reent.h>
#include <stdio.h>
#include <sys/types.h>

extern struct _reent *_impure_ptr;
extern struct _reent _impure_data;
extern void __sinit(struct _reent *);

/* Raw syscall write helper (kept for reference, but not used below). */
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

    /* ============================================================
     * ISOLATION TEST 1: a single, fixed, 22-byte write.
     * We expect exactly:
     *     HELLO_FROM_REENT_INIT\n
     * (22 characters: H E L L O _ F R O M _ R E E N T _ I N I T \n)
     * ============================================================ */
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

    /* ============================================================
     * ISOLATION TEST 2: three short writes in a row.
     * We expect:
     *     A1
     *     B2
     *     C3
     * ============================================================ */
    diag_write("A1\n", 3);
    diag_write("B2\n", 3);
    diag_write("C3\n", 3);

    /* ============================================================
     * ISOLATION TEST 3: a write whose buffer is on the stack.
     * This exercises the stack-pointer-correctness of the syscall
     * entry path. We expect: STACKBUF_OK\n
     * ============================================================ */
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

    /* Final line so we know the function completed. */
    diag_write("REENT_INIT_DONE\n", 16);

    if (_impure_ptr->__cleanup == NULL) {
        __sinit(_impure_ptr);
    }
}
