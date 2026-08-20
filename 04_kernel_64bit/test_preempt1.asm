global _start

section .text
_start:
    ; Enable interrupts
    sti

loop:
    ; SYS_WRITE
    mov rax, 1
    mov rdi, 1
    mov rsi, msg
    mov rdx, msg_len
    syscall

    ; Jump back to loop
    jmp loop

section .data
msg: db "Hello from User1!", 10
msg_len equ $ - msg
