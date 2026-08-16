[bits 64]
default rel

section .text
global jump_to_user_mode
extern serial_print
extern serial_print_hex

jump_to_user_mode:
    cli

    ; entry in rdi, stack in rsi (already aligned by C)

    push qword 0x2B        ; SS = user data
    push rsi               ; RSP = user stack
    pushfq
    pop rax
    or rax, 0x3200         ; IF=1, IOPL=3
    push rax               ; RFLAGS
    push qword 0x33        ; CS = user code
    push rdi               ; RIP = entry

    ; After building the frame, before iretq:
    mov dx, 0x3F8
    mov al, 'F'
    out dx, al
    mov al, ':'
    out dx, al
    mov rax, [rsp]        ; RIP
    call serial_print_hex
    mov al, ' '
    out dx, al
    mov rax, [rsp+8]      ; CS
    call serial_print_hex
    mov al, ' '
    out dx, al
    mov rax, [rsp+16]     ; RFLAGS
    call serial_print_hex
    mov al, ' '
    out dx, al
    mov rax, [rsp+24]     ; RSP
    call serial_print_hex
    mov al, ' '
    out dx, al
    mov rax, [rsp+32]     ; SS
    call serial_print_hex
    mov al, 0x0A
    out dx, al
    mov al, 0x0D
    out dx, al

    iretq
