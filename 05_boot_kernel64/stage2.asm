[bits 16]
[org 0x10000]

; ============================================
; Constants
; ============================================
BOOTINFO_MAGIC equ 0x4F534F444E4F53
BOOTINFO_VERSION equ 1

; ============================================
; DAP entries - at the beginning for fixed offsets
; ============================================

dap_kernel:
    db 16
    db 0
    dw 128
    dw 0x0000
    dw 0x8000
    dq 64

; ============================================
; Real Mode Code
; ============================================

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

    ; Load kernel
    mov si, dap_kernel
    mov dl, 0x80
    mov ah, 0x42
    int 0x13
    jc disk_error

    ; Get memory map
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

disk_error:
    mov ax, 0xB800
    mov ds, ax
    mov byte [0], 'E'
    mov byte [1], 0x0C
    hlt
    jmp disk_error

; ============================================
; 32-bit Protected Mode
; ============================================
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

; ============================================
; 64-bit Long Mode
; ============================================
[bits 64]
long_mode_entry:
    mov rsp, 0x80000

    ; ============================================
    ; Fill BootInfo structure
    ; ============================================
    mov rbx, bootinfo
    
    ; Fill memory map info
    mov qword [rbx + 0x10], e820_buffer    ; memory_map_addr
    movzx rax, word [e820_count]
    mov qword [rbx + 0x18], rax            ; memory_map_count
    
    ; Note: kernel_phys_start (0x20) and kernel_phys_end (0x28) 
    ; are already filled in the bootinfo data section
    
    ; Note: pml4_addr (0x30) is already filled in the bootinfo data section
    ; Note: pml4_virt (0x38) is already filled in the bootinfo data section
    
    ; Note: boot_drive (0x58) is already filled in the bootinfo data section
    
    ; Set flags
    mov qword [rbx + 0x80], 0x01           ; flags: bit 0 = booted from HDD

    ; ============================================
    ; Copy kernel from 0x80000 to 0x100000
    ; ============================================
    mov rsi, 0x00080000
    mov rdi, 0x00100000
    mov rcx, 8192
    rep movsq

    ; Jump to kernel
    mov rdi, bootinfo
    mov rax, 0xFFFFFFFF80100000
    jmp rax

; ============================================
; GDT
; ============================================
gdt_start:
    dq 0x0000000000000000
    db 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x9A, 0xCF, 0x00
    db 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x92, 0xCF, 0x00
    db 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x9A, 0xAF, 0x00
    db 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x92, 0xAF, 0x00
    db 0xFF, 0xFF, 0x00, 0x00, 0x00, 0xFA, 0xAF, 0x00
    db 0xFF, 0xFF, 0x00, 0x00, 0x00, 0xF2, 0xAF, 0x00
    dq 0x0000000000000000
    dq 0x0000000000000000
gdt_end:

gdt_descriptor:
    dw gdt_end - gdt_start - 1
    dq gdt_start

; ============================================
; Page Tables - ORIGINAL + HHDM mapping
; ============================================

align 4096
pml4:
    ; PML4[0] -> Identity mapping for low memory
    dq pdpt_identity + 3
    
    ; PML4[1] to PML4[255] = 0
    times 255 dq 0
    
    ; PML4[256] -> HHDM mapping (for kernel VMM)
    dq pdpt_hhdm + 3
    
    ; PML4[257] to PML4[509] = 0
    times 253 dq 0
    
    ; PML4[510] -> Recursive mapping
    dq pml4 + 0x003
    
    ; PML4[511] -> Higher-half kernel mapping
    dq pdpt_higher + 3

align 4096
pdpt_identity:
    dq pd + 3
    times 511 dq 0

align 4096
pdpt_hhdm:
    ; HHDM: Identity map the same physical memory
    ; Maps physical memory at HHDM_START (0xFFFF800000000000)
    dq pd_hhdm + 3
    times 511 dq 0

align 4096
pdpt_higher:
    times 509 dq 0
    dq pml4 + 0x003
    dq pd + 3
    dq 0

align 4096
pd:
    %assign i 0
    %rep 512
        dq (i * 0x200000) + 0x83
        %assign i i+1
    %endrep
    times (512 - 512) dq 0

; HHDM page directory - maps the same physical memory
align 4096
pd_hhdm:
    %assign i 0
    %rep 512
        dq (i * 0x200000) + 0x83
        %assign i i+1
    %endrep
    times (512 - 512) dq 0

; ============================================
; BootInfo Structure (must match bootinfo.h)
; ============================================
; typedef struct BootInfo {
;     uint64_t magic;              ; 0x00
;     uint64_t version;            ; 0x08
;     uint64_t memory_map_addr;    ; 0x10
;     uint64_t memory_map_count;   ; 0x18
;     uint64_t kernel_phys_start;  ; 0x20
;     uint64_t kernel_phys_end;    ; 0x28
;     uint64_t pml4_addr;          ; 0x30
;     uint64_t pml4_virt;          ; 0x38
;     uint64_t framebuffer_addr;   ; 0x40
;     uint32_t framebuffer_width;  ; 0x48
;     uint32_t framebuffer_height; ; 0x4C
;     uint32_t framebuffer_pitch;  ; 0x50
;     uint32_t framebuffer_bpp;    ; 0x54
;     uint64_t boot_drive;         ; 0x58
;     uint64_t acpi_rsdp;          ; 0x60
;     uint64_t smbios_addr;        ; 0x68
;     uint64_t cmdline;            ; 0x70
;     uint64_t cmdline_len;        ; 0x78
;     uint64_t flags;              ; 0x80
;     uint64_t reserved[6];        ; 0x88 - 0xBF
; } BootInfo;
; Size: 0xC0 (192 bytes)

align 16
bootinfo:
    dq BOOTINFO_MAGIC       ; 0x00: magic
    dq BOOTINFO_VERSION     ; 0x08: version
    dq e820_buffer          ; 0x10: memory_map_addr
    dq 0                    ; 0x18: memory_map_count (filled in long_mode_entry)
    dq 0x00100000           ; 0x20: kernel_phys_start
    dq 0x00100000 + (128*512) ; 0x28: kernel_phys_end (64 sectors = 128*512 bytes)
    dq pml4                 ; 0x30: pml4_addr
    dq 0xFFFFFF7FBFDFE000   ; 0x38: pml4_virt (recursive mapping address)
    dq 0                    ; 0x40: framebuffer_addr (none yet)
    dd 0                    ; 0x48: framebuffer_width
    dd 0                    ; 0x4C: framebuffer_height
    dd 0                    ; 0x50: framebuffer_pitch
    dd 0                    ; 0x54: framebuffer_bpp
    dq 0x80                 ; 0x58: boot_drive (0x80 = first HDD)
    dq 0                    ; 0x60: acpi_rsdp
    dq 0                    ; 0x68: smbios_addr
    dq 0                    ; 0x70: cmdline
    dq 0                    ; 0x78: cmdline_len
    dq 0                    ; 0x80: flags (filled in long_mode_entry)
    times 6 dq 0            ; 0x88 - 0xBF: reserved

; ============================================
; Data Structures
; ============================================

e820_buffer:
    times 64*24 db 0

e820_count:
    dw 0
    dd 0
