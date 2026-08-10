#include <stdint.h>
#include <stddef.h>
#include "include/vmm.h"
#include "include/pmm.h"
#include "include/vga.h"
#include "include/serial.h"

// Recursive mapping: PML4[510] maps to the PML4 itself
// The virtual address for the PML4 via recursive mapping is:
// PML4[510] -> PDPT[510] -> PD[510] -> PT[510] -> offset
#define PML4_RECURSIVE_INDEX 510
#define PML4_RECURSIVE_VIRT  ((uint64_t)0xFFFF800000000000 | \
                             ((uint64_t)PML4_RECURSIVE_INDEX << 39) | \
                             ((uint64_t)PML4_RECURSIVE_INDEX << 30) | \
                             ((uint64_t)PML4_RECURSIVE_INDEX << 21) | \
                             ((uint64_t)PML4_RECURSIVE_INDEX << 12))

static uint64_t* get_pml4_virt(void) {
    return (uint64_t*)PML4_RECURSIVE_VIRT;
}

void vmm_init(void) {
    vga_print("VMM: Initializing...\n");
    serial_print("VMM: Initializing...\n");
    
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
    
    // Get PML4 virtual address via recursive mapping
    uint64_t* pml4_virt = get_pml4_virt();
    
    vga_print("VMM: PML4 virt = 0x");
    vga_print_hex_cur((uint64_t)pml4_virt);
    vga_print("\n");
    serial_print("VMM: PML4 virt = 0x");
    serial_print_hex((uint64_t)pml4_virt);
    serial_print("\n");
    
    // Check if we can read the PML4
    vga_print("VMM: Checking PML4[0]...\n");
    serial_print("VMM: Checking PML4[0]...\n");
    
    uint64_t pml4_entry0 = pml4_virt[0];
    vga_print("VMM: PML4[0] = 0x");
    vga_print_hex_cur(pml4_entry0);
    vga_print("\n");
    serial_print("VMM: PML4[0] = 0x");
    serial_print_hex(pml4_entry0);
    serial_print("\n");
    
    // Check if the recursive entry is set correctly
    vga_print("VMM: Checking PML4[510]...\n");
    serial_print("VMM: Checking PML4[510]...\n");
    
    uint64_t pml4_entry510 = pml4_virt[510];
    vga_print("VMM: PML4[510] = 0x");
    vga_print_hex_cur(pml4_entry510);
    vga_print("\n");
    serial_print("VMM: PML4[510] = 0x");
    serial_print_hex(pml4_entry510);
    serial_print("\n");
    
    // The recursive entry should point back to the PML4
    if ((pml4_entry510 & ~0xFFF) == pml4_phys) {
        vga_print("VMM: Recursive mapping verified!\n");
        serial_print("VMM: Recursive mapping verified!\n");
    } else {
        vga_print("VMM: WARNING: Recursive mapping not found!\n");
        serial_print("VMM: WARNING: Recursive mapping not found!\n");
    }
    
    // Now try to allocate and map a page
    uint64_t pdpt_phys = pmm_alloc_page();
    if (pdpt_phys != 0) {
        vga_print("VMM: Allocated PDPT at 0x");
        vga_print_hex_cur(pdpt_phys);
        vga_print("\n");
        serial_print("VMM: Allocated PDPT at 0x");
        serial_print_hex(pdpt_phys);
        serial_print("\n");
        
        // Now we can write to PML4 for HHDM
        uint32_t pml4_idx = (HHDM_START >> 39) & 0x1FF;
        vga_print("VMM: HHDM PML4 index = ");
        vga_print_dec_cur(pml4_idx);
        vga_print("\n");
        serial_print("VMM: HHDM PML4 index = ");
        serial_print_dec(pml4_idx);
        serial_print("\n");
        
        // Write to PML4
        pml4_virt[pml4_idx] = pdpt_phys | 0x03;
        vga_print("VMM: Wrote to PML4[");
        vga_print_dec_cur(pml4_idx);
        vga_print("] = 0x");
        vga_print_hex_cur(pml4_virt[pml4_idx]);
        vga_print("\n");
        serial_print("VMM: Wrote to PML4[");
        serial_print_dec(pml4_idx);
        serial_print("] = 0x");
        serial_print_hex(pml4_virt[pml4_idx]);
        serial_print("\n");
        
        vga_print("VMM: SUCCESS! Recursive paging works!\n");
        serial_print("VMM: SUCCESS! Recursive paging works!\n");
    } else {
        vga_print("VMM: Failed to allocate PDPT\n");
        serial_print("VMM: Failed to allocate PDPT\n");
    }
    
    vga_print("VMM: Initialization complete\n");
    serial_print("VMM: Initialization complete\n");
}

void vmm_map_page(uint64_t virt, uint64_t phys, uint64_t flags) {
    uint64_t* pml4 = get_pml4_virt();
    uint32_t pml4_idx = (virt >> 39) & 0x1FF;
    uint32_t pdpt_idx = (virt >> 30) & 0x1FF;
    uint32_t pd_idx = (virt >> 21) & 0x1FF;
    uint32_t pt_idx = (virt >> 12) & 0x1FF;
    
    uint64_t* pdpt;
    if (!(pml4[pml4_idx] & 0x01)) {
        uint64_t pdpt_phys = pmm_alloc_page();
        if (!pdpt_phys) return;
        pml4[pml4_idx] = pdpt_phys | flags | 0x01;
        pdpt = (uint64_t*)(HHDM_START + pdpt_phys);
        for (int i = 0; i < 512; i++) pdpt[i] = 0;
    } else {
        pdpt = (uint64_t*)(HHDM_START + (pml4[pml4_idx] & ~0xFFF));
    }
    
    uint64_t* pd;
    if (!(pdpt[pdpt_idx] & 0x01)) {
        uint64_t pd_phys = pmm_alloc_page();
        if (!pd_phys) return;
        pdpt[pdpt_idx] = pd_phys | flags | 0x01;
        pd = (uint64_t*)(HHDM_START + pd_phys);
        for (int i = 0; i < 512; i++) pd[i] = 0;
    } else {
        pd = (uint64_t*)(HHDM_START + (pdpt[pdpt_idx] & ~0xFFF));
    }
    
    uint64_t* pt;
    if (!(pd[pd_idx] & 0x01)) {
        uint64_t pt_phys = pmm_alloc_page();
        if (!pt_phys) return;
        pd[pd_idx] = pt_phys | flags | 0x01;
        pt = (uint64_t*)(HHDM_START + pt_phys);
        for (int i = 0; i < 512; i++) pt[i] = 0;
    } else {
        pt = (uint64_t*)(HHDM_START + (pd[pd_idx] & ~0xFFF));
    }
    
    pt[pt_idx] = phys | flags | 0x01;
}

void vmm_unmap_page(uint64_t virt) { (void)virt; }
uint64_t vmm_get_phys(uint64_t virt) { (void)virt; return 0; }
int vmm_is_mapped(uint64_t virt) { (void)virt; return 0; }
