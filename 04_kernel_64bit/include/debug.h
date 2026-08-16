#ifndef DEBUG_H
#define DEBUG_H

#include <stdint.h>
#include "serial.h"

static inline void debug_rsp(const char* msg) {
    uint64_t rsp;
    __asm__ volatile ("mov %%rsp, %0" : "=r"(rsp));
    serial_print(msg);
    serial_print(" RSP=0x");
    serial_print_hex(rsp);
    serial_print("\n");
}

// Function prototype for IRETQ frame dumping
void dump_iretq_frame_serial(void);

#endif
