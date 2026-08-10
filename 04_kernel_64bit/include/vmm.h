#ifndef VMM_H
#define VMM_H

#include <stdint.h>

#define VMM_PRESENT     0x001
#define VMM_WRITE       0x002
#define VMM_USER        0x004

// HHDM (Higher Half Direct Map)
#define HHDM_START      0xFFFF800000000000

void vmm_init(void);
void vmm_map_page(uint64_t virt, uint64_t phys, uint64_t flags);
void vmm_unmap_page(uint64_t virt);
uint64_t vmm_get_phys(uint64_t virt);
int vmm_is_mapped(uint64_t virt);

#endif
