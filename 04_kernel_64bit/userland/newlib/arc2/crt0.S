.intel_syntax noprefix
.global _start
.extern main
.extern exit
.extern __bss_start
.extern __bss_end
.extern __libc_init_array
.extern donsdos_newlib_init
.extern __sinit
.extern _impure_ptr
.extern _impure_data

.section .data
.align 8
/*
 * Saved argc/argv from the kernel's resume frame.  These live in
 * .data, which is NOT cleared by the BSS-clear loop below, so we can
 * write them at the very top of _start and read them just before
 * calling main.  (Registers rdi/rsi are clobbered by the BSS clear
 * and by the libc init calls.)
 *
 * The kernel places argc in rdi and argv in rsi when the process
 * starts (see sys_exec in user_syscall.c).  Programs spawned with
 * no arguments get argc=0, argv=NULL.
 */
argc_saved:
    .quad 0
argv_saved:
    .quad 0

.section .text
_start:
    /*
     * Stash the kernel-provided argc/argv immediately.  Nothing else
     * in _start may touch rdi/rsi before these two stores.
     */
    mov [rip + argc_saved], rdi
    mov [rip + argv_saved], rsi

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
    mov rbx, rsp
    and rsp, -16
    push rbx
    sub rsp, 8

    // ---- hard-set _impure_ptr before anything else touches newlib ----
    movabsq rax, offset _impure_data
    movabsq rbx, offset _impure_ptr
    mov [rbx], rax
    // ------------------------------------------------------------------

    // Initialize Newlib's global static constructors array
    call __libc_init_array

    // Initialize Newlib reentrancy
    call donsdos_newlib_init

    // Restore argc/argv and pass them to main.
    mov rdi, [rip + argc_saved]
    mov rsi, [rip + argv_saved]
    xor rdx, rdx            // envp = NULL
    call main

    mov rdi, rax
    call exit

.freeze:
    hlt
    jmp .freeze
