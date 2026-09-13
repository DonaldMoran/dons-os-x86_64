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

; CORE SYSTEM LINK REGISTER FOR PREEMPTIVE SCHEDULING:
extern timer_preempt_handler
extern irq1_handler
extern isr13_handler

; =====================================================================
; Macros for GPR save/restore. Used by every stub that calls C.
; The C ABI lets the callee clobber rax, rcx, rdx, rsi, rdi, r8..r11,
; so any stub that returns to interrupted code with iretq must restore
; all of those. rbx, rbp, r12..r15 are callee-saved and would normally
; be preserved by the callee, but the kernel's C is compiled freestanding
; with the standard SysV convention, so a stray inline-asm clobber or a
; compiler bug could leave them dirty. Push them all; the cost is
; negligible and the failure mode (a corrupted register that only shows
; up 50 instructions later on an iretq) is otherwise unfindable.
; =====================================================================
%macro PUSH_ALL_GPRS 0
    push rax
    push rbx
    push rcx
    push rdx
    push rsi
    push rdi
    push rbp
    push r8
    push r9
    push r10
    push r11
    push r12
    push r13
    push r14
    push r15
%endmacro

%macro POP_ALL_GPRS 0
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
%endmacro

; =====================================================================
; IRQ0 — PIT timer. Special: the C handler returns the RSP to resume
; from, and the stub uses it to switch stacks. This is the scheduler's
; ABI boundary; do not change the frame layout without also changing
; timer_preempt_handler and the initial frame built in process_create.
; =====================================================================
irq0_stub:
    ; CPU has automatically pushed: SS, RSP, RFLAGS, CS, RIP
    PUSH_ALL_GPRS

    ; Pass the frame base as argument 1 (RDI)
    mov rdi, rsp

    ; Invoke the C preemptive time-slicer
    call timer_preempt_handler

    ; The handler returns the RSP to resume from in RAX. If it was a
    ; context switch, RAX is the incoming process's saved frame base;
    ; otherwise it is our own frame base (no-op switch).
    mov rsp, rax

    POP_ALL_GPRS

    ; EOI to master PIC
    push rax
    mov al, 0x20
    out 0x20, al
    pop rax

    iretq

; =====================================================================
; IRQ1 — PS/2 keyboard.
; =====================================================================
irq1_stub:
    PUSH_ALL_GPRS
    mov rdi, rsp
    call irq1_handler
    POP_ALL_GPRS
    iretq

; =====================================================================
; CPU exceptions. The CPU pushes an error code for some vectors
; (8, 13, 14) and not for others (0, 1). The stubs push a dummy 0 for
; the ones that don't, so the C handler always sees:
;   [rsp+0]  = error code
;   [rsp+8]  = RIP
;   [rsp+16] = CS
;   [rsp+24] = RFLAGS
;   [rsp+32] = RSP
;   [rsp+40] = SS
; =====================================================================

; #DE — divide by zero (no CPU error code)
isr0_stub:
    push 0                  ; fake error code
    PUSH_ALL_GPRS
    mov rdi, rsp
    call isr0_handler
    POP_ALL_GPRS
    add rsp, 8              ; drop fake error code
    iretq

; #DB — debug (no CPU error code)
isr1_stub:
    push 0
    PUSH_ALL_GPRS
    mov rdi, rsp
    call isr1_handler
    POP_ALL_GPRS
    add rsp, 8
    iretq

; #DF — double fault (CPU pushes an error code, always 0)
isr8_stub:
    PUSH_ALL_GPRS
    mov rdi, rsp
    call isr8_handler
    POP_ALL_GPRS
    add rsp, 8              ; drop CPU error code
    iretq

; #GP — general protection fault (CPU pushes an error code)
isr13_stub:
    cli
    PUSH_ALL_GPRS
    mov rdi, rsp
    call isr13_handler
    POP_ALL_GPRS
    add rsp, 8              ; drop CPU error code
    iretq

; #PF — page fault (CPU pushes an error code)
isr14_stub:
    cli
    PUSH_ALL_GPRS
    mov rdi, rsp
    call isr14_handler
    POP_ALL_GPRS
    add rsp, 8              ; drop CPU error code
    iretq
