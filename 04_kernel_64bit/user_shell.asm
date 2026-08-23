BITS 64
default rel

; System call numbers
%define SYS_WRITE 1
%define SYS_EXIT  2
%define SYS_READ  3

; Constants
%define MAX_CMD_LEN 64

section .text
global _start

_start:
    ; Print welcome message
    lea rdi, [rel welcome_msg]
    call print_string

shell_loop:
    ; Print prompt
    lea rdi, [rel prompt]
    call print_string

    ; Read command
    lea rdi, [rel cmd_buffer]
    mov rsi, MAX_CMD_LEN
    call read_line

    ; Check for empty command
    cmp byte [rel cmd_buffer], 0
    je shell_loop

    ; Check for "exit"
    lea rdi, [rel cmd_buffer]
    lea rsi, [rel exit_cmd]
    call strcmp
    test rax, rax
    jz .exit

    ; Check for "help"
    lea rdi, [rel cmd_buffer]
    lea rsi, [rel help_cmd]
    call strcmp
    test rax, rax
    jz .help

    ; Check for "clear"
    lea rdi, [rel cmd_buffer]
    lea rsi, [rel clear_cmd]
    call strcmp
    test rax, rax
    jz .clear

    ; Check for "whoami"
    lea rdi, [rel cmd_buffer]
    lea rsi, [rel whoami_cmd]
    call strcmp
    test rax, rax
    jz .whoami

    ; Unknown command
    lea rdi, [rel unknown_msg]
    call print_string
    jmp shell_loop

.help:
    lea rdi, [rel help_msg]
    call print_string
    jmp shell_loop

.clear:
    lea rdi, [rel clear_msg]
    call print_string
    jmp shell_loop

.whoami:
    lea rdi, [rel whoami_msg]
    call print_string
    jmp shell_loop

.exit:
    lea rdi, [rel goodbye_msg]
    call print_string
    mov rax, SYS_EXIT
    mov rdi, 0
    syscall

; ============================================================
; print_string: Print a null-terminated string
; Input: RDI = pointer to string
; ============================================================
print_string:
    push rsi
    push rdx
    push rax

    ; Calculate string length
    xor rsi, rsi
    mov rdx, rdi
.loop:
    cmp byte [rdx + rsi], 0
    je .done
    inc rsi
    jmp .loop
.done:

    ; System call: SYS_WRITE
    mov rax, SYS_WRITE
    mov rdi, 1          ; stdout
    ; rsi already has length
    ; rdx already has pointer
    syscall

    pop rax
    pop rdx
    pop rsi
    ret

; ============================================================
; read_line: Read a line from stdin
; Input: RDI = buffer pointer, RSI = buffer size
; Output: Buffer filled with null-terminated string
; ============================================================
read_line:
    push rdi
    push rsi
    push rcx
    push rax

    mov rcx, rsi        ; max bytes to read
    mov rdi, rdi        ; buffer pointer
    xor rsi, rsi        ; bytes read

.read_loop:
    push rdi
    push rsi
    push rcx
    push rax
    push rdx

    ; Read one character
    mov rax, SYS_READ
    mov rdi, 0          ; stdin
    lea rsi, [rel char_buf]
    mov rdx, 1
    syscall

    pop rdx
    pop rax
    pop rcx
    pop rsi
    pop rdi

    ; Check if read was successful
    test rax, rax
    jz .read_loop

    ; Get the character
    mov al, byte [rel char_buf]

    ; Check for newline
    cmp al, 10          ; '\n'
    je .done

    ; Check for backspace
    cmp al, 8           ; '\b'
    jne .store_char

    ; Backspace: remove last character
    cmp rsi, 0
    je .read_loop
    dec rsi
    ; Echo backspace to console
    push rax
    push rdi
    push rsi
    push rcx
    push rdx
    lea rsi, [rel backspace_echo]
    mov rax, SYS_WRITE
    mov rdi, 1
    mov rdx, 3
    syscall
    pop rdx
    pop rcx
    pop rsi
    pop rdi
    pop rax
    jmp .read_loop

.store_char:
    ; Store character in buffer
    mov byte [rdi + rsi], al
    inc rsi

    ; Echo character to console
    push rax
    push rdi
    push rsi
    push rcx
    push rdx
    lea rsi, [rel char_buf]
    mov rax, SYS_WRITE
    mov rdi, 1
    mov rdx, 1
    syscall
    pop rdx
    pop rcx
    pop rsi
    pop rdi
    pop rax

    ; Check if buffer is full
    cmp rsi, rcx
    jge .done

    jmp .read_loop

.done:
    ; Null-terminate the buffer
    mov byte [rdi + rsi], 0

    ; Print newline
    push rax
    push rdi
    push rsi
    push rcx
    push rdx
    lea rsi, [rel newline]
    mov rax, SYS_WRITE
    mov rdi, 1
    mov rdx, 1
    syscall
    pop rdx
    pop rcx
    pop rsi
    pop rdi
    pop rax

    pop rax
    pop rcx
    pop rsi
    pop rdi
    ret

; ============================================================
; strcmp: Compare two strings
; Input: RDI = string 1, RSI = string 2
; Output: RAX = 0 if equal, non-zero otherwise
; ============================================================
strcmp:
    push rsi
    push rdi
    push rcx

    xor rcx, rcx
.loop:
    mov al, byte [rdi + rcx]
    mov bl, byte [rsi + rcx]
    cmp al, bl
    jne .not_equal
    cmp al, 0
    je .equal
    inc rcx
    jmp .loop

.equal:
    xor rax, rax
    jmp .done

.not_equal:
    mov rax, 1

.done:
    pop rcx
    pop rdi
    pop rsi
    ret

section .data
    welcome_msg db "User Shell v0.1", 10, "Type 'help' for commands", 10, 0
    prompt db "$> ", 0
    help_cmd db "help", 0
    exit_cmd db "exit", 0
    clear_cmd db "clear", 0
    whoami_cmd db "whoami", 0
    unknown_msg db "Unknown command. Type 'help'", 10, 0
    help_msg db "Commands:", 10, "  help   - Show this help", 10, "  exit   - Exit shell", 10, "  clear  - Clear screen", 10, "  whoami - Show current user", 10, 0
    clear_msg db 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 0
    whoami_msg db "user", 10, 0
    goodbye_msg db "Goodbye!", 10, 0
    newline db 10, 0
    backspace_echo db 8, 32, 8, 0
    char_buf db 0

section .bss
    cmd_buffer resb MAX_CMD_LEN
