[bits 64]
default rel

section .text
global user_syscall_entry
global syscall_init_asm

extern syscall_dispatch
extern process_exit
extern serial_print
extern serial_print_hex
extern syscall_pre_sysret_diag

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
; On entry:
;   RCX = user RIP
;   R11 = user RFLAGS
;   RAX = syscall number
;   RDI, RSI, RDX, R10, R8, R9 = args
;
; We must preserve ALL user-visible callee-saved registers
; (rbx, rbp, r12, r13, r14, r15) across the call to syscall_dispatch.
;
; Additionally, we save the user's RCX and R11 so sysret can restore them.
; ---------------------------------------------------------------------------
user_syscall_entry:
    ; ---- Save the original callee-saved registers FIRST ----
    ; We must not clobber any of these before they are saved,
    ; because the C ABI requires them preserved across the syscall.
    push rbx
    push rbp
    push r12
    push r13
    push r14
    push r15

    ; Save the syscall return RIP / RFLAGS that the CPU put in RCX / R11.
    ; These are the values sysret will use. Store them in the frame.
    push rcx                        ; user RIP
    push r11                        ; user RFLAGS

    ; Save user RSP too (syscall does NOT save RSP; it's still the user's).
    ; We don't strictly need to, because RSP is unchanged, but keeping it
    ; explicit makes the frame layout symmetric and debuggable.
    push rsp                        ; user RSP (as-is)

    ; Save volatile registers that syscall_dispatch may clobber but
    ; which userland might expect to survive? Actually caller-saved regs
    ; (rax, rdi, rsi, rdx, rcx, r8, r9, r10, r11) are by ABI not preserved
    ; across syscalls. Only callee-saved (rbx, rbp, r12-r15) must be.
    ; We've saved those above. Push r8/r9 only so we can use them as args.
    push r8
    push r9

    ; ---- Argument shuffling ----
    ; userland: rax=num, rdi=arg0, rsi=arg1, rdx=arg2, r10=arg3, r8=arg4, r9=arg5
    ; C:        rdi,   rsi,   rdx,   rcx,   r8,   r9,   [stack]
    push r9                         ; arg5 -> 7th arg (on stack)
    mov rbx, rax                    ; save syscall number for later
    mov r9, r8                      ; arg4 -> r9
    mov r8, r10                     ; arg3 -> r8
    mov rcx, rdx                    ; arg2 -> rcx
    mov rdx, rsi                    ; arg1 -> rdx
    mov rsi, rdi                    ; arg0 -> rsi
    mov rdi, rax                    ; num  -> rdi

    call syscall_dispatch
    add rsp, 8                      ; discard stacked arg5

    ; ---- SYS_EXIT check ----
    cmp rbx, 2
    je .handle_exit

    ; ---- Restore user-visible state ----
    ; Discard the r8/r9 slots we pushed (their caller-saved, no need to restore)
    pop r9
    pop r8

    ; Discard user RSP slot (it was never changed)
    add rsp, 8

    ; Restore user RFLAGS into r11 and user RIP into rcx (for sysret)
    pop r11                         ; user RFLAGS
    pop rcx                         ; user RIP

    ; Restore callee-saved registers. ORDER IS REVERSE OF PUSH.
    pop r15
    pop r14
    pop r13
    pop r12
    pop rbp
    pop rbx

    ; At this point, RCX = user RIP, R11 = user RFLAGS, RSP = user RSP.
    ; All callee-saved registers have their user values.

    ; ---- Optional diagnostic ----
    push rsp
    push rcx
    push r11
    mov rdi, rcx
    mov rsi, r11
    sub rsp, 8
    call syscall_pre_sysret_diag
    add rsp, 8
    pop r11
    pop rcx
    pop rsp

    o64 sysret

.handle_exit:
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

section .data
exit_msg: db "SYS_EXIT: status=", 0
newline: db 0x0A, 0x0D, 0
