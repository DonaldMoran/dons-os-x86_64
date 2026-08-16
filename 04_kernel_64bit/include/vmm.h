#pragma once
#include <stdint.h>

#define HHDM_START 0xFFFF800000000000ULL
#define PAGE_SIZE 4096

#define PT_PRESENT  0x001
#define PT_WRITE    0x002
#define PT_USER     0x004
#define PT_EXEC     0x008
#define PT_HUGE     0x080
#define PT_NX       0x8000000000000000ULL

//~ #define USER_STACK_BASE 0x7FFFFFE00000ULL
//~ #define USER_CODE_BASE  0x400000ULL

//~ #define USER_CODE_BASE  0x400000
//~ #define USER_STACK_BASE 0x7FFFFFE00000
#define USER_CODE_BASE  0x400000ULL
#define USER_STACK_BASE 0x7FFFFFE00000ULL

void vmm_init(void);
void vmm_map_page(uint64_t virt, uint64_t phys, uint64_t flags);
void vmm_unmap_page(uint64_t virt);
uint64_t vmm_get_phys(uint64_t virt);
int vmm_is_mapped(uint64_t virt);
void vmm_dump_page_table(uint64_t virt);
