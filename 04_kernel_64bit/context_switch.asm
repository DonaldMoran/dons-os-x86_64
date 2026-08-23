[bits 64]
default rel

%define HHDM_START 0xFFFF800000000000

section .text
global context_switch

; void context_switch(pcb_t* prev, pcb_t* next)
; prev in RDI, next in RSI
context_switch:
    push rbp
    push rbx
    push r12
    push r13
    push r14
    push r15
    
    mov r12, rsi
    
    test rdi, rdi
    jz .skip_save
    
    mov [rdi + 0x70], rax
    mov [rdi + 0x78], rbx
    mov [rdi + 0x80], rcx
    mov [rdi + 0x88], rdx
    mov [rdi + 0x90], rsi
    mov [rdi + 0x98], rdi
    mov [rdi + 0xA0], rbp
    mov [rdi + 0xA8], r8
    mov [rdi + 0xB0], r9
    mov [rdi + 0xB8], r10
    mov [rdi + 0xC0], r11
    mov [rdi + 0xC8], r12
    mov [rdi + 0xD0], r13
    mov [rdi + 0xD8], r14
    mov [rdi + 0xE0], r15
    
    mov [rdi + 0xE8], rsp
    mov rax, [rsp]
    mov [rdi + 0xF0], rax

.skip_save:
    test r12, r12
    jz .restore_and_return
    
    ; Check if this is a user process
    ; entry_point != 0 AND entry_point is in user space (bit 63 = 0)
    cmp qword [r12 + 0x38], 0
    je .kernel_process      ; entry_point == 0 -> kernel process (idle)
    
    ; Check if entry_point is in user space (bit 63 = 0)
    mov rax, [r12 + 0x38]
    test rax, rax
    js .kernel_process      ; Bit 63 set -> kernel address
    
    ; User process - enter user mode
    jmp .switch_to_user

.kernel_process:
    jmp .restore_and_return

.switch_to_user:
    mov [r12 + 0x30], rsp
    
    push rax
    push rbx
    push rcx
    push rdx
    
    mov rax, [r12 + 0x30]
    mov rbx, HHDM_START + 0x5000 + 0x04
    mov [rbx], rax
    
    pop rdx
    pop rcx
    pop rbx
    pop rax
    
    mov rax, [r12 + 0x70]
    mov rbx, [r12 + 0x78]
    mov rcx, [r12 + 0x80]
    mov rdx, [r12 + 0x88]
    mov rbp, [r12 + 0xA0]
    mov r8,  [r12 + 0xA8]
    mov r9,  [r12 + 0xB0]
    mov r10, [r12 + 0xB8]
    mov r11, [r12 + 0xC0]
    mov r13, [r12 + 0xD0]
    mov r14, [r12 + 0xD8]
    mov r15, [r12 + 0xE0]
    mov rdi, [r12 + 0x98]
    mov rsi, [r12 + 0x90]
    
    push qword 0x2B        ; SS (User Data)
    push qword [r12 + 0x68] ; RSP (User stack top)
    pushfq
    pop rax
    or rax, 0x3200         ; RFLAGS (IF=1, IOPL=3)
    push rax
    push qword 0x33        ; CS (User Code)
    push qword [r12 + 0x38] ; RIP (Entry point)
    
    iretq

.restore_and_return:
    pop r15
    pop r14
    pop r13
    pop r12
    pop rbx
    pop rbp
    ret
