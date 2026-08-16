global _start

_start:
    mov rax, 1          ; SYS_WRITE
    mov rdi, 1          ; fd = stdout
    mov rsi, msg        ; buffer
    mov rdx, msg_len    ; length
    syscall

    cli
    hlt

msg: db "Hello from user mode!", 10
msg_len equ $ - msg

