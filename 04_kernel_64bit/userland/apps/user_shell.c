// user_shell.c - Use const string (goes in .rodata)
#include <stdio.h>

// const string goes in .rodata, not .data
static const char msg[] = "Hello from Picolibc!\n";

int main(void) {
    long ret;
    __asm__ volatile (
        "syscall"
        : "=a"(ret)
        : "a"(1),      // SYS_WRITE
          "D"(1),      // fd = stdout
          "S"(msg),    // buffer
          "d"(22)      // length
        : "rcx", "r11", "memory"
    );
    
    while(1) {
        __asm__ volatile("pause");
    }
    return 0;
}
