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

    ;  /* 1. Synchronize segments to user mode data selector */
    mov ax, 0x2B         
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    ; /* 2. PATCHED: Enforce strict RCX length register targeting for rep stosb.
       ; This accurately clears Newlib's tracking common segment structures! */
    movabsq rdi, offset __bss_start
    movabsq rsi, offset __bss_end
    mov rcx, rsi
    sub rcx, rdi         /* RCX = exact total byte length to clear */
    xor rax, rax         /* Write 0 */
    rep stosb            /* Zero out the BSS segment byte-by-byte */

    ;  /* 3. Ensure strict System V AMD64 ABI 16-byte stack alignment */
    mov rbx, rsp
    and rsp, -16
    push rbx             

    ; /* 4. Initialize Newlib's global static constructors array */
    call __libc_init_array

    ; /* 4b. Initialize Newlib reentrancy (sets up _impure_ptr and stdout/stderr) */
    call donsdos_newlib_init

    sub rsp, 8

    ; /* 5. Set up explicit parameters for main (argc, argv, envp)
       ;~ We pass a clean NULL environment to satisfy standard IO pointer lookups */
    xor rdi, rdi         /* argc = 0 */
    xor rsi, rsi         /* argv = NULL */
    xor rdx, rdx         /* envp = NULL */
    call main

    add rsp, 8
    mov rdi, rax
    call exit

.freeze:
    hlt
    jmp .freeze
