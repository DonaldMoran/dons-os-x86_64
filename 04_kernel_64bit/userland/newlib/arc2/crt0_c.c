#include <stdint.h>
#include <stddef.h>

extern unsigned char __bss_start;
extern unsigned char __bss_end;

void _start_c(void) {
    // 1. Synchronize user space active segment selectors
    __asm__ volatile("mov $0x28, %%ax\n\tmov %%ax, %%fs\n\tmov %%ax, %%gs\n\t" : : : "rax");
    
    // 2. Clear BSS
    unsigned char *bss = &__bss_start;
    while (bss < &__bss_end) {
        *bss++ = 0;
    }

    // 3. Directly branch into the user application main block
    extern int main(int argc, char *argv[]);
    int ret = main(0, NULL);

    // 4. Clean exit trap handoff
    __asm__ volatile (
        "syscall"
        :
        : "a"(2), "D"(ret)
        : "rcx", "r11", "memory"
    );
    while(1) __asm__ volatile("hlt");
}

// 5. UNIFIED NAKED ENTRY VECTOR
void _start(void) __attribute__((naked, section(".text")));
void _start(void) {
    __asm__ volatile (
        "xor %%rbp, %%rbp\n\t"      // Clear frame pointer
        "mov %%rsp, %%rax\n\t"
        "and $-16, %%rsp\n\t"       // Force strict 16-byte alignment
        "sub $8, %%rsp\n\t"
        "jmp _start_c\n\t"          // Absolute control transfer pass
        :
        :
        : "rax", "memory"
    );
}
