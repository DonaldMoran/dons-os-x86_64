#ifndef RING3_H
#define RING3_H

#include <stdint.h>

#define USER_CODE_BASE  0x4000000000
#define USER_STACK_BASE 0x400000000000

// Function prototypes
void ring3_enter(uint64_t entry, uint64_t stack, uint64_t arg1, uint64_t arg2);
void create_user_process(void (*entry)(void*), void* arg);
void user_test(void* arg);
void user_test2(void* arg);

// Add this to ring3.h
void simple_user_test(void);

#endif // RING3_H
