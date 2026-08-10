#ifndef BOOTINFO_H
#define BOOTINFO_H
#include "stdint.h"

typedef struct {
    uint64_t base;
    uint64_t length;
    uint32_t type;
    uint32_t reserved;
} MemoryMapEntry;

typedef struct {
    uint64_t memory_map_addr;
    uint64_t memory_map_count;

    uint64_t framebuffer_addr;
    uint32_t framebuffer_width;
    uint32_t framebuffer_height;
    uint32_t framebuffer_pitch;
    uint32_t framebuffer_bpp;

    uint64_t kernel_phys_start;
    uint64_t kernel_phys_end;

    uint64_t pml4_addr;
    uint64_t pml4_virt;          // Virtual address of PML4 via recursive mapping
} BootInfo;

#endif
