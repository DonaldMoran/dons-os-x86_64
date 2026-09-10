[bits 64]
default rel

section .text
global context_switch

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

    mov [rdi + 0x70], rax
    mov [rdi + 0x78], rbx
    mov [rdi + 0x80], rcx
    mov [rdi + 0x88], rdx
    mov [rdi + 0x90], rsi
    mov [rdi + 0x98], rdi
    mov [rdi + 0xA0], rbp
    mov [rdi + 0xA8], r8
    mov [rdi + 0xB0], r9
    mov [rdi + 0xB8], r10
    mov [rdi + 0xC0], r11
    mov [rdi + 0xC8], r12
    mov [rdi + 0xD0], r13
    mov [rdi + 0xD8], r14
    mov [rdi + 0xE0], r15

    mov [rdi + 0xE8], rsp   ; save current RSP
    mov rax, [rsp]
    mov [rdi + 0xF0], rax   ; save current RIP (return address)

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
    mov rax, [r12 + 0x70]
    mov rbx, [r12 + 0x78]
    mov rcx, [r12 + 0x80]
    mov rdx, [r12 + 0x88]
    mov rbp, [r12 + 0xA0]
    mov r8,  [r12 + 0xA8]
    mov r9,  [r12 + 0xB0]
    mov r10, [r12 + 0xB8]
    mov r11, [r12 + 0xC0]
    mov r13, [r12 + 0xD0]
    mov r14, [r12 + 0xD8]
    mov r15, [r12 + 0xE0]
    mov rdi, [r12 + 0x98]
    mov rsi, [r12 + 0x90]

    ; Build iretq frame for ring 3
    push qword 0x2B         ; SS (user data selector with RPL=3)
    push qword [r12 + 0x68] ; RSP (user stack top)
    
    ; CRITICAL FIX: Force an explicit clean 64-bit user flag structure (0x3202)
    ; Bit 1 (0x02) = Mandatory System Reserved Bit
    ; Bit 9 (0x0200) = Interrupt Flag Enabled (IF=1), allowing hardware timer ticks
    ; Bits 12-13 (0x3000) = Input/Output Privilege Level set to Ring 3 (IOPL=3)
    ; This explicitly allows standard Newlib runtime libraries to coordinate unprivileged code!
    push qword 0x3202       ; RFLAGS (IF=1, IOPL=3, Clean Long Mode structure)
    
    push qword 0x33         ; CS (user code selector with RPL=3)
    push qword [r12 + 0x38] ; RIP (entry point)

    ; Clear r12 last since it was holding our pcb_t pointer structure
    mov r12, [r12 + 0xC8]

    iretq

.kernel_task:
    ; ---- Kernel task (idle, etc.): just switch stack and return ----
    ; Restore general registers (except RSP and RIP)
    mov rax, [r12 + 0x70]
    mov rbx, [r12 + 0x78]
    mov rcx, [r12 + 0x80]
    mov rdx, [r12 + 0x88]
    mov rbp, [r12 + 0xA0]
    mov r8,  [r12 + 0xA8]
    mov r9,  [r12 + 0xB0]
    mov r10, [r12 + 0xB8]
    mov r11, [r12 + 0xC0]
    mov r13, [r12 + 0xD0]
    mov r14, [r12 + 0xD8]
    mov r15, [r12 + 0xE0]
    mov rdi, [r12 + 0x98]
    mov rsi, [r12 + 0x90]

    ; Switch stack to the new task's kernel stack
    mov rsp, [r12 + 0xE8]   ; load saved RSP

    ; Jump to the new task's entry point
    mov rax, [r12 + 0xF0]   ; load saved RIP
    jmp rax

.restore_and_return:
    pop r15
    pop r14
    pop r13
    pop r12
    pop rbx
    pop rbp
    ret
