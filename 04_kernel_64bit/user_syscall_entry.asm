[bits 64]
default rel

section .text
global user_syscall_entry
extern syscall_dispatch
extern kmain_shell_loop

user_syscall_entry:
    push rbp
    mov rbp, rsp

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

    ; Normal syscall: return to user via SYSRET
    pop rbp
    sysret              ; uses current RCX/R11 as user RIP/RFLAGS

.return_to_kernel:
    pop rbp
    jmp kmain_shell_loop
