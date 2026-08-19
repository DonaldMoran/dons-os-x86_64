[bits 64]
default rel

section .text
global context_switch

; void context_switch(pcb_t* prev, pcb_t* next)
; prev in RDI, next in RSI
context_switch:
    ; ============================================================
    ; Save current process state (prev in RDI)
    ; ============================================================
    
    ; Save general purpose registers (offsets from 0x70)
    mov [rdi + 0x70], rax    ; RAX
    mov [rdi + 0x78], rbx    ; RBX
    mov [rdi + 0x80], rcx    ; RCX
    mov [rdi + 0x88], rdx    ; RDX
    mov [rdi + 0x90], rsi    ; RSI
    mov [rdi + 0x98], rdi    ; RDI (original)
    mov [rdi + 0xA0], rbp    ; RBP
    mov [rdi + 0xA8], r8     ; R8
    mov [rdi + 0xB0], r9     ; R9
    mov [rdi + 0xB8], r10    ; R10
    mov [rdi + 0xC0], r11    ; R11
    mov [rdi + 0xC8], r12    ; R12
    mov [rdi + 0xD0], r13    ; R13
    mov [rdi + 0xD8], r14    ; R14
    mov [rdi + 0xE0], r15    ; R15
    
    ; Save RSP and RIP
    mov [rdi + 0xE8], rsp    ; RSP
    mov rax, [rsp]           ; Get return address
    mov [rdi + 0xF0], rax    ; RIP
    
    ; ============================================================
    ; Switch to next process (next in RSI)
    ; ============================================================
    
    ; Restore RSP and RIP first
    mov rax, [rsi + 0xF0]    ; Load next RIP
    mov rsp, [rsi + 0xE8]    ; Load next RSP
    
    ; Push return address onto new stack
    push rax
    
    ; Restore all registers from next's PCB
    mov rax, [rsi + 0x70]
    mov rbx, [rsi + 0x78]
    mov rcx, [rsi + 0x80]
    mov rdx, [rsi + 0x88]
    mov rbp, [rsi + 0xA0]
    mov r8,  [rsi + 0xA8]
    mov r9,  [rsi + 0xB0]
    mov r10, [rsi + 0xB8]
    mov r11, [rsi + 0xC0]
    mov r12, [rsi + 0xC8]
    mov r13, [rsi + 0xD0]
    mov r14, [rsi + 0xD8]
    mov r15, [rsi + 0xE0]
    mov rdi, [rsi + 0x98]    ; Load RDI
    mov rsi, [rsi + 0x90]    ; Load RSI (last)
    
    ret
