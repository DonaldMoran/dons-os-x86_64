[bits 64]
default rel

section .text
global _start
extern _start_c

_start:
    ; 1. Clear frame pointer to mark a clean call stack root
    xor rbp, rbp

    ; 2. Enforce strict 16-byte stack boundary alignment for System V ABI
    mov rax, rsp
    and rsp, ~0xF
    sub rsp, 8

    ; 3. Jump directly into our C runtime initialization block via an absolute jump
    jmp _start_c

.freeze:
    hlt
    jmp .freeze
