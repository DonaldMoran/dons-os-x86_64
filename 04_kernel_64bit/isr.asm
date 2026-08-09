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
extern isr13_handler
extern isr14_handler

extern irq0_handler
extern irq1_handler

; IRQ0 - PIT Timer
irq0_stub:
    push rbp
    mov rbp, rsp
    call irq0_handler
    pop rbp
    iretq

; IRQ1 - Keyboard
irq1_stub:
    push rbp
    mov rbp, rsp
    call irq1_handler
    pop rbp
    iretq

; Divide-by-zero (NO error code) - FIXED: push dummy error code
isr0_stub:
    push 0              ; Push dummy error code to keep stack consistent
    push rbp
    mov  rbp, rsp
    call isr0_handler
    pop  rbp
    add  rsp, 8         ; Remove dummy error code
    iretq

; Debug (NO error code) - FIXED: push dummy error code
isr1_stub:
    push 0              ; Push dummy error code to keep stack consistent
    push rbp
    mov  rbp, rsp
    call isr1_handler
    pop  rbp
    add  rsp, 8         ; Remove dummy error code
    iretq

; Double Fault (HAS error code) - CPU pushes error code
isr8_stub:
    push rbp
    mov  rbp, rsp
    mov  rdi, rsp        ; rdi = pointer to exception frame (includes error code)
    call isr8_handler
    pop  rbp
    iretq

; General Protection Fault (HAS error code)
isr13_stub:
    push rbp
    mov  rbp, rsp
    mov  rdi, rsp        ; rdi = pointer to exception frame
    call isr13_handler
    pop  rbp
    iretq

; Page Fault (HAS error code) - CPU pushes error code
isr14_stub:
    push rbp
    mov  rbp, rsp
    mov  rdi, rsp        ; rdi = pointer to exception frame (includes error code)
    call isr14_handler
    pop  rbp
    iretq
