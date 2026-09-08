#ifndef USER_SPACE_H
#define USER_SPACE_H

#include <stdint.h>

/*
 * User address space layout
 */
#define USER_CODE_BASE   0x0000008000000000ULL
#define USER_STACK_BASE  0x00007FFFFFE00000ULL

#endif
