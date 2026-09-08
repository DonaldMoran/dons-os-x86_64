// user_syscall.h
#ifndef USER_SYSCALL_H
#define USER_SYSCALL_H

#include <stdint.h>

// Function prototypes
uint64_t syscall_dispatch(uint64_t num, uint64_t arg0, uint64_t arg1, 
                          uint64_t arg2, uint64_t arg3, 
                          uint64_t arg4, uint64_t arg5);
void syscall_init(void);

#endif
