[bits 64]
default rel

section .bss
global user_rsp_storage
user_rsp_storage:    resq 1

section .text
global user_syscall_entry
global syscall_init_asm

extern syscall_dispatch
extern kmain_shell_loop
extern kernel_stack_top

; ---------------------------------------------------------------------------
; Syscall init: set up EFER, STAR, LSTAR, FMASK
; ---------------------------------------------------------------------------
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
    mov rax, user_syscall_entry  ; syscall entry point
    wrmsr

    mov ecx, 0xC0000084          ; IA32_FMASK
    xor eax, eax
    xor edx, edx
    wrmsr

    ret

; ---------------------------------------------------------------------------
; Syscall entry from usermode
; ---------------------------------------------------------------------------
user_syscall_entry:
    push rbp
    mov rbp, rsp

    ; Save user RIP (RCX) and user RFLAGS (R11) for SYSRET
    push rcx
    push r11

    ; rax = syscall number
    mov r14, rax        ; save syscall number

    ; Save original args (avoid r11, it's special for sysret)
    mov r12, rdi        ; arg0
    mov r13, rsi        ; arg1
    mov r15, rdx        ; arg2

    ; System V ABI: syscall_dispatch(num, arg0, arg1, arg2, arg3, arg4, arg5)
    mov rdi, r14        ; num
    mov rsi, r12        ; arg0
    mov rdx, r13        ; arg1
    mov rcx, r15        ; arg2
    mov r8,  r10        ; arg3
    ; r9 already holds arg5

    call syscall_dispatch

    ; Check for SYS_EXIT (2)
    cmp r14, 2
    je .return_to_kernel

    ; Normal syscall: restore user RIP/RFLAGS and return via SYSRET
    pop r11             ; restore user RFLAGS
    pop rcx             ; restore user RIP
    pop rbp
    o64 sysret          ; 64-bit SYSRET using RCX/R11

.return_to_kernel:
    pop r11             ; discard saved user RFLAGS
    pop rcx             ; discard saved user RIP
    pop rbp
    
    ; switch to kernel stack (same one TSS uses)
    mov rsp, [kernel_stack_top]
     
    jmp kmain_shell_loop
