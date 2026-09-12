[bits 64]
default rel

section .text
global user_syscall_entry
global syscall_init_asm

extern syscall_dispatch
extern process_exit
extern serial_print
extern serial_print_hex

; ---------------------------------------------------------------------------
; Syscall init: set up EFER, STAR, LSTAR, FMASK
; ---------------------------------------------------------------------------
syscall_init_asm:
    mov ecx, 0xC0000080
    rdmsr
    or eax, 0x1
    wrmsr

    mov ecx, 0xC0000081
    xor edx, edx
    xor eax, eax
    mov edx, 0x00180000
    mov eax, 0x00200000
    wrmsr

    mov ecx, 0xC0000082
    mov rax, user_syscall_entry
    wrmsr

    mov ecx, 0xC0000084
    mov eax, 0x00000200
    xor edx, edx
    wrmsr

    ret

; ---------------------------------------------------------------------------
; Syscall entry from usermode
; ---------------------------------------------------------------------------
user_syscall_entry:
    ; Save original callee-saved registers FIRST.
    push rbx
    push rbp
    push r12
    push r13
    push r14
    push r15

    ; Save the syscall return RIP / RFLAGS that the CPU put in RCX / R11.
    push rcx                        ; user RIP
    push r11                        ; user RFLAGS

    ; Save user RSP (syscall does not save RSP).
    push rsp

    ; Save r8/r9 so we can freely use them during arg shuffling.
    push r8
    push r9

    ; Arg5 -> 7th C argument, on stack.
    push r9
    mov rbx, rax                    ; save syscall number
    mov r9, r8                      ; arg4 -> r9
    mov r8, r10                     ; arg3 -> r8
    mov rcx, rdx                    ; arg2 -> rcx
    mov rdx, rsi                    ; arg1 -> rdx
    mov rsi, rdi                    ; arg0 -> rsi
    mov rdi, rax                    ; num  -> rdi

    call syscall_dispatch
    add rsp, 8                      ; discard stacked arg5

    cmp rbx, 2
    je .handle_exit

    pop r9
    pop r8
    add rsp, 8                      ; discard user RSP slot
    pop r11                         ; user RFLAGS
    pop rcx                         ; user RIP

    pop r15
    pop r14
    pop r13
    pop r12
    pop rbp
    pop rbx

    o64 sysret

.handle_exit:
    pop r9
    pop r8
    add rsp, 8                      ; discard user RSP slot
    add rsp, 16                     ; discard r11, rcx
    pop r15
    pop r14
    pop r13
    pop r12
    pop rbp
    pop rbx

    push rax
    mov rdi, exit_msg
    call serial_print
    pop rax
    push rax
    call serial_print_hex
    mov rdi, newline
    call serial_print
    pop rax

    mov rdi, rax
    jmp process_exit

section .data
exit_msg: db "SYS_EXIT: status=", 0
newline: db 0x0A, 0x0D, 0
