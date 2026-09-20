[bits 64]
default rel

section .text
global context_switch

; Correct pcb_t offsets (from process.h). These MUST match the C struct
; layout exactly. Any change to pcb_t that shifts a field used here must
; be reflected below — the assembly has no way to know on its own. The
; _Static_assert block at the bottom of process.c guards these offsets
; at compile time; if the struct layout ever changes, the build fails
; with a message pointing at this file.
;
; Current layout:
;   cr3              = 0x30
;   entry_point      = 0x38
;   user_stack_top   = 0x70
;   r15 = 0x98, r14 = 0xA0, r13 = 0xA8, r12 = 0xB0
;   r11 = 0xB8, r10 = 0xC0, r9  = 0xC8, r8  = 0xD0
;   rbp = 0xD8, rdi = 0xE0, rsi = 0xE8, rdx = 0xF0
;   rcx = 0xF8, rbx = 0x100, rax = 0x108
;   rsp = 0x110, rip = 0x118
;   block_kind       = 0x158

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
    mov rax, [r15 + 0x158]              ; prev->block_kind
    test rax, rax
    jnz .ring0_save

    mov rax, [r15 + 0x118]              ; prev->rip
    cmp rax, KERNEL_BASE
    jae .ring0_save

    ; --- USER-PREEMPTED SAVE PATH ---
    push qword 0x2B                     ; SS       [0x98]
    push qword [r15 + 0x110]            ; RSP      [0x90]
    push qword 0x202                    ; RFLAGS   [0x88]
    push qword 0x33                     ; CS       [0x80]
    mov rax, [r15 + 0x118]              ; prev->rip
    test rax, rax
    jnz .have_rip
    mov rax, [r15 + 0x38]               ; fall back to entry_point
.have_rip:
    push rax                            ; RIP      [0x78]

    push qword [r15 + 0x108]            ; rax      [0x70]
    push qword [r15 + 0x100]            ; rbx      [0x68]
    push qword [r15 + 0x0F8]            ; rcx      [0x60]
    push qword [r15 + 0x0F0]            ; rdx      [0x58]
    push qword [r15 + 0x0E8]            ; rsi      [0x50]
    push qword [r15 + 0x0E0]            ; rdi      [0x48]
    push qword [r15 + 0x0D8]            ; rbp      [0x40]
    push qword [r15 + 0x0D0]            ; r8       [0x38]
    push qword [r15 + 0x0C8]            ; r9       [0x30]
    push qword [r15 + 0x0C0]            ; r10      [0x28]
    push qword [r15 + 0x0B8]            ; r11      [0x20]
    push qword [r15 + 0x0B0]            ; r12      [0x18]
    push qword [r15 + 0x0A8]            ; r13      [0x10]
    push qword [r15 + 0x0A0]            ; r14      [0x08]
    push qword [r15 + 0x098]            ; r15      [0x00]

    mov [r15 + 0x110], rsp              ; prev->rsp = frame base
    mov rax, cr3
    mov [r15 + 0x30], rax
    jmp .skip_save

    ; --- RING-0 SAVE PATH ---
    ;
    ; Called when a kernel-mode process yields voluntarily. At this
    ; point, the shell/process has called us through the normal C
    ; ABI, so the live callee-saved registers are the ones the caller
    ; expects to be preserved: rbp, rbx, r12, r13, r14, r15.
    ;
    ; They were pushed at function entry and are on the stack at:
    ;   [rsp + 0x00] = r15
    ;   [rsp + 0x08] = r14
    ;   [rsp + 0x10] = r13
    ;   [rsp + 0x18] = r12
    ;   [rsp + 0x20] = rbx
    ;   [rsp + 0x28] = rbp
    ;   [rsp + 0x30] = caller's return address
    ;
    ; We MUST copy those live values into the PCB before building the
    ; resume frame. If we don't, the frame is built from the PCB's
    ; *stale* register values (whatever was last written by the timer
    ; preemption handler or by process_create's initial frame — which
    ; sets everything to zero). On resume, `pop rbp` restores 0, and
    ; the very first `[rbp - N]` access in the resumed function faults
    ; with CR2 = -N. That is exactly the #PF at kmain_shell_loop+0x10B
    ; (`movzbl -0x2a(%rbp), %eax`) that motivated this fix.
    ;
    ; Caller-saved registers (rax, rcx, rdx, rsi, rdi, r8-r11) are
    ; intentionally not saved: the C ABI lets context_switch clobber
    ; them, so the caller has already spilled any live values across
    ; the call.
