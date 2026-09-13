[bits 64]
default rel

section .text
global context_switch

; Correct pcb_t offsets (from process.h):
;   cr3              = 0x30
;   entry_point      = 0x38
;   user_stack_top   = 0x68
;   r15 = 0x90, r14 = 0x98, r13 = 0xa0, r12 = 0xa8
;   r11 = 0xb0, r10 = 0xb8, r9  = 0xc0, r8  = 0xc8
;   rbp = 0xd0, rdi = 0xd8, rsi = 0xe0, rdx = 0xe8
;   rcx = 0xf0, rbx = 0xf8, rax = 0x100
;   rsp = 0x108, rip = 0x110
;   block_kind       = 0x150

%define KERNEL_BASE 0xFFFFFFFF80000000

; void context_switch(pcb_t* prev, pcb_t* next)
context_switch:
    push rbp
    push rbx
    push r12
    push r13
    push r14
    push r15

    mov r12, rsi            ; r12 = next

    test rdi, rdi
    jz .skip_save

    ; r15 = prev (scratch; real r15 value is reloaded from PCB slot).
    mov r15, rdi

    ; Decide which save path: ring-0 or user.
    ; Primary signal: prev->block_kind. A blocking syscall sets this
    ; before yielding, because prev->rip still holds the process's last
    ; user-mode RIP at that point and cannot distinguish "blocked in
    ; kernel" from "preempted from user mode".
    ; Secondary signal (block_kind == 0): prev->rip >= KERNEL_BASE.
    mov rax, [r15 + 0x150]              ; prev->block_kind
    test rax, rax
    jnz .ring0_save

    mov rax, [r15 + 0x110]              ; prev->rip
    cmp rax, KERNEL_BASE
    jae .ring0_save

    ; --- USER-PREEMPTED SAVE PATH ---
    push qword 0x20                     ; SS       [0x98]
    push qword [r15 + 0x108]            ; RSP      [0x90]
    push qword 0x202                    ; RFLAGS   [0x88]
    push qword 0x18                     ; CS       [0x80]
    mov rax, [r15 + 0x110]              ; prev->rip
    test rax, rax
    jnz .have_rip
    mov rax, [r15 + 0x38]               ; fall back to entry_point
.have_rip:
    push rax                            ; RIP      [0x78]

    push qword [r15 + 0x100]            ; rax      [0x70]
    push qword [r15 + 0x0F8]            ; rbx      [0x68]
    push qword [r15 + 0x0F0]            ; rcx      [0x60]
    push qword [r15 + 0x0E8]            ; rdx      [0x58]
    push qword [r15 + 0x0E0]            ; rsi      [0x50]
    push qword [r15 + 0x0D8]            ; rdi      [0x48]
    push qword [r15 + 0x0D0]            ; rbp      [0x40]
    push qword [r15 + 0x0C8]            ; r8       [0x38]
    push qword [r15 + 0x0C0]            ; r9       [0x30]
    push qword [r15 + 0x0B8]            ; r10      [0x28]
    push qword [r15 + 0x0B0]            ; r11      [0x20]
    push qword [r15 + 0x0A8]            ; r12      [0x18]
    push qword [r15 + 0x0A0]            ; r13      [0x10]
    push qword [r15 + 0x098]            ; r14      [0x08]
    push qword [r15 + 0x090]            ; r15      [0x00]

    mov [r15 + 0x108], rsp              ; prev->rsp = frame base
    mov rax, cr3
    mov [r15 + 0x30], rax
    jmp .skip_save

    ; --- RING-0 SAVE PATH ---
.ring0_save:
    mov rax, [rsp + 48]                 ; caller's return address = kernel RIP
    push rax                            ; temp0 (RIP)  @ [rsp+0]
    lea rax, [rsp + 64]                 ; caller's RSP = orig_rsp + 56
    push rax                            ; temp1 (RSP)  @ [rsp+0], temp0 @ [rsp+8]

    push qword 0x20                     ; SS       [0x98]
    push qword [rsp + 8]                ; RSP      [0x90]  <- temp1
    push qword 0x202                    ; RFLAGS   [0x88]
    push qword 0x18                     ; CS       [0x80]
    push qword [rsp + 40]               ; RIP      [0x78]  <- temp0

    push qword [r15 + 0x100]            ; rax      [0x70]
    push qword [r15 + 0x0F8]            ; rbx      [0x68]
    push qword [r15 + 0x0F0]            ; rcx      [0x60]
    push qword [r15 + 0x0E8]            ; rdx      [0x58]
    push qword [r15 + 0x0E0]            ; rsi      [0x50]
    push qword [r15 + 0x0D8]            ; rdi      [0x48]
    push qword [r15 + 0x0D0]            ; rbp      [0x40]
    push qword [r15 + 0x0C8]            ; r8       [0x38]
    push qword [r15 + 0x0C0]            ; r9       [0x30]
    push qword [r15 + 0x0B8]            ; r10      [0x28]
    push qword [r15 + 0x0B0]            ; r11      [0x20]
    push qword [r15 + 0x0A8]            ; r12      [0x18]
    push qword [r15 + 0x0A0]            ; r13      [0x10]
    push qword [r15 + 0x098]            ; r14      [0x08]
    push qword [r15 + 0x090]            ; r15      [0x00]

    mov [r15 + 0x108], rsp              ; prev->rsp = frame base
    mov rax, cr3
    mov [r15 + 0x30], rax
    ; fall through to .skip_save

.skip_save:
    test r12, r12
    jz .restore_and_return

    mov rax, [r12 + 0x30]
    mov cr3, rax
    mov rax, cr3
    mov cr3, rax
    nop
    nop
    nop
    mov cr3, rax

    mov rax, [r12 + 0x38]
    cmp rax, 0xFFFFFFFF80000000
    jae .kernel_task

    ; --- User process path ---
    mov rcx, [r12 + 0x38]
    invlpg [rcx]
    mov rcx, [r12 + 0x68]
    invlpg [rcx]

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

    push qword 0x2B
    push qword [r12 + 0x68]
    push qword 0x3202
    push qword 0x33
    push qword [r12 + 0x38]

    mov r12, [r12 + 0x0A8]
    iretq

.kernel_task:
    mov rsp, [r12 + 0x108]
    pop r15
    pop r14
    pop r13
    pop r12
    pop r11
    pop r10
    pop r9
    pop r8
    pop rbp
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop rbx
    pop rax
    iretq

.restore_and_return:
    pop r15
    pop r14
    pop r13
    pop r12
    pop rbx
    pop rbp
    ret
