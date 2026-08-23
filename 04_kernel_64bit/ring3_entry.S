[bits 64]
default rel

section .text
global jump_to_user_mode

; void jump_to_user_mode(uint64_t entry, uint64_t stack)
; entry in RDI, stack in RSI
jump_to_user_mode:
    ; Build IRETQ frame
    push qword 0x28        ; SS = user data (0x28)
    push rsi               ; RSP = user stack
    pushfq
    pop rax
    or rax, 0x3200         ; RFLAGS with IF=1, IOPL=3
    push rax
    push qword 0x30        ; CS = user code (0x30)
    push rdi               ; RIP = entry
    
    iretq
