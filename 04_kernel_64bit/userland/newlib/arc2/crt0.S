.intel_syntax noprefix
.global _start
.extern main
.extern exit
.extern __bss_start
.extern __bss_end
.extern __libc_init_array
.extern donsdos_newlib_init

.section .text
_start:
    xor rbp, rbp

    // Synchronize segments to user mode data selector
    mov ax, 0x2B
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    // Clear the BSS segment
    movabsq rdi, offset __bss_start
    movabsq rsi, offset __bss_end
    mov rcx, rsi
    sub rcx, rdi
    xor rax, rax
    rep stosb

    // Ensure strict System V AMD64 ABI 16-byte stack alignment.
    // Before each call instruction, RSP must be 16-byte aligned so that
    // the callee sees RSP = 8 (mod 16) at entry. SSE-vectorized code in
    // libc.a relies on this for aligned movaps stores.
    mov rbx, rsp
    and rsp, -16         // RSP = 0 (mod 16)
    push rbx             // RSP = 8 (mod 16)
    sub rsp, 8           // RSP = 0 (mod 16), aligned for calls below

    // Initialize Newlib's global static constructors array
    call __libc_init_array

    // Initialize Newlib reentrancy (sets up _impure_ptr and stdout/stderr)
    call donsdos_newlib_init

    // Set up explicit parameters for main (argc, argv, envp)
    xor rdi, rdi         // argc = 0
    xor rsi, rsi         // argv = NULL
    xor rdx, rdx         // envp = NULL
    call main

    mov rdi, rax
    call exit

.freeze:
    hlt
    jmp .freeze
