[bits 16]
[org 0x10000]
default abs   ; suppress NASM warning about implicit DEFAULT ABS

; ============================================
; Constants
; ============================================
BOOTINFO_MAGIC equ 0x4F534F444E4F53
BOOTINFO_VERSION equ 1

; ============================================
; Kernel staging and destination layout
; --------------------------------------------
;   Staging   : 0x80000 .. 0xB7FFF  (192 KB, three 64 KB passes)
;                 - 0x80000 .. 0x9FFFF  PASS 1+2 (usable RAM)
;                 - 0xA0000 .. 0xAFFFF  PASS 3 (VGA graphics memory,
;                   unused in text mode 03h)
;   Destination: 0x100000 .. 0x12FFFF (192 KB, copied in long mode)
;
; The staging region must stop below 0xB8000, which is where the
; VGA text-mode framebuffer lives. A fourth pass starting at
; 0xB000:0000 would clobber the screen.
; ============================================
KERNEL_SECTORS_PER_PASS equ 128
KERNEL_PASSES           equ 3
KERNEL_TOTAL_SECTORS    equ KERNEL_SECTORS_PER_PASS * KERNEL_PASSES
KERNEL_TOTAL_BYTES      equ KERNEL_TOTAL_SECTORS * 512      ; 192 KB
KERNEL_TOTAL_QWORDS     equ KERNEL_TOTAL_BYTES / 8          ; 24576

; ============================================
; DAP entries - at the beginning for fixed offsets
; Configured for isolated 128-sector passes to prevent segment wraps
; ============================================
dap_kernel:
    db 16
    db 0
    dw 128            ; Read exactly 128 sectors (64 KB) per pass
    dw 0x0000         ; Safe destination offset coord
.segment:
    dw 0x8000         ; Dynamically incremented! (Starts at 0x8000)
.lba:
    dd 64             ; Lower 32-bits of starting LBA (Kernel starts at sector 64)
    dd 0              ; Upper 32-bits of starting LBA

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

    ; --- Set VGA text mode 03h (MikeOS-style) ---
    mov ah, 0x00
    mov al, 0x03
    int 0x10
    ; -------------------------------------------

    in  al, 0x92
    or  al, 00000010b
    out 0x92, al

    ; ----------------------------------------------------
    ; PASS 1: Load first 128 sectors (64 KB) to 0x8000:0000
    ; ----------------------------------------------------
    mov si, dap_kernel
    mov dl, 0x80
    mov ah, 0x42
    int 0x13
    jc disk_error

    ; ----------------------------------------------------
    ; THE TRICK: Advance your segment and LBA coordinates!
    ; We shift the segment forward by 0x1000 to target
    ; the next 64 KB block, preventing segment wrap-around!
    ; We use dword to keep the 16-bit compiler happy.
    ; ----------------------------------------------------
    add word [dap_kernel.segment], 0x1000  ; Next target segment pointer: 0x9000
    add dword [dap_kernel.lba], 128        ; Next target disk sector coordinate: 64 + 128 = 192

    ; ----------------------------------------------------
    ; PASS 2: Load next 128 sectors (64 KB) to 0x9000:0000
    ; ----------------------------------------------------
    mov si, dap_kernel
    mov dl, 0x80
    mov ah, 0x42
    int 0x13
    jc disk_error

    ; ----------------------------------------------------
    ; Advance to the third and final 64 KB block. Segment
    ; 0xA000 lands at physical 0xA0000..0xAFFFF, which is
    ; VGA graphics memory on real hardware but is unused in
    ; text mode 03h (the text framebuffer is at 0xB8000).
    ; ----------------------------------------------------
    add word [dap_kernel.segment], 0x1000  ; Next target segment pointer: 0xA000
    add dword [dap_kernel.lba], 128        ; Next target disk sector coordinate: 192 + 128 = 320

    ; ----------------------------------------------------
    ; PASS 3: Load final 128 sectors (64 KB) to 0xA000:0000
    ; Total loaded: 384 sectors = 192 KB at 0x80000..0xB7FFF
    ; ----------------------------------------------------
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
    mov es, ax
    xor di, di              ; ES:DI points to the very first char cell on screen
    mov byte [es:di], 'E'   ; Write 'E' character to row 0, col 0
    inc di                  ; Move to attribute byte color cell
    mov byte [es:di], 0x0C  ; Write bright red error color
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

    ; Fill BootInfo structure
    mov rbx, bootinfo
    
    mov qword [rbx + 0x10], e820_buffer    ; memory_map_addr
    movzx rax, word [e820_count]
    mov qword [rbx + 0x18], rax            ; memory_map_count

    ; ============================================
    ; Copy kernel from staging (0x80000) to destination (0x100000)
    ;
    ; 192 KB total = 24576 quadwords. The staging region is
    ; 0x80000..0xB7FFF and the destination is 0x100000..0x12FFFF,
    ; so they do not overlap; the rep movsq is a straight copy.
    ;
    ; If you raise KERNEL_TOTAL_QWORDS above the value that keeps
    ; staging below 0xB8000, you will overwrite the VGA text
    ; framebuffer. See the layout comment near the top of the file.
    ; ============================================
    mov rsi, 0x00080000
    mov rdi, 0x00100000
    mov rcx, KERNEL_TOTAL_QWORDS
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
    dq pdpt_identity + 3
    times 255 dq 0
    dq pdpt_hhdm + 3
    times 253 dq 0
    dq pml4 + 0x003
    dq pdpt_higher + 3

align 4096
pdpt_identity:
    dq pd + 3
    times 511 dq 0

align 4096
pdpt_hhdm:
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
align 16
bootinfo:
    dq BOOTINFO_MAGIC       ; 0x00: magic
    dq BOOTINFO_VERSION     ; 0x08: version
    dq e820_buffer          ; 0x10: memory_map_addr
    dq 0                    ; 0x18: memory_map_count
    dq 0x00100000           ; 0x20: kernel_phys_start
    dq 0x00100000 + KERNEL_TOTAL_BYTES ; 0x28: kernel_phys_end (192 KB reserved)
    dq pml4                 ; 0x30: pml4_addr
    dq 0xFFFFFF7FBFDFE000   ; 0x38: pml4_virt
    dq 0                    ; 0x40: framebuffer_addr
    dd 0                    ; 0x48: framebuffer_width
    dd 0                    ; 0x4C: framebuffer_height
    dd 0                    ; 0x50: framebuffer_pitch
    dd 0                    ; 0x54: framebuffer_bpp
    dq 0x80                 ; 0x58: boot_drive
    dq 0                    ; 0x60: acpi_rsdp
    dq 0                    ; 0x68: smbios_addr
    dq 0                    ; 0x70: cmdline
    dq 0                    ; 0x78: cmdline_len
    dq 0                    ; 0x80: flags
    times 6 dq 0            ; 0x88 - 0xBF: reserved

; ============================================
; Data Structures
; ============================================
e820_buffer:
    times 64*24 db 0

e820_count:
    dw 0
    dd 0
