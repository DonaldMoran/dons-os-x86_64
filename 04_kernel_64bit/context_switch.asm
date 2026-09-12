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

    ; --- Build a timer-compatible frame for prev on the current stack ---
    ; iretq portion (SS, RSP, RFLAGS, CS, RIP)
    push qword 0x20                     ; SS
    push qword [r15 + 0x108]            ; RSP (prev's last saved kernel RSP; may be 0 for first save)
    push qword 0x202                    ; RFLAGS
    push qword 0x18                     ; CS
    mov rax, [r15 + 0x110]              ; prev->rip
    test rax, rax
    jnz .have_rip
    mov rax, [r15 + 0x38]               ; fall back to entry_point
.have_rip:
    push rax                            ; RIP

    ; 15 GPRs in irq0_stub order
    push qword [r15 + 0x090]
    push qword [r15 + 0x098]
    push qword [r15 + 0x0A0]
    push qword [r15 + 0x0A8]
    push qword [r15 + 0x0B0]
    push qword [r15 + 0x0B8]
    push qword [r15 + 0x0C0]
    push qword [r15 + 0x0C8]
    push qword [r15 + 0x0D0]
    push qword [r15 + 0x0D8]
    push qword [r15 + 0x0E0]
    push qword [r15 + 0x0E8]
    push qword [r15 + 0x0F0]
    push qword [r15 + 0x0F8]
    push qword [r15 + 0x100]

    mov [r15 + 0x108], rsp              ; prev->rsp = frame base

    mov rax, cr3
    mov [r15 + 0x30], rax

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
    ; --- Kernel task: resume the frame that was saved (or pre-built). ---
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
