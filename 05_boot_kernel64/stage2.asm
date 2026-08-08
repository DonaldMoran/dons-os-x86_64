[bits 16]
[org 0x10000]

start:
    cli

    ; ----------------------------------------------------
    ; Breadcrumbs in real mode only
    ; ----------------------------------------------------
    mov ax, 0xB800
    mov ds, ax
    mov byte [0], '1'      ; stage2 start
    mov byte [1], 0x07

    mov byte [2], 'R'      ; stage2 running
    mov byte [3], 0x07

    ; ----------------------------------------------------
    ; Set up segment registers (real mode)
    ; ----------------------------------------------------
    mov ax, 0x1000
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7C00

    ; ----------------------------------------------------
    ; Enable A20
    ; ----------------------------------------------------
    in  al, 0x92
    or  al, 00000010b
    out 0x92, al

    ; ----------------------------------------------------
    ; Load kernel (64 sectors from LBA 64 → 0x8000:0000)
    ; ----------------------------------------------------
    mov si, dap_kernel
    mov dl, 0x80
    mov ah, 0x42
    int 0x13

    ; ----------------------------------------------------
    ; E820 memory map (real mode)
    ; ----------------------------------------------------
    xor ebx, ebx                 ; continuation value = 0
    mov di, e820_buffer          ; ES:DI = buffer
    mov dword [e820_count], 0    ; entry count = 0

e820_loop:
    mov eax, 0xE820
    mov ecx, 24                  ; size of buffer
    mov edx, 0x534D4150          ; 'SMAP'
    int 0x15

    jc e820_done                 ; error → stop
    cmp eax, 0x534D4150
    jne e820_done                ; bad signature → stop

    ; one entry written at ES:DI
    add di, 24                   ; advance buffer
    inc word [e820_count]        ; count++

    test ebx, ebx                ; EBX = 0 → last entry
    jnz e820_loop

e820_done:

    ; ----------------------------------------------------
    ; Load GDT (still real mode)
    ; ----------------------------------------------------
    lgdt [gdt_descriptor]

    ; ----------------------------------------------------
    ; Enter protected mode
    ; ----------------------------------------------------
    mov eax, cr0
    or  eax, 1
    mov cr0, eax

    ; *** IMPORTANT: use 32‑bit far jump ***
    jmp dword 0x08:pm_entry

; --------------------------------------------------------
; Disk Address Packet for kernel
; --------------------------------------------------------
dap_kernel:
    db 16
    db 0
    dw 64          ; sector count
    dw 0x0000      ; offset
    dw 0x8000      ; segment -> 0x00080000
    dq 64          ; LBA

; --------------------------------------------------------
; Protected mode entry (32-bit)
; --------------------------------------------------------
[bits 32]
pm_entry:
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov esp, 0x90000

    ; Enable PAE
    mov eax, cr4
    or  eax, 1 << 5
    mov cr4, eax

    ; Load PML4 (physical address)
    mov eax, pml4
    mov cr3, eax

    ; Enable long mode
    mov ecx, 0xC0000080
    rdmsr
    or  eax, 1 << 8
    wrmsr

    ; Enable paging
    mov eax, cr0
    or  eax, 0x80000000
    mov cr0, eax

    ; Far jump to 64‑bit mode
    push dword 0x18
    push dword long_mode_entry
    retf

; --------------------------------------------------------
; Long mode entry
; --------------------------------------------------------
[bits 64]
long_mode_entry:
[default rel]
    mov rsp, 0x80000

    xor rax, rax
    xor rbx, rbx
    xor rcx, rcx
    xor rdx, rdx

    ; ----------------------------------------------------
    ; Fill BootInfo
    ; ----------------------------------------------------
    mov rbx, bootinfo

    ; memory map
    mov qword [rbx + 0], e820_buffer      ; memory_map_addr
    movzx rax, word [e820_count]
    mov qword [rbx + 8], rax              ; memory_map_count

    ; framebuffer (none yet)
    mov qword [rbx + 16], 0               ; framebuffer_addr
    mov dword [rbx + 24], 0              ; framebuffer_width
    mov dword [rbx + 28], 0              ; framebuffer_height
    mov dword [rbx + 32], 0              ; framebuffer_pitch
    mov dword [rbx + 36], 0              ; framebuffer_bpp

    ; kernel physical range
    mov qword [rbx + 40], 0x00100000        ; kernel_phys_start
    mov qword [rbx + 48], 0x00100000 + (64 * 512) ; kernel_phys_end

    ; paging root
    mov rax, pml4
    mov qword [rbx + 56], rax        ; pml4_addr

    ; ----------------------------------------------------
    ; Move kernel from 0x00080000 → 0x00100000
    ; ----------------------------------------------------
    mov rsi, 0x00080000
    mov rdi, 0x00100000
    mov rcx, 1024
    rep movsq

    ; ----------------------------------------------------
    ; Pass BootInfo pointer in RDI and jump to kernel
    ; ----------------------------------------------------
    mov rdi, bootinfo
    mov rax, 0xFFFFFFFF80100000     ; Higher-half + 1MB offset
    jmp rax

; --------------------------------------------------------
; GDT
; --------------------------------------------------------
gdt_start:
    dq 0
    dq 0x00CF9A000000FFFF   ; 0x08: 32‑bit code
    dq 0x00CF92000000FFFF   ; 0x10: 32‑bit data
    dq 0x00AF9A000000FFFF   ; 0x18: 64‑bit code
gdt_end:

gdt_descriptor:
    dw gdt_end - gdt_start - 1
    dd gdt_start            ; org 0x10000 → physical matches linear

; --------------------------------------------------------
; Paging structures - FIXED: PDPT has entry 510 for higher-half
; --------------------------------------------------------
align 4096
pml4:
    ; Entry 0: Identity mapping
    dq pdpt + 3
    
    ; Entries 1-510: Unused
    times 510 dq 0
    
    ; Entry 511: Higher-half mapping
    dq pdpt + 3

align 4096
pdpt:
    ; Entry 0: Identity mapping (maps 0x0000000000000000)
    dq pd + 3
    
    ; Entries 1-509: Unused
    times 509 dq 0
    
    ; Entry 510: Higher-half mapping (maps 0xFFFFFFFF80000000)
    dq pd + 3
    
    ; Entry 511: Unused
    dq 0

align 4096
pd:
    %assign i 0
    %rep 256
        dq (i * 0x200000) + 0x83
        %assign i i+1
    %endrep
    times 256 dq 0

; --------------------------------------------------------
; BootInfo struct
; --------------------------------------------------------
bootinfo:
    dq 0                      ; memory_map_addr
    dq 0                      ; memory_map_count
    dq 0                      ; framebuffer_addr
    dd 0                      ; framebuffer_width
    dd 0                      ; framebuffer_height
    dd 0                      ; framebuffer_pitch
    dd 0                      ; framebuffer_bpp
    dq 0                      ; kernel_phys_start
    dq 0                      ; kernel_phys_end
    dq 0                      ; pml4_addr

; --------------------------------------------------------
; E820 buffer + count
; --------------------------------------------------------
e820_buffer:
    times 64*24 db 0          ; 64 entries × 24 bytes

e820_count:
    dw 0
    dd 0                      ; padding to keep alignment tidy
