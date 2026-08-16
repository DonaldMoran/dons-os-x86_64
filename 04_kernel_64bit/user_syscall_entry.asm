[bits 64]
default rel

section .text
global user_syscall_entry
extern syscall_dispatch

user_syscall_entry:
     push rbp
     mov rbp, rsp
     
     ; Linux syscall ABI on entry:
     ;   rax = num
     ;   rdi = arg0 (fd)
     ;   rsi = arg1 (buf)
     ;   rdx = arg2 (count)
     ;   r10 = arg3
     ;   r8  = arg4
     ;   r9  = arg5
     
     ; Save original args in temps
     mov r11, rdi        ; save fd
     mov r12, rsi        ; save buf
     mov r13, rdx        ; save count
     
     ; System V C ABI for syscall_dispatch:
     ;   rdi = num
     ;   rsi = arg0
     ;   rdx = arg1
     ;   rcx = arg2
     ;   r8  = arg3
     ;   r9  = arg4
     
     mov rdi, rax        ; num
     mov rsi, r11        ; arg0 = fd
     mov rdx, r12        ; arg1 = buf
     mov rcx, r13        ; arg2 = count
     mov r8,  r10        ; arg3
     ; r9 already holds arg5
     
     call syscall_dispatch
     
     pop rbp
     
     ; Just halt instead of trying to return
     cli
     hlt
