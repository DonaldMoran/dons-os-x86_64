#ifndef RING3_H
#define RING3_H

#include <stdint.h>

// Use ULL suffix to force 64-bit literals
#define USER_CODE_BASE  0x400000ULL
#define USER_STACK_BASE 0x7FFFFFE00000ULL

//~ void ring3_enter(uint64_t entry, uint64_t stack, uint64_t arg1, uint64_t arg2);
uint64_t ring3_enter(uint64_t entry, uint64_t stack, uint64_t arg1, uint64_t arg2);
void create_user_process(void (*entry)(void*), void* arg);
void user_test(void* arg);
void user_test2(void* arg);

#endif
