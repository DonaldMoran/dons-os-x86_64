#pragma once
#include <stdint.h>

#define HHDM_START 0xFFFF800000000000
#define PAGE_SIZE 4096

#define PT_PRESENT  0x001
#define PT_WRITE    0x002
#define PT_USER     0x004
#define PT_HUGE     0x080

void vmm_init(void);
void vmm_map_page(uint64_t virt, uint64_t phys, uint64_t flags);
void vmm_unmap_page(uint64_t virt);
uint64_t vmm_get_phys(uint64_t virt);
int vmm_is_mapped(uint64_t virt);
