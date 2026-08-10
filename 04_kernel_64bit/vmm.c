#include <stdint.h>
#include <stddef.h>
#include "include/vmm.h"
#include "include/pmm.h"
#include "include/vga.h"
#include "include/serial.h"

#define PML4_RECURSIVE_INDEX 510

static uint64_t* get_pml4_virt(void) {
    uint64_t pml4_virt = 0xFFFF800000000000ULL | 
                         ((uint64_t)510 << 39) | 
                         ((uint64_t)510 << 30) | 
                         ((uint64_t)510 << 21) | 
                         ((uint64_t)510 << 12);
    return (uint64_t*)pml4_virt;
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
    
    uint64_t* pml4 = get_pml4_virt();
    
    vga_print("VMM: PML4 virt = 0x");
    vga_print_hex_cur((uint64_t)pml4);
    vga_print("\n");
    serial_print("VMM: PML4 virt = 0x");
    serial_print_hex((uint64_t)pml4);
    serial_print("\n");
    
    vga_print("VMM: PML4[0] = 0x");
    vga_print_hex_cur(pml4[0]);
    vga_print("\n");
    serial_print("VMM: PML4[0] = 0x");
    serial_print_hex(pml4[0]);
    serial_print("\n");
    
    vga_print("VMM: PML4[510] = 0x");
    vga_print_hex_cur(pml4[510]);
    vga_print("\n");
    serial_print("VMM: PML4[510] = 0x");
    serial_print_hex(pml4[510]);
    serial_print("\n");
    
    vga_print("VMM: Using identity mapping for page tables\n");
    serial_print("VMM: Using identity mapping for page tables\n");
    vga_print("VMM: Initialization complete\n");
    serial_print("VMM: Initialization complete\n");
}

void vmm_map_page(uint64_t virt, uint64_t phys, uint64_t flags) {
    serial_print("MAP: Mapping 0x");
    serial_print_hex(virt);
    serial_print(" -> 0x");
    serial_print_hex(phys);
    serial_print("\n");
    
    uint64_t* pml4 = get_pml4_virt();
    
    uint32_t pml4_idx = (virt >> 39) & 0x1FF;
    uint32_t pdpt_idx = (virt >> 30) & 0x1FF;
    uint32_t pd_idx = (virt >> 21) & 0x1FF;
    uint32_t pt_idx = (virt >> 12) & 0x1FF;
    
    serial_print("MAP: indices: ");
    serial_print_dec(pml4_idx);
    serial_print(",");
    serial_print_dec(pdpt_idx);
    serial_print(",");
    serial_print_dec(pd_idx);
    serial_print(",");
    serial_print_dec(pt_idx);
    serial_print("\n");
    
    // Use identity mapping for page tables
    uint64_t* pdpt;
    if (!(pml4[pml4_idx] & 0x01)) {
        uint64_t pdpt_phys = pmm_alloc_page();
        if (!pdpt_phys) {
            serial_print("MAP: Failed to allocate PDPT!\n");
            return;
        }
        pml4[pml4_idx] = pdpt_phys | flags | 0x01;
        serial_print("MAP: PDPT phys = 0x");
        serial_print_hex(pdpt_phys);
        serial_print("\n");
        // Identity mapping: physical address = virtual address
        pdpt = (uint64_t*)pdpt_phys;
        serial_print("MAP: PDPT virt = 0x");
        serial_print_hex((uint64_t)pdpt);
        serial_print("\n");
        // SKIP ZEROING - trust PMM
        serial_print("MAP: PDPT allocated (not zeroed)\n");
    } else {
        pdpt = (uint64_t*)(pml4[pml4_idx] & ~0xFFF);
    }
    
    // Get or allocate PD using identity mapping
    uint64_t* pd;
    if (!(pdpt[pdpt_idx] & 0x01)) {
        uint64_t pd_phys = pmm_alloc_page();
        if (!pd_phys) {
            serial_print("MAP: Failed to allocate PD!\n");
            return;
        }
        pdpt[pdpt_idx] = pd_phys | flags | 0x01;
        serial_print("MAP: PD phys = 0x");
        serial_print_hex(pd_phys);
        serial_print("\n");
        pd = (uint64_t*)pd_phys;
        serial_print("MAP: PD virt = 0x");
        serial_print_hex((uint64_t)pd);
        serial_print("\n");
        serial_print("MAP: PD allocated (not zeroed)\n");
    } else {
        pd = (uint64_t*)(pdpt[pdpt_idx] & ~0xFFF);
    }
    
    // Get or allocate PT using identity mapping
    uint64_t* pt;
    if (!(pd[pd_idx] & 0x01)) {
        uint64_t pt_phys = pmm_alloc_page();
        if (!pt_phys) {
            serial_print("MAP: Failed to allocate PT!\n");
            return;
        }
        pd[pd_idx] = pt_phys | flags | 0x01;
        serial_print("MAP: PT phys = 0x");
        serial_print_hex(pt_phys);
        serial_print("\n");
        pt = (uint64_t*)pt_phys;
        serial_print("MAP: PT virt = 0x");
        serial_print_hex((uint64_t)pt);
        serial_print("\n");
        serial_print("MAP: PT allocated (not zeroed)\n");
    } else {
        pt = (uint64_t*)(pd[pd_idx] & ~0xFFF);
    }
    
    // Map the page
    pt[pt_idx] = phys | flags | 0x01;
    serial_print("MAP: Page mapped successfully!\n");
    
    // Flush TLB
    asm volatile("invlpg (%0)" : : "r" (virt) : "memory");
}

void vmm_unmap_page(uint64_t virt) { (void)virt; }
uint64_t vmm_get_phys(uint64_t virt) { (void)virt; return 0; }
int vmm_is_mapped(uint64_t virt) { (void)virt; return 0; }
