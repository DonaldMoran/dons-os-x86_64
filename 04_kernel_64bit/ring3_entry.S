section .text
global jump_to_user_mode

jump_to_user_mode:
    ; Debug marker 1 - we entered the function
    mov rax, 0xDEADBEEF
    mov rbx, 0xCAFEBABE
    
    ; Save arguments - use r12/r13 (callee-saved)
    push r12
    push r13
    push r14          ; Also save r14 for debug
    push r15          ; Also save r15 for debug
    
    mov r12, rdi      ; Save entry point (0x4000000000)
    mov r13, rsi      ; Save user stack (0x400000000FF0)
    
    ; Debug: Store the values in registers before pushing
    mov r14, r12      ; Entry point
    mov r15, r13      ; User stack
    
    ; Align RSP to 16 bytes
    and rsp, 0xFFFFFFFFFFFFFFF0
    
    ; Build iretq frame (in reverse order)
    ; SS, RSP, RFLAGS, CS, RIP
    
    ; 1. Push SS (user data selector with RPL=3)
    push qword 0x33
    
    ; 2. Push RSP (user stack)
    push r13
    
    ; 3. Push RFLAGS (0x202 = interrupts enabled)
    push qword 0x202
    
    ; 4. Push CS (user code selector with RPL=3)
    push qword 0x2B
    
    ; 5. Push RIP (user entry)
    push r12
    
    ; === DEBUG: Read the stack frame values ===
    ; These will be visible in the QEMU register dump on page fault
    mov r8, [rsp]      ; RIP - should be 0x4000000000
    mov r9, [rsp+8]    ; CS - should be 0x2B
    mov r10, [rsp+16]  ; RFLAGS - should be 0x202
    mov r11, [rsp+24]  ; RSP - should be user stack
    mov r12, [rsp+32]  ; SS - should be 0x33
    
    ; Debug marker 2 - values are loaded
    mov rax, 0xAAAAAAAA
    mov rbx, 0xBBBBBBBB
    mov rcx, 0xCCCCCCCC
    mov rdx, 0xDDDDDDDD
    ; === END DEBUG ===
    
    ; Set data segments to user selectors
    mov ax, 0x33
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    
    ; Debug marker 3 - about to iretq
    mov rax, 0x55555555
    mov rbx, 0x66666666
    
    ; Execute iretq
    iretq
    
    ; Should never reach here
    cli
    hlt
    ret
