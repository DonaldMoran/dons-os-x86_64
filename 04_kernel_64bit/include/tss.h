#ifndef TSS_H
#define TSS_H

#include <stdint.h>

extern uint64_t kernel_stack[];
extern uint64_t *kernel_stack_top;


// TSS structure (64-bit format)
typedef struct {
    uint32_t reserved0;
    uint64_t rsp0;
    uint64_t rsp1;
    uint64_t rsp2;
    uint64_t reserved1;
    uint64_t ist1;
    uint64_t ist2;
    uint64_t ist3;
    uint64_t ist4;
    uint64_t ist5;
    uint64_t ist6;
    uint64_t ist7;
    uint64_t reserved2;
    uint16_t reserved3;
    uint16_t iopb_base;
} __attribute__((packed)) tss_t;

// I/O Permission Bitmap size (65536 ports = 8192 bytes)
#define TSS_IOMAP_SIZE 8192
// Total TSS size including I/O bitmap + terminator
#define TSS_TOTAL_SIZE (sizeof(tss_t) + TSS_IOMAP_SIZE + 1)

// TSS physical address
#define TSS_PHYS_ADDR 0x5000

// Kernel stack top used by user_syscall_entry.asm for the duration of a
// syscall. Symmetric with tss->rsp0, which is what the CPU loads on a
// ring-3 -> ring-0 transition for interrupts. Both must point at the same
// per-process kernel stack, or a timer that fires during a syscall would
// build a second live frame on a different stack.
extern uint64_t g_syscall_stack_top;

// Function prototypes
void tss_init(void);
void tss_set_kernel_stack(uint64_t stack);
void tss_set_syscall_stack(uint64_t stack);
void tss_dump(void);

#endif // TSS_H
