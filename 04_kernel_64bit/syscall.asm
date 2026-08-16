section .bss
global user_rsp_storage
user_rsp_storage:    resq 1

section .text
global syscall_init_asm
global syscall_handler
extern syscall_dispatcher

syscall_init_asm:
    mov ecx, 0xC0000080          ; IA32_EFER
    rdmsr
    or eax, 0x1                  ; SCE
    wrmsr

    mov ecx, 0xC0000081          ; IA32_STAR
    mov eax, 0
    mov edx, 0x002B0018          ; user CS=0x2B, kernel CS=0x18
    wrmsr

    mov ecx, 0xC0000082          ; IA32_LSTAR
    mov rax, syscall_handler
    wrmsr

    mov ecx, 0xC0000084          ; IA32_FMASK
    xor eax, eax
    xor edx, edx
    wrmsr

    ret

syscall_handler:
    ; save user RIP/RFLAGS
    mov r12, rcx
    mov r13, r11

    ; save volatile regs (not rcx/r11)
    push rax
    push rbx
    push rdx
    push rsi
    push rdi
    push r8
    push r9
    push r10
    push r14
    push r15

    ; syscall ABI → C ABI
    mov rcx, r10

    call syscall_dispatcher

    ; restore regs (rax = return value)
    pop r15
    pop r14
    pop r10
    pop r9
    pop r8
    pop rdi
    pop rsi
    pop rdx
    pop rbx
    pop rax

    ; restore user RIP/RFLAGS
    mov rcx, r12
    mov r11, r13

    ; DO NOT touch RSP here
    o64 sysret
