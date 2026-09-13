[bits 64]
default rel

section .text
global user_syscall_entry
global syscall_init_asm

extern syscall_dispatch
extern process_exit
extern serial_print
extern serial_print_hex
extern g_syscall_stack_top

; ---------------------------------------------------------------------------
; Syscall init: set up EFER, STAR, LSTAR, FMASK
; ---------------------------------------------------------------------------
syscall_init_asm:
    mov ecx, 0xC0000080
    rdmsr
    or eax, 0x1
    wrmsr

    mov ecx, 0xC0000081
    xor edx, edx
    xor eax, eax
    mov edx, 0x00180000
    mov eax, 0x00200000
    wrmsr

    mov ecx, 0xC0000082
    mov rax, user_syscall_entry
    wrmsr

    mov ecx, 0xC0000084
    mov eax, 0x00000200
    xor edx, edx
    wrmsr

    ret

; ---------------------------------------------------------------------------
; Syscall entry from usermode
;
; On entry (set by the CPU):
;   RCX = user RIP (return address)
;   R11 = user RFLAGS
;   RSP = user stack pointer (UNCHANGED by the syscall instruction)
;   CS  = 0x18, SS = 0x20 (from STAR)
;
; Plan:
;   1. cli. The next few instructions touch a global scratch slot and
;      the syscall stack pointer. An interrupt in this window would
;      find the CPU on the user stack with the kernel stack not yet
;      installed, which is exactly the bug class this change removes.
;   2. Save user RSP to a global scratch slot.
;   3. Load RSP from g_syscall_stack_top.
;   4. sti. From here on we are on the kernel stack, and the timer
;      path (TSS.RSP0 == same kernel stack) is safe to nest.
;   5. Push the 6 callee-saved GPRs, RCX, R11, the user RSP slot, and
;      R8/R9. The layout matches the old code so the arg shuffle below
;      is unchanged.
;   6. Shuffle args, call syscall_dispatch.
;   7. On return: pop everything, restore user RSP from the pushed
;      slot (staged through R10, which sysret does not read), sysret.
;
; The global scratch g_user_rsp_save is safe on UP because the window
; between the store and the load of RSP is covered by cli.
; ---------------------------------------------------------------------------

section .bss
align 8
g_user_rsp_save: resq 1

section .text

user_syscall_entry:
    cli

    ; Save user RSP. It is pushed later as part of the frame so the
    ; epilogue can recover it symmetrically.
    mov [rel g_user_rsp_save], rsp

    ; Switch to the per-process kernel stack.
    mov rsp, [rel g_syscall_stack_top]
    test rsp, rsp
    jz .no_stack

    sti

    ; Save original callee-saved registers FIRST.
    push rbx
    push rbp
    push r12
    push r13
    push r14
    push r15

    ; Save the syscall return RIP / RFLAGS that the CPU put in RCX / R11.
    push rcx                        ; user RIP
    push r11                        ; user RFLAGS

    ; Push the saved user RSP slot (the value lives in the global).
    push qword [rel g_user_rsp_save]

    ; Save r8/r9 so we can freely use them during arg shuffling.
    push r8
    push r9

    ; Arg5 -> 7th C argument, on stack.
    push r9
    mov rbx, rax                    ; save syscall number
    mov r9, r8                      ; arg4 -> r9
    mov r8, r10                     ; arg3 -> r8
    mov rcx, rdx                    ; arg2 -> rcx
    mov rdx, rsi                    ; arg1 -> rdx
    mov rsi, rdi                    ; arg0 -> rsi
    mov rdi, rax                    ; num  -> rdi

    call syscall_dispatch
    add rsp, 8                      ; discard stacked arg5

    cmp rbx, 2
    je .handle_exit

    pop r9
    pop r8
    pop r10                         ; user RSP -> r10 (sysret does not read r10)
    pop r11                         ; user RFLAGS
    pop rcx                         ; user RIP

    pop r15
    pop r14
    pop r13
    pop r12
    pop rbp
    pop rbx

    mov rsp, r10
    o64 sysret

.handle_exit:
    ; Same teardown, but we do not return to user mode: process_exit
    ; never returns. The kernel stack we are on belongs to the exiting
    ; process and will be reused by whichever process gets its slot.
    pop r9
    pop r8
    add rsp, 8                      ; discard user RSP slot
    add rsp, 16                     ; discard r11, rcx
    pop r15
    pop r14
    pop r13
    pop r12
    pop rbp
    pop rbx

    push rax
    mov rdi, exit_msg
    call serial_print
    pop rax
    push rax
    call serial_print_hex
    mov rdi, newline
    call serial_print
    pop rax

    mov rdi, rax
    jmp process_exit

.no_stack:
    ; g_syscall_stack_top was zero. This should never happen once
    ; process_init has run. Emit a diagnostic and halt rather than
    ; corrupting kernel state.
    mov rsp, [rel g_user_rsp_save]
    mov rdi, no_stack_msg
    call serial_print
    cli
.hang:
    hlt
    jmp .hang

section .data
exit_msg: db "SYS_EXIT: status=", 0
newline: db 0x0A, 0x0D, 0
no_stack_msg: db "SYSCALL: g_syscall_stack_top is NULL!", 0x0A, 0x0D, 0
