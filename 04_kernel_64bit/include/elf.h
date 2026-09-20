#ifndef ELF_H
#define ELF_H

#include <stdint.h>
#include "process.h"

// ELF64 header
typedef struct {
    uint8_t  e_ident[16];    // Magic number and other info
    uint16_t e_type;         // Object file type
    uint16_t e_machine;      // Architecture
    uint32_t e_version;      // Object file version
    uint64_t e_entry;        // Entry point virtual address
    uint64_t e_phoff;        // Program header table file offset
    uint64_t e_shoff;        // Section header table file offset
    uint32_t e_flags;        // Processor-specific flags
    uint16_t e_ehsize;       // ELF header size in bytes
    uint16_t e_phentsize;    // Program header table entry size
    uint16_t e_phnum;        // Program header table entry count
    uint16_t e_shentsize;    // Section header table entry size
    uint16_t e_shnum;        // Section header table entry count
    uint16_t e_shstrndx;     // Section header table string table index
} __attribute__((packed)) Elf64_Ehdr;

// ELF64 program header
typedef struct {
    uint32_t p_type;         // Segment type
    uint32_t p_flags;        // Segment flags
    uint64_t p_offset;       // Segment file offset
    uint64_t p_vaddr;        // Segment virtual address
    uint64_t p_paddr;        // Segment physical address
    uint64_t p_filesz;       // Segment size in file
    uint64_t p_memsz;        // Segment size in memory
    uint64_t p_align;        // Segment alignment
} __attribute__((packed)) Elf64_Phdr;

// ELF magic numbers
#define ELF_MAGIC0 0x7F
#define ELF_MAGIC1 'E'
#define ELF_MAGIC2 'L'
#define ELF_MAGIC3 'F'

// Program header types
#define PT_LOAD 1

// ELF flags (for p_flags)
#define PF_X 1   // Execute
#define PF_W 2   // Write
#define PF_R 4   // Read

// Function prototypes
void elf_load(const void* elf_data);
void elf_add_page_to_pcb(pcb_t* pcb, uint64_t phys);

/*
 * Load an ELF64 image that is already resident in kernel memory into
 * the address space of `pcb`.  Allocates and maps every page the
 * LOAD segments need, copies file bytes in, zeroes the bss tail, and
 * returns the ELF's entry point (0 on failure).
 *
 * The caller must have already created `pcb` with process_create and
 * removed it from the ready queue; the caller must re-add it after
 * setting pcb->entry_point to the returned value.
 */
uint64_t elf_load_into_process(pcb_t* pcb, const void* elf_data);

#endif
