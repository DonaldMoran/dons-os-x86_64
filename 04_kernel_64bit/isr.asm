[bits 64]

global isr0_stub
global isr1_stub
global isr8_stub
global isr13_stub
global isr14_stub

global irq0_stub
global irq1_stub

extern isr0_handler
extern isr1_handler
extern isr8_handler
extern isr14_handler

extern irq0_handler
extern irq1_handler
extern isr13_handler


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

isr0_stub:
    push 0
    push rbp
    mov  rbp, rsp
    call isr0_handler
    pop  rbp
    add  rsp, 8
    iretq

isr1_stub:
    push 0
    push rbp
    mov  rbp, rsp
    call isr1_handler
    pop  rbp
    add  rsp, 8
    iretq

isr8_stub:
    push rbp
    mov  rbp, rsp
    mov  rdi, rsp
    call isr8_handler
    pop  rbp
    iretq

; GP Fault - call C handler
isr13_stub:
    cli
    push rbp
    mov  rbp, rsp
    lea  rdi, [rsp + 8]  ; Skip rbp, point to error code
    call isr13_handler
    pop  rbp
    add  rsp, 8          ; Remove error code
    iretq

; Page Fault - call C handler
isr14_stub:
    cli
    push rbp
    mov  rbp, rsp
    lea  rdi, [rsp + 8]  ; Skip rbp, point to error code
    call isr14_handler
    pop  rbp
    add  rsp, 8          ; Remove error code
    iretq
