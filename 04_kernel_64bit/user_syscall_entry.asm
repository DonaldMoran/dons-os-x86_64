[bits 64]
default rel

section .data
global user_rsp_storage
user_rsp_storage:    dq 0

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
    ; Enable SYSCALL/SYSRET in EFER
    mov ecx, 0xC0000080
    rdmsr
    or eax, 0x1
    wrmsr

    ; Set up STAR
    mov ecx, 0xC0000081
    xor edx, edx
    xor eax, eax
    mov edx, 0x00000018
    mov eax, 0x00000020
    wrmsr

    ; Set up LSTAR
    mov ecx, 0xC0000082
    mov rax, user_syscall_entry
    wrmsr

    ; Set up FMASK
    mov ecx, 0xC0000084
    xor eax, eax
    xor edx, edx
    wrmsr

    ret

; ---------------------------------------------------------------------------
; Syscall entry from usermode
; ---------------------------------------------------------------------------
user_syscall_entry:
    ; Save user RSP
    mov [user_rsp_storage], rsp
    
    push rbp
    mov rbp, rsp
    
    ; Save user RIP (RCX) and user RFLAGS (R11) for SYSRET
    push rcx
    push r11
    
    ; Save all registers
    push rbx
    push r12
    push r13
    push r14
    push r15
    
    ; Arguments on syscall:
    ; rax = syscall number
    ; rdi = arg0
    ; rsi = arg1  
    ; rdx = arg2
    ; r10 = arg3
    ; r8  = arg4
    ; r9  = arg5
    
    ; Save syscall number
    mov r14, rax
    
    ; Save arguments
    mov r12, rdi        ; arg0
    mov r13, rsi        ; arg1
    mov r15, rdx        ; arg2
    mov rbx, r10        ; arg3 (saved in rbx)
    
    ; Call syscall_dispatch(num, arg0, arg1, arg2, arg3, arg4, arg5)
    mov rdi, r14        ; num
    mov rsi, r12        ; arg0
    mov rdx, r13        ; arg1
    mov rcx, r15        ; arg2
    mov r8,  rbx        ; arg3
    mov r9,  r8         ; arg4
    ; arg5 is already in r9
    
    call syscall_dispatch
    
    ; Check for SYS_EXIT (2)
    cmp r14, 2
    je .handle_exit
    
    ; Normal syscall: restore and return via SYSRET
    pop r15
    pop r14
    pop r13
    pop r12
    pop rbx
    
    pop r11             ; restore user RFLAGS
    pop rcx             ; restore user RIP
    pop rbp
    
    ; Restore user RSP before SYSRET
    mov rsp, [user_rsp_storage]
    
    o64 sysret          ; Return to user mode

.handle_exit:
    ; SYS_EXIT - process_exit() handles cleanup and scheduling
    ; Use JMP instead of CALL to avoid stack corruption
    
    ; Restore registers
    pop r15
    pop r14
    pop r13
    pop r12
    pop rbx
    
    pop r11             ; discard saved user RFLAGS
    pop rcx             ; discard saved user RIP
    pop rbp
    
    ; Debug: print that we're exiting
    push rax            ; save return value
    mov rdi, exit_msg
    call serial_print
    pop rax
    push rax
    call serial_print_hex
    mov rdi, newline
    call serial_print
    pop rax
    
    ; Pass exit status to process_exit
    mov rdi, rax        ; status
    jmp process_exit    ; Jump to process_exit (never returns)

section .data
exit_msg: db "SYS_EXIT: status=", 0
newline: db 0x0A, 0x0D, 0
