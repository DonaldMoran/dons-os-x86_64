[bits 16]
[org 0x10000]

start:
    cli
    mov ax, 0x1000
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7C00

    in  al, 0x92
    or  al, 00000010b
    out 0x92, al

    mov si, dap_kernel
    mov dl, 0x80
    mov ah, 0x42
    int 0x13

    xor ebx, ebx
    mov di, e820_buffer
    mov dword [e820_count], 0

e820_loop:
    mov eax, 0xE820
    mov ecx, 24
    mov edx, 0x534D4150
    int 0x15
    jc e820_done
    cmp eax, 0x534D4150
    jne e820_done
    add di, 24
    inc word [e820_count]
    test ebx, ebx
    jnz e820_loop

e820_done:

    lgdt [gdt_descriptor]
    mov eax, cr0
    or  eax, 1
    mov cr0, eax
    jmp dword 0x08:pm_entry

dap_kernel:
    db 16
    db 0
    dw 128
    dw 0x0000
    dw 0x8000
    dq 64

[bits 32]
pm_entry:
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov esp, 0x90000

    mov eax, cr4
    or  eax, 1 << 5
    mov cr4, eax

    mov eax, pml4
    mov cr3, eax

    mov ecx, 0xC0000080
    rdmsr
    or  eax, 1 << 8
    wrmsr

    mov eax, cr0
    or  eax, 0x80000000
    mov cr0, eax

    push dword 0x18
    push dword long_mode_entry
    retf

[bits 64]
long_mode_entry:
[default rel]
    mov rsp, 0x80000

    mov rbx, bootinfo
    mov qword [rbx + 0], e820_buffer
    movzx rax, word [e820_count]
    mov qword [rbx + 8], rax
    mov qword [rbx + 16], 0
    mov dword [rbx + 24], 0
    mov dword [rbx + 28], 0
    mov dword [rbx + 32], 0
    mov dword [rbx + 36], 0
    mov qword [rbx + 40], 0x00100000
    mov qword [rbx + 48], 0x00100000 + (64 * 512)
    mov rax, pml4
    mov qword [rbx + 56], rax

    mov rsi, 0x00080000
    mov rdi, 0x00100000
    mov rcx, 8192
    rep movsq

    mov rdi, bootinfo
    mov rax, 0xFFFFFFFF80100000
    jmp rax

gdt_start:
    dq 0
    dq 0x00CF9A000000FFFF
    dq 0x00CF92000000FFFF
    dq 0x00AF9A000000FFFF
gdt_end:

gdt_descriptor:
    dw gdt_end - gdt_start - 1
    dd gdt_start

align 4096
pml4:
    dq pdpt_identity + 3
    times 509 dq 0
    dq pml4 + 0x003
    dq pdpt_higher + 3

align 4096
pdpt_identity:
    dq pd + 3
    times 511 dq 0

align 4096
pdpt_higher:
    times 509 dq 0
    dq pml4 + 0x003
    dq pd + 3
    dq 0

; Identity PD - maps 128 * 2MB = 256MB
align 4096
pd:
    %assign i 0
    %rep 128
        dq (i * 0x200000) + 0x83
        %assign i i+1
    %endrep
    times (512 - 128) dq 0

bootinfo:
    dq 0
    dq 0
    dq 0
    dd 0
    dd 0
    dd 0
    dd 0
    dq 0
    dq 0
    dq 0

e820_buffer:
    times 64*24 db 0

e820_count:
    dw 0
    dd 0
