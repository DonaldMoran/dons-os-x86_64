#ifndef TSS_H
#define TSS_H

#include <stdint.h>

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

// Function prototypes
void tss_init(void);
void tss_set_kernel_stack(uint64_t stack);

#endif // TSS_H
