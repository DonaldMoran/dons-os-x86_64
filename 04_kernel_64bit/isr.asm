[bits 64]

global isr0_stub
global isr1_stub
global isr13_stub
global irq0_stub
global irq1_stub

extern isr0_handler
extern isr1_handler
extern isr13_handler
extern irq0_handler
extern irq1_handler

irq0_stub:
    push rbp
    mov rbp, rsp
    call irq0_handler
    pop rbp
    iretq

irq1_stub:
    push rbp
    mov rbp, rsp
    call irq1_handler
    pop rbp
    iretq

; Divide-by-zero
isr0_stub:
    push rbp
    mov  rbp, rsp
    call isr0_handler
    pop  rbp
    iretq

; Debug
isr1_stub:
    push rbp
    mov  rbp, rsp
    call isr1_handler
    pop  rbp
    iretq

; General Protection Fault (#GP, vector 13)
isr13_stub:
    push rbp
    mov  rbp, rsp
    call isr13_handler
    pop  rbp
    iretq
