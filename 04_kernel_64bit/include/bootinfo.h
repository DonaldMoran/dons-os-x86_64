#ifndef BOOTINFO_H
#define BOOTINFO_H

#include <stdint.h>

#define BOOTINFO_MAGIC 0x4F534F444E4F53ULL  // "DONSOS" in ASCII
#define BOOTINFO_VERSION 1

typedef struct {
    uint64_t base;
    uint64_t length;
    uint32_t type;
    uint32_t reserved;
} __attribute__((packed)) MemoryMapEntry;

typedef struct {
    // Magic for validation
    uint64_t magic;                  // 0x00: BOOTINFO_MAGIC
    uint64_t version;                // 0x08: BOOTINFO_VERSION
    
    // Memory map (from BIOS E820)
    uint64_t memory_map_addr;        // 0x10: Physical address of E820 map
    uint64_t memory_map_count;       // 0x18: Number of E820 entries
    
    // Kernel locations
    uint64_t kernel_phys_start;      // 0x20: Physical start of kernel
    uint64_t kernel_phys_end;        // 0x28: Physical end of kernel
    
    // Paging
    uint64_t pml4_addr;              // 0x30: Physical address of PML4
    uint64_t pml4_virt;              // 0x38: Virtual address of PML4 (recursive mapping)
    
    // Framebuffer (0 if not available)
    uint64_t framebuffer_addr;       // 0x40: Physical address of framebuffer
    uint32_t framebuffer_width;      // 0x48: Screen width in pixels
    uint32_t framebuffer_height;     // 0x4C: Screen height in pixels
    uint32_t framebuffer_pitch;      // 0x50: Bytes per scanline
    uint32_t framebuffer_bpp;        // 0x54: Bits per pixel
    
    // Boot device
    uint64_t boot_drive;             // 0x58: BIOS drive number
    
    // ACPI (0 if not available)
    uint64_t acpi_rsdp;              // 0x60: ACPI RSDP physical address
    uint64_t smbios_addr;            // 0x68: SMBIOS table physical address
    
    // Command line (0 if none)
    uint64_t cmdline;                // 0x70: Physical address of command line
    uint64_t cmdline_len;            // 0x78: Length of command line
    
    // Runtime flags
    uint64_t flags;                  // 0x80: Various boot flags
    
    // Reserved for future expansion
    uint64_t reserved[6];            // 0x88 - 0xBF: Reserved
} BootInfo;
// Total size: 0xC0 (192 bytes)

#endif

//~ #ifndef BOOTINFO_H
//~ #define BOOTINFO_H
//~ #include "stdint.h"

//~ typedef struct {
    //~ uint64_t base;
    //~ uint64_t length;
    //~ uint32_t type;
    //~ uint32_t reserved;
//~ } MemoryMapEntry;

//~ typedef struct {
    //~ uint64_t memory_map_addr;
    //~ uint64_t memory_map_count;

    //~ uint64_t framebuffer_addr;
    //~ uint32_t framebuffer_width;
    //~ uint32_t framebuffer_height;
    //~ uint32_t framebuffer_pitch;
    //~ uint32_t framebuffer_bpp;

    //~ uint64_t kernel_phys_start;
    //~ uint64_t kernel_phys_end;

    //~ uint64_t pml4_addr;
    //~ uint64_t pml4_virt;          // Virtual address of PML4 via recursive mapping
//~ } BootInfo;

//~ #endif
