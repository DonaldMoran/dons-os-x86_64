; syscall.asm - SYSCALL/SYSRET handler
; Uses MSRs to set up the syscall entry point

section .text
global syscall_handler
global syscall_init
extern syscall_dispatcher

; The syscall handler - this is where user code jumps via SYSCALL
; When SYSCALL is executed:
; - RCX = RIP (return address)
; - R11 = RFLAGS (saved)
; - CPL changes from 3 to 0
; - Stack: user RSP is in RSP, but we need to switch to kernel stack
syscall_handler:
    ; Save user registers
    ; We need to preserve everything since the syscall could affect them
    push rbp
    mov rbp, rsp
    
    ; Save all registers that might be clobbered
    push rax
    push rbx
    push rcx
    push rdx
    push rsi
    push rdi
    push r8
    push r9
    push r10
    push r11
    push r12
    push r13
    push r14
    push r15
    
    ; Set up kernel stack (use current stack)
    ; We're already on kernel stack if syscall was properly set up
    
    ; Call the C dispatcher with syscall number in rax
    ; Arguments: rdi, rsi, rdx, r10, r8, r9 (x86_64 calling convention)
    ; Note: syscall uses r10 for 4th arg, but C uses rcx, so we need to move
    ; System call number is in rax
    mov rdi, rax        ; syscall number
    mov rsi, rbx        ; arg1 (user-space, but we already saved)
    mov rdx, rcx        ; arg2
    ; For 4th arg, syscall uses r10, but C expects rcx
    ; We need to restore from saved registers
    mov rcx, r10        ; 4th arg from user (moved to rcx for C)
    ; 5th arg in r8, 6th in r9 - already correct for C
    
    ; Actually, syscall arguments are in rdi, rsi, rdx, r10, r8, r9
    ; We need to call syscall_dispatcher(syscall_num, arg1, arg2, arg3, arg4, arg5)
    ; Our saved registers: rax=num, rbx=arg1, rcx=arg2, rdx=arg3, rsi=arg4, rdi=arg5
    ; Wait, we saved them in order. Let's just load from the saved stack
    
    ; Better approach: the syscall handler receives args in the standard syscall ABI
    ; syscall number in rax, args in rdi, rsi, rdx, r10, r8, r9
    ; But we saved rdi, rsi, etc. We need to restore them
    ; Actually, on entry to syscall_handler, the user args are in:
    ; rax = syscall number
    ; rdi = arg1
    ; rsi = arg2
    ; rdx = arg3
    ; r10 = arg4
    ; r8 = arg5
    ; r9 = arg6
    
    ; Since we saved them, let's just use the current values
    ; But we need to move r10 to rcx for C calling convention (4th arg)
    mov rcx, r10
    
    ; Now call the dispatcher
    call syscall_dispatcher
    
    ; Return value is in rax
    ; Restore all registers except rax (which holds return value)
    ; But we need to be careful: we saved the original rax, which is the syscall number
    ; We need to keep the return value in rax
    
    ; Restore saved registers (except rax)
    pop r15
    pop r14
    pop r13
    pop r12
    pop r11
    pop r10
    pop r9
    pop r8
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop rbx
    ; Don't pop rax - keep return value
    ; But we need to pop the saved rax without losing the return value
    ; Let's handle this differently
    
    ; Actually, it's simpler to use a different approach
    ; Just preserve rax as the return value
    ; The saved rax is on the stack, we need to skip it
    add rsp, 8          ; Skip saved rax
    ; pop rbx           ; Already popped earlier
    
    ; pop rbp
    pop rbp
    
    ; Return to user mode
    ; SYSRET will restore RCX to RIP and R11 to RFLAGS
    o64 sysret

; Initialize syscall MSRs
syscall_init:
    ; IA32_STAR (0xC0000081) - SYSCALL/SYSRET segment selectors
    ; Bits 0-15: SYSCALL CS (ring 0)
    ; Bits 16-31: SYSRET CS (ring 3)
    ; For 64-bit: CS = 0x08 (kernel), SS = 0x10
    ; SYSCALL uses: CS = STAR[0:15], SS = STAR[0:15]+8
    ; SYSRET uses: CS = STAR[16:31], SS = STAR[16:31]+8
    mov ecx, 0xC0000081
    ; Set kernel CS = 0x08, user CS = 0x1B (0x18 + 3)
    mov eax, 0x001B0008  ; Lower 32 bits
    mov edx, 0x00000000  ; Upper 32 bits
    wrmsr
    
    ; IA32_LSTAR (0xC0000082) - SYSCALL entry point
    mov ecx, 0xC0000082
    mov rax, syscall_handler
    wrmsr
    
    ; IA32_FMASK (0xC0000084) - RFLAGS mask
    ; Clear certain flags when entering kernel (e.g., DF)
    mov ecx, 0xC0000084
    mov eax, 0x00000000
    mov edx, 0x00000000
    wrmsr
    
    ret

; Note: We need to use the correct syscall ABI
; The above handler needs to be revised for proper register handling
; Here's a cleaner version:

section .text
global syscall_handler_clean
syscall_handler_clean:
    ; Save user state
    ; We'll use a different approach: swapgs for GS base
    
    ; Save all registers that could be clobbered
    push rbp
    mov rbp, rsp
    
    ; We need to save ALL registers because user space expects them preserved
    push rax
    push rbx
    push rcx
    push rdx
    push rsi
    push rdi
    push r8
    push r9
    push r10
    push r11
    push r12
    push r13
    push r14
    push r15
    
    ; Now call dispatcher with the arguments
    ; We have: rax=syscall#, rdi=arg1, rsi=arg2, rdx=arg3, r10=arg4, r8=arg5, r9=arg6
    ; Move r10 to rcx for C calling convention
    mov rcx, r10
    
    ; Call dispatcher
    call syscall_dispatcher
    
    ; Store return value in the saved rax location
    mov [rbp + 8], rax   ; Replace saved rax with return value
    
    ; Restore all registers (rax will be overwritten with saved value, which is now return)
    pop r15
    pop r14
    pop r13
    pop r12
    pop r11
    pop r10
    pop r9
    pop r8
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop rbx
    pop rax              ; This will be the return value from dispatcher
    pop rbp
    
    ; Return to user mode
    o64 sysret
