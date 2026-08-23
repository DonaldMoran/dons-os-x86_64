[bits 64]
default rel

section .text
global jump_to_user_mode

; void jump_to_user_mode(uint64_t entry, uint64_t stack)
; entry in RDI, stack in RSI
jump_to_user_mode:
    ; Build IRETQ frame
    push qword 0x2B        ; SS = user data (0x28)
    push rsi               ; RSP = user stack
    pushfq
    pop rax
    or rax, 0x0200         ; RFLAGS with IF=1 (bit 9), IOPL=0 (bits 13:12 = 0)
    ; Clear IOPL bits (bits 13:12) to avoid GP fault
    and rax, ~0x3000       ; Clear IOPL (bits 13:12)
    or rax, 0x0200         ; Set IF (bit 9)
    push rax
    push qword 0x33        ; CS = user code (0x30)
    push rdi               ; RIP = entry
    
    iretq
