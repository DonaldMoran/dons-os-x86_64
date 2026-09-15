[bits 64]
[section .text]
[global _start]

_start:
    ; We are running in higher-half (0xFFFFFFFF80000000).
    ; Set up a temporary stack that is BELOW the kernel .bss, so that
    ; zeroing .bss does not clobber the stack we are currently using.
    mov rsp, 0xFFFFFFFF80090000

    ; Preserve the BootInfo pointer that stage2 passed in rdi across
    ; the .bss zeroing (rep stosq clobbers rdi, rcx, rax).
    mov r12, rdi

    ; Zero the kernel's .bss: [__bss_start, _kernel_end).
    ;
    ; The .bss section is NOBITS and is not present in kernel.bin, so
    ; whatever the bootloader left in physical memory at those addresses
    ; would otherwise be interpreted as initialized kernel globals.
    ; That is the source of the nondeterministic user-mode failures we
    ; were chasing: kernel globals read -1 out of stale boot memory.
    ;
    ; Both symbols are page-aligned by the linker script, so the size
    ; is a multiple of 8 and rep stosq is safe.
    extern __bss_start
    extern _kernel_end
    mov rdi, __bss_start
    mov rcx, _kernel_end
    sub rcx, rdi
    shr rcx, 3
    xor eax, eax
    rep stosq

    ; Clear the remaining caller-saved GPRs the C ABI does not define.
    xor rax, rax
    xor rbx, rbx
    xor rcx, rcx
    xor rdx, rdx

    ; Restore the BootInfo pointer as the first argument to kmain.
    mov rdi, r12

    extern kmain
    call kmain

hang:
    jmp hang
