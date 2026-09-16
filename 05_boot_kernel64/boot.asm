[bits 16]
[org 0x7C00]

start:
    ; ----------------------------------------------------
    ; Breadcrumb: Boot sector is running
    ; ----------------------------------------------------
    mov ax, 0xB800
    mov ds, ax
    mov byte [0], 'B'
    mov byte [1], 0x07

    ; ----------------------------------------------------
    ; Set DS back to boot sector, ES to stage2 destination
    ; ----------------------------------------------------
    mov ax, 0x0000       ; boot sector segment
    mov ds, ax           ; DAP lives here

    mov ax, 0x1000       ; stage2 destination segment
    mov es, ax           ; BIOS will use this from DAP

    ; ----------------------------------------------------
    ; Load stage2 (128 sectors = 64 KB) from LBA 1 -> 0x1000:0000
    ;
    ; Was 64 sectors (32 KB).  Raised to 128 sectors to give
    ; stage2 room for the shared 4 KB page table (pt_low) plus
    ; any future boot-time tables.  The kernel now starts at
    ; LBA 128, so stage2 has LBA 1..127 available (63.5 KB).
    ; ----------------------------------------------------
    mov si, dap_stage2   ; DS:SI -> DAP in boot sector
    mov dl, 0x80         ; first hard disk
    mov ah, 0x42         ; extended read (LBA)
    int 0x13
    jc disk_error

    ; ----------------------------------------------------
    ; Jump to stage2 at 0x1000:0000
    ; ----------------------------------------------------
    jmp 0x1000:0

disk_error:
    mov ax, 0xB800
    mov ds, ax
    mov byte [2], 'E'
    mov byte [3], 0x0C
    hlt
    jmp disk_error

dap_stage2:
    db 16                 ; size of DAP
    db 0                  ; reserved
    dw 128                ; number of sectors to read (was 64)
    dw 0x0000             ; offset
    dw 0x1000             ; segment (0x1000:0000 = 0x00010000)
    dq 1                  ; starting LBA (stage2 at LBA 1)

times 510 - ($ - $$) db 0
dw 0xAA55
