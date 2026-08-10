#include <stdint.h>
#include <stddef.h>
#include "include/vmm.h"
#include "include/pmm.h"
#include "include/vga.h"
#include "include/serial.h"

void vmm_init(void) {
    vga_print("VMM: Initializing HHDM...\n");
    serial_print("VMM: Initializing HHDM...\n");
    
    // Get CR3
    uint64_t cr3;
    asm volatile("mov %%cr3, %0" : "=r"(cr3));
    uint64_t pml4_phys = cr3 & ~0xFFF;
    
    vga_print("VMM: CR3 = 0x");
    vga_print_hex_cur(cr3);
    vga_print("\n");
    serial_print("VMM: CR3 = 0x");
    serial_print_hex(cr3);
    serial_print("\n");
    
    vga_print("VMM: PML4 phys = 0x");
    vga_print_hex_cur(pml4_phys);
    vga_print("\n");
    serial_print("VMM: PML4 phys = 0x");
    serial_print_hex(pml4_phys);
    serial_print("\n");
    
    // Print HHDM_START
    vga_print("VMM: HHDM_START = 0x");
    vga_print_hex_cur(HHDM_START);
    vga_print("\n");
    serial_print("VMM: HHDM_START = 0x");
    serial_print_hex(HHDM_START);
    serial_print("\n");
    
    vga_print("VMM: HHDM ready (stub)\n");
    serial_print("VMM: HHDM ready (stub)\n");
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
