[bits 64]
[section .text]
[global _start]

_start:
    ; Now we're running in higher-half (0xFFFFFFFF80000000)
    ; Set up stack in higher-half space
    mov rsp, 0xFFFFFFFF80090000
    
    ; Clear registers
    xor rax, rax
    xor rbx, rbx
    xor rcx, rcx
    xor rdx, rdx
    
    extern kmain
    call kmain

hang:
    jmp hang