.ring0_save:
    ; Persist the live callee-saved registers into the PCB.
    mov rax, [rsp + 0x00]
    mov [r15 + 0x098], rax              ; r15
    mov rax, [rsp + 0x08]
    mov [r15 + 0x0A0], rax              ; r14
    mov rax, [rsp + 0x10]
    mov [r15 + 0x0A8], rax              ; r13
    mov rax, [rsp + 0x18]
    mov [r15 + 0x0B0], rax              ; r12
    mov rax, [rsp + 0x20]
    mov [r15 + 0x100], rax              ; rbx
    mov rax, [rsp + 0x28]
    mov [r15 + 0x0D8], rax              ; rbp

    ; Now build the resume frame from the (now up-to-date) PCB values.
    mov rax, [rsp + 48]                 ; caller's return address = kernel RIP
    push rax                            ; temp0 (RIP)  @ [rsp+0]
    lea rax, [rsp + 64]                 ; caller's RSP = orig_rsp + 56
    push rax                            ; temp1 (RSP)  @ [rsp+0], temp0 @ [rsp+8]

    push qword 0x20                     ; SS       [0x98]
    push qword [rsp + 8]                ; RSP      [0x90]  <- temp1
    push qword 0x202                    ; RFLAGS   [0x88]
    push qword 0x18                     ; CS       [0x80]
    push qword [rsp + 40]               ; RIP      [0x78]  <- temp0

    push qword [r15 + 0x108]            ; rax      [0x70]
    push qword [r15 + 0x100]            ; rbx      [0x68]
    push qword [r15 + 0x0F8]            ; rcx      [0x60]
    push qword [r15 + 0x0F0]            ; rdx      [0x58]
    push qword [r15 + 0x0E8]            ; rsi      [0x50]
    push qword [r15 + 0x0E0]            ; rdi      [0x48]
    push qword [r15 + 0x0D8]            ; rbp      [0x40]
    push qword [r15 + 0x0D0]            ; r8       [0x38]
    push qword [r15 + 0x0C8]            ; r9       [0x30]
    push qword [r15 + 0x0C0]            ; r10      [0x28]
    push qword [r15 + 0x0B8]            ; r11      [0x20]
    push qword [r15 + 0x0B0]            ; r12      [0x18]
    push qword [r15 + 0x0A8]            ; r13      [0x10]
    push qword [r15 + 0x0A0]            ; r14      [0x08]
    push qword [r15 + 0x098]            ; r15      [0x00]

    mov [r15 + 0x110], rsp              ; prev->rsp = frame base
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
    mov rcx, [r12 + 0x70]
    invlpg [rcx]

    mov rax, [r12 + 0x108]
    mov rbx, [r12 + 0x100]
    mov rcx, [r12 + 0x0F8]
    mov rdx, [r12 + 0x0F0]
    mov rbp, [r12 + 0x0D8]
    mov r8,  [r12 + 0x0D0]
    mov r9,  [r12 + 0x0C8]
    mov r10, [r12 + 0x0C0]
    mov r11, [r12 + 0x0B8]
    mov r13, [r12 + 0x0A8]
    mov r14, [r12 + 0x0A0]
    mov r15, [r12 + 0x098]
    mov rdi, [r12 + 0x0E0]
    mov rsi, [r12 + 0x0E8]

    push qword 0x2B
    push qword [r12 + 0x70]
    push qword 0x3202
    push qword 0x33
    push qword [r12 + 0x38]

    mov r12, [r12 + 0x0B0]
    iretq

.kernel_task:
    ; Frame validation: catch a corrupt iretq frame before it produces
    ; an opaque kernel-mode #GP. The expected frame is a 20-slot block
    ; at [r12 + 0x110], with rip at +0x78 and cs at +0x80. A healthy
    ; kernel-mode frame has rip >= KERNEL_BASE and cs == 0x18. If
    ; either check fails, emit a single marker byte on COM1 and halt.
    ;
    ; This check costs two compares and two conditional branches per
    ; switch. It is left in permanently: the failure mode it catches
    ; (a corrupt resume frame producing a #GP on the iretq with no
    ; indication of which slot was wrong) is otherwise very hard to
    ; diagnose, and the checks are invisible on the healthy path.
    mov rsp, [r12 + 0x110]

    mov rbx, [rsp + 0x78]               ; frame's rip
    mov rcx, KERNEL_BASE
    cmp rbx, rcx
    jae .frame_rip_ok

    mov dx, 0x3F8                       ; COM1
    mov al, 'R'                         ; rip out of range
    out dx, al
    mov al, 0x0A
    out dx, al
    hlt

.frame_rip_ok:
    mov rbx, [rsp + 0x80]               ; frame's cs
    cmp rbx, 0x18
    je .frame_cs_ok

    mov dx, 0x3F8
    mov al, 'C'                         ; cs != 0x18
    out dx, al
    mov al, 0x0A
    out dx, al
    hlt

.frame_cs_ok:
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
