global _start

section .text
_start:
    sti
    mov rbx, 0

loop:
    mov rax, 1
    mov rdi, 1
    mov rsi, msg
    mov rdx, msg_len
    syscall
    
    inc rbx
    cmp rbx, 5          ; Exit after 5 iterations
    jl loop

    ; SYS_EXIT
    mov rax, 2
    mov rdi, 0
    syscall

section .data
msg: db "Hello from User1!", 10
msg_len equ $ - msg




;~ global _start

;~ section .text
;~ _start:
    ;~ ; Enable interrupts
    ;~ sti

;~ loop:
    ;~ ; SYS_WRITE
    ;~ mov rax, 1
    ;~ mov rdi, 1
    ;~ mov rsi, msg
    ;~ mov rdx, msg_len
    ;~ syscall

    ;~ ; Jump back to loop
    ;~ jmp loop

;~ section .data
;~ msg: db "Hello from User1!", 10
;~ msg_len equ $ - msg
