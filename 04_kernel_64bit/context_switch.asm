[bits 64]
default rel

section .text
global context_switch

; Correct pcb_t offsets (from process.h):
;   cr3              = 0x30
;   entry_point      = 0x38
;   user_stack_top   = 0x68
;   r15              = 0x90
;   r14              = 0x98
;   r13              = 0xa0
;   r12              = 0xa8
;   r11              = 0xb0
;   r10              = 0xb8
;   r9               = 0xc0
;   r8               = 0xc8
;   rbp              = 0xd0
;   rdi              = 0xd8
;   rsi              = 0xe0
;   rdx              = 0xe8
;   rcx              = 0xf0
;   rbx              = 0xf8
;   rax              = 0x100
;   rsp              = 0x108
;   rip              = 0x110

; void context_switch(pcb_t* prev, pcb_t* next)
; prev in RDI, next in RSI
context_switch:
    push rbp
    push rbx
    push r12
    push r13
    push r14
    push r15

    mov r12, rsi            ; r12 = next

    ; Save previous process's registers (if prev != NULL)
    test rdi, rdi
    jz .skip_save

    mov [rdi + 0x100], rax
    mov [rdi + 0x0F8], rbx
    mov [rdi + 0x0F0], rcx
    mov [rdi + 0x0E8], rdx
    mov [rdi + 0x0E0], rsi
    mov [rdi + 0x0D8], rdi
    mov [rdi + 0x0D0], rbp
    mov [rdi + 0x0C8], r8
    mov [rdi + 0x0C0], r9
    mov [rdi + 0x0B8], r10
    mov [rdi + 0x0B0], r11
    mov [rdi + 0x0A8], r12
    mov [rdi + 0x0A0], r13
    mov [rdi + 0x098], r14
    mov [rdi + 0x090], r15

    mov [rdi + 0x108], rsp   ; save current RSP
    mov rax, [rsp]
    mov [rdi + 0x110], rax   ; save current RIP (return address)

    ; Save current CR3 into prev->cr3
    mov rax, cr3
    mov [rdi + 0x30], rax

.skip_save:
    test r12, r12
    jz .restore_and_return

    ; Load the next process's CR3
    mov rax, [r12 + 0x30]   ; pcb->cr3
    mov cr3, rax

    ; Flush TLB (double reload)
    mov rax, cr3
    mov cr3, rax
    nop
    nop
    nop
    mov cr3, rax

    ; Determine if it's a user process (entry_point < KERNEL_BASE)
    mov rax, [r12 + 0x38]   ; entry_point
    cmp rax, 0xFFFFFFFF80000000
    jae .kernel_task        ; if >= KERNEL_BASE → kernel task

    ; ---- User process: use iretq with user selectors ----
    ; Invalidate user addresses
    mov rcx, [r12 + 0x38]   ; entry point
    invlpg [rcx]
    mov rcx, [r12 + 0x68]   ; user_stack_top
    invlpg [rcx]

    ; Restore general registers (except RSP, which will be set by iret)
    mov rax, [r12 + 0x100]
    mov rbx, [r12 + 0x0F8]
    mov rcx, [r12 + 0x0F0]
    mov rdx, [r12 + 0x0E8]
    mov rbp, [r12 + 0x0D0]
    mov r8,  [r12 + 0x0C8]
    mov r9,  [r12 + 0x0C0]
    mov r10, [r12 + 0x0B8]
    mov r11, [r12 + 0x0B0]
    mov r13, [r12 + 0x0A0]
    mov r14, [r12 + 0x098]
    mov r15, [r12 + 0x090]
    mov rdi, [r12 + 0x0D8]
    mov rsi, [r12 + 0x0E0]

    ; Build iretq frame for ring 3
    push qword 0x2B         ; SS
    push qword [r12 + 0x68] ; RSP (user stack top)
    push qword 0x3202       ; RFLAGS
    push qword 0x33         ; CS
    push qword [r12 + 0x38] ; RIP (entry point)

    ; Clear r12 last
    mov r12, [r12 + 0x0A8]

    iretq

.kernel_task:
    ; ---- Kernel task (idle, etc.): just switch stack and return ----
    mov rax, [r12 + 0x100]
    mov rbx, [r12 + 0x0F8]
    mov rcx, [r12 + 0x0F0]
    mov rdx, [r12 + 0x0E8]
    mov rbp, [r12 + 0x0D0]
    mov r8,  [r12 + 0x0C8]
    mov r9,  [r12 + 0x0C0]
    mov r10, [r12 + 0x0B8]
    mov r11, [r12 + 0x0B0]
    mov r13, [r12 + 0x0A0]
    mov r14, [r12 + 0x098]
    mov r15, [r12 + 0x090]
    mov rdi, [r12 + 0x0D8]
    mov rsi, [r12 + 0x0E0]

    mov rsp, [r12 + 0x108]
    mov rax, [r12 + 0x110]
    jmp rax

.restore_and_return:
    pop r15
    pop r14
    pop r13
    pop r12
    pop rbx
    pop rbp
    ret
