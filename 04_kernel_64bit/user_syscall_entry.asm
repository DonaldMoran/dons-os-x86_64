[bits 64]
default rel

section .text
global user_syscall_entry
extern syscall_dispatch
extern kmain_shell_loop

user_syscall_entry:
    push rbp
    mov rbp, rsp

    ; Save SYSCALL return state
    push rcx            ; user RIP
    push r11            ; user RFLAGS

    mov r14, rax        ; syscall number

    ; Save original args
    mov r11, rdi        ; fd
    mov r12, rsi        ; buf
    mov r13, rdx        ; count

    ; System V ABI for syscall_dispatch
    mov rdi, r14        ; num
    mov rsi, r11        ; arg0
    mov rdx, r12        ; arg1
    mov rcx, r13        ; arg2
    mov r8,  r10        ; arg3
    ; r9 already holds arg5

    call syscall_dispatch

    ; Check for SYS_EXIT (2)
    cmp r14, 2
    je .return_to_kernel
    
    ; TEMP: don’t sysret, just treat all syscalls as exit
    jmp kmain_shell_loop

    ; Normal syscall: return to user via SYSRETQ
    pop r11             ; restore user RFLAGS
    pop rcx             ; restore user RIP
    pop rbp
    sysret              ; NASM does not support sysretq, that is intel syntax

.return_to_kernel:
    pop r11             ; discard saved RFLAGS
    pop rcx             ; discard saved RIP
    pop rbp
    jmp kmain_shell_loop
