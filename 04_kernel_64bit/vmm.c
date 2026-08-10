#include <stdint.h>
#include <stddef.h>
#include "include/vmm.h"
#include "include/pmm.h"
#include "include/vga.h"

// Simple stub - just prints
void vmm_init(void) {
    // vga_print("VMM: Initialized (HHDM ready)\n");
}

void vmm_map_page(uint64_t virt, uint64_t phys, uint64_t flags) {
    (void)virt; (void)phys; (void)flags;
}

void vmm_unmap_page(uint64_t virt) {
    (void)virt;
}

uint64_t vmm_get_phys(uint64_t virt) {
    (void)virt;
    return 0;
}

int vmm_is_mapped(uint64_t virt) {
    (void)virt;
    return 0;
}
