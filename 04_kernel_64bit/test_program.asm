global _start

section .text
_start:
    mov rax, 1
    mov rdi, 1
    lea rsi, [rel msg1]
    mov rdx, 22
    syscall

    mov rax, 1
    mov rdi, 1
    lea rsi, [rel msg2]
    mov rdx, 26
    syscall

    cli
hang_loop:
    hlt
    jmp hang_loop

section .data
msg1: db "Hello from userland!", 0x0A
msg2: db "This is a second line.", 0x0A

;~ global _start

;~ section .text
;~ _start:
    ;~ ; SYS_WRITE
    ;~ mov rax, 1          ; SYS_WRITE
    ;~ mov rdi, 1          ; fd = stdout
    ;~ mov rsi, msg        ; buffer
    ;~ mov rdx, msg_len    ; length
    ;~ syscall

    ;~ ; SYS_EXIT
    ;~ mov rax, 2          ; SYS_EXIT
    ;~ mov rdi, 0          ; status = 0
    ;~ syscall

    ;~ ; Should never reach here
    ;~ cli
    ;~ hlt

;~ section .data
;~ msg: db "Hello from user mode!", 10
;~ msg_len equ $ - msg
