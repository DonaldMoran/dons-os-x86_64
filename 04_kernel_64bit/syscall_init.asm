; syscall_init.asm - Assembly initialization for syscalls
section .text
global syscall_init_asm
global syscall_handler_entry

extern syscall_dispatcher

; SYSCALL handler entry point
syscall_handler_entry:
    ; Save all registers
    push rbp
    mov rbp, rsp
    push rax
    push rbx
    push rcx
    push rdx
    push rsi
    push rdi
    push r8
    push r9
    push r10
    push r11
    push r12
    push r13
    push r14
    push r15
    
    ; Call dispatcher with arguments in correct order
    ; At this point: rax=syscall#, rdi=arg1, rsi=arg2, rdx=arg3, r10=arg4, r8=arg5, r9=arg6
    ; For C: rdi=arg1, rsi=arg2, rdx=arg3, rcx=arg4, r8=arg5, r9=arg6
    ; So we need to move r10 to rcx, and rax to rdi
    mov rcx, r10        ; 4th arg
    mov rdi, rax        ; syscall number goes to rdi (1st arg)
    ; rsi, rdx, r8, r9 already in correct positions
    
    call syscall_dispatcher
    
    ; Store return value in saved rax position
    mov [rbp + 8], rax
    
    ; Restore all registers
    pop r15
    pop r14
    pop r13
    pop r12
    pop r11
    pop r10
    pop r9
    pop r8
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop rbx
    pop rax              ; This gets the return value
    pop rbp
    
    ; Return to user mode
    o64 sysret

; Initialize MSRs for syscall
syscall_init_asm:
    ; IA32_STAR (0xC0000081)
    mov ecx, 0xC0000081
    ; Lower 32 bits: SYSCALL CS (kernel) = 0x08, SYSRET CS (user) = 0x1B
    ; Upper 32 bits: reserved
    mov eax, 0x001B0008
    mov edx, 0x00000000
    wrmsr
    
    ; IA32_LSTAR (0xC0000082) - Entry point
    mov ecx, 0xC0000082
    mov rax, syscall_handler_entry
    wrmsr
    
    ; IA32_FMASK (0xC0000084) - Clear DF, etc.
    mov ecx, 0xC0000084
    mov eax, 0x00000000
    mov edx, 0x00000000
    wrmsr
    
    ret
