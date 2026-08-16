; syscall_init.asm - Assembly initialization for syscalls
section .text
global syscall_init_asm
global syscall_handler_entry

extern syscall_dispatcher

; Dedicated stack for syscalls (in .bss)
section .bss
align 16
syscall_stack_bottom:
    resb 4096
syscall_stack_top:

section .text

; SYSCALL handler entry point
syscall_handler_entry:
    ; DEBUG: Serial marker 'S' = entered syscall handler
    mov al, 'S'
    mov dx, 0x3F8
    out dx, al
    
    ; Save user RSP
    mov r15, rsp
    
    ; Switch to dedicated syscall stack
    lea rsp, [syscall_stack_top]
    sub rsp, 16
    
    ; Save user RSP
    push r15
    
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
    
    ; Call dispatcher
    mov rcx, r10        ; 4th arg
    mov rdi, rax        ; syscall number
    call syscall_dispatcher
    
    ; Store return value
    mov [rbp + 8], rax
    
    ; Restore all registers
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
    pop rax
    pop rbp
    
    ; Restore user RSP
    pop rsp
    
    ; DEBUG: Serial marker 'R' = returning to user mode
    mov al, 'R'
    mov dx, 0x3F8
    out dx, al
    
    ; Return to user mode
    o64 sysret

; Initialize MSRs for syscall
syscall_init_asm:
    ; IA32_STAR (0xC0000081)
    mov ecx, 0xC0000081
    mov eax, 0x002B0008
    mov edx, 0x00000000
    wrmsr
    
    ; IA32_LSTAR (0xC0000082)
    mov ecx, 0xC0000082
    mov rax, syscall_handler_entry
    wrmsr
    
    ; IA32_FMASK (0xC0000084)
    mov ecx, 0xC0000084
    mov eax, 0x00000000
    mov edx, 0x00000000
    wrmsr
    
    ret