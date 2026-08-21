#ifndef RING3_H
#define RING3_H

#include <stdint.h>

/*
 * User address space layout
 *
 * USER_CODE_BASE  : base virtual address for user programs
 * USER_STACK_BASE : base virtual address for user stack (grows downward)
 */

#define USER_CODE_BASE   0x0000008000000000ULL
#define USER_STACK_BASE  0x000000007FFFFFE00000ULL

uint64_t ring3_enter(uint64_t entry, uint64_t stack,
                     uint64_t arg1, uint64_t arg2);

void create_user_process(void (*entry)(void*), void* arg);
void user_test(void* arg);
void user_test2(void* arg);

#endif
