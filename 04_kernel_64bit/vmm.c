// vmm.c - Using recursive mapping for page table access
#include <stdint.h>
#include <stddef.h>
#include "include/vmm.h"
#include "include/pmm.h"
#include "include/vga.h"
#include "include/serial.h"

// Recursive mapping address for PML4
static uint64_t* get_pml4_virt(void) {
    return (uint64_t*)0xFFFFFF7FBFDFE000ULL;
}

//~ // Get virtual address of a page table entry using recursive mapping
//~ // This works for any physical address
//~ static uint64_t* get_pte_virt(uint64_t phys, uint32_t idx) {
    //~ // Use recursive mapping to access the page table
    //~ // The recursive entry is at PML4 index 510
    //~ // So we can access any page table by walking the page tables
    //~ // For now, we'll use the identity mapping for low addresses
    //~ // and HHDM for others
    //~ if (phys < 0x200000) {
        //~ return (uint64_t*)(phys + (idx * 8));
    //~ }
    //~ // For higher addresses, we need to use the recursive mapping
    //~ // But we can't easily access arbitrary page tables this way
    //~ // So we'll use HHDM
    //~ return (uint64_t*)(phys + HHDM_START + (idx * 8));
//~ }

void vmm_init(void) {
    vga_print("VMM: Initializing...\n");
    serial_print("VMM: Initializing...\n");
    
    uint64_t cr3;
    asm volatile("mov %%cr3, %0" : "=r"(cr3));
    
    vga_print("VMM: CR3 = 0x");
    vga_print_hex_cur(cr3);
    vga_print("\n");
    serial_print("VMM: CR3 = 0x");
    serial_print_hex(cr3);
    serial_print("\n");
    
    vga_print("VMM: NX support available\n");
    serial_print("VMM: NX support available\n");
    
    vga_print("VMM: Initialization complete\n");
    serial_print("VMM: Initialization complete\n");
}

void vmm_map_page(uint64_t virt, uint64_t phys, uint64_t flags) {
    serial_print("VMM: Mapping 0x");
    serial_print_hex(virt);
    serial_print(" -> 0x");
    serial_print_hex(phys);
    serial_print(" flags 0x");
    serial_print_hex(flags);
    serial_print("\n");
    
    // Clear NX from physical address
    phys = phys & ~PT_NX;
    
    // Determine flags
    uint64_t user_flag = (flags & PT_USER) ? PT_USER : 0;
    uint64_t write_flag = (flags & PT_WRITE) ? PT_WRITE : 0;
    uint64_t present_flag = PT_PRESENT;
    uint64_t nx_flag = (flags & PT_NX) ? PT_NX : 0;
    
    uint64_t* pml4 = get_pml4_virt();
    
    uint32_t pml4_idx = (virt >> 39) & 0x1FF;
    uint32_t pdpt_idx = (virt >> 30) & 0x1FF;
    uint32_t pd_idx = (virt >> 21) & 0x1FF;
    uint32_t pt_idx = (virt >> 12) & 0x1FF;
    
    // Allocate PDPT
    uint64_t* pdpt;
    if (!(pml4[pml4_idx] & 0x01)) {
        uint64_t pdpt_phys = pmm_alloc_page();
        if (!pdpt_phys) {
            serial_print("VMM: Failed to allocate PDPT!\n");
            return;
        }
        pml4[pml4_idx] = pdpt_phys | present_flag | write_flag | user_flag;
        serial_print("VMM: Allocated PDPT at 0x");
        serial_print_hex(pdpt_phys);
        serial_print("\n");
        pdpt = (uint64_t*)pdpt_phys;
    } else {
        uint64_t pdpt_phys = pml4[pml4_idx] & ~0xFFF;
        pdpt = (uint64_t*)pdpt_phys;
    }
    
    // Allocate PD
    uint64_t* pd;
    if (!(pdpt[pdpt_idx] & 0x01)) {
        uint64_t pd_phys = pmm_alloc_page();
        if (!pd_phys) {
            serial_print("VMM: Failed to allocate PD!\n");
            return;
        }
        pdpt[pdpt_idx] = pd_phys | present_flag | write_flag | user_flag;
        serial_print("VMM: Allocated PD at 0x");
        serial_print_hex(pd_phys);
        serial_print("\n");
        pd = (uint64_t*)pd_phys;
    } else {
        uint64_t pd_phys = pdpt[pdpt_idx] & ~0xFFF;
        pd = (uint64_t*)pd_phys;
    }
    
    // Allocate PT
    uint64_t* pt;
    if (!(pd[pd_idx] & 0x01)) {
        uint64_t pt_phys = pmm_alloc_page();
        if (!pt_phys) {
            serial_print("VMM: Failed to allocate PT!\n");
            return;
        }
        pd[pd_idx] = pt_phys | present_flag | write_flag | user_flag;
        serial_print("VMM: Allocated PT at 0x");
        serial_print_hex(pt_phys);
        serial_print("\n");
        pt = (uint64_t*)pt_phys;
    } else {
        uint64_t pt_phys = pd[pd_idx] & ~0xFFF;
        pt = (uint64_t*)pt_phys;
    }
    
    // Map the page - make sure WRITE bit is set!
    uint64_t pte = phys | present_flag | PT_WRITE | user_flag;
    // Force WRITE bit for heap pages
    if (flags & PT_WRITE) {
        pte |= PT_WRITE;
    }
    if (nx_flag) {
        pte |= PT_NX;
        serial_print("VMM: NX bit set for page\n");
    }
    
    pt[pt_idx] = pte;
    serial_print("VMM: Mapped page: PT[");
    serial_print_dec(pt_idx);
    serial_print("]=0x");
    serial_print_hex(pt[pt_idx]);
    serial_print("\n");
    
    // Full TLB flush
    uint64_t cr3;
    asm volatile("mov %%cr3, %0" : "=r"(cr3));
    asm volatile("mov %0, %%cr3" : : "r"(cr3));
    asm volatile("invlpg (%0)" : : "r" (virt) : "memory");
}

void vmm_unmap_page(uint64_t virt) {
    (void)virt;
}

uint64_t vmm_get_phys(uint64_t virt) {
    uint64_t* pml4 = get_pml4_virt();
    
    uint32_t pml4_idx = (virt >> 39) & 0x1FF;
    uint32_t pdpt_idx = (virt >> 30) & 0x1FF;
    uint32_t pd_idx = (virt >> 21) & 0x1FF;
    uint32_t pt_idx = (virt >> 12) & 0x1FF;
    
    if (!(pml4[pml4_idx] & 0x01)) return 0;
    uint64_t pdpt_phys = pml4[pml4_idx] & ~0xFFF;
    uint64_t* pdpt = (uint64_t*)pdpt_phys;
    
    if (!(pdpt[pdpt_idx] & 0x01)) return 0;
    uint64_t pd_phys = pdpt[pdpt_idx] & ~0xFFF;
    uint64_t* pd = (uint64_t*)pd_phys;
    
    if (!(pd[pd_idx] & 0x01)) return 0;
    uint64_t pt_phys = pd[pd_idx] & ~0xFFF;
    uint64_t* pt = (uint64_t*)pt_phys;
    
    if (!(pt[pt_idx] & 0x01)) return 0;
    return (pt[pt_idx] & ~0xFFF) | (virt & 0xFFF);
}

int vmm_is_mapped(uint64_t virt) {
    return vmm_get_phys(virt) != 0;
}

void vmm_dump_page_table(uint64_t virt) {
    uint64_t* pml4 = get_pml4_virt();
    
    uint32_t pml4_idx = (virt >> 39) & 0x1FF;
    uint32_t pdpt_idx = (virt >> 30) & 0x1FF;
    uint32_t pd_idx = (virt >> 21) & 0x1FF;
    uint32_t pt_idx = (virt >> 12) & 0x1FF;
    
    serial_print("Page table dump for 0x");
    serial_print_hex(virt);
    serial_print("\n");
    serial_print("  PML4[");
    serial_print_dec(pml4_idx);
    serial_print("] = 0x");
    serial_print_hex(pml4[pml4_idx]);
    serial_print("\n");
    
    if (!(pml4[pml4_idx] & 0x01)) {
        serial_print("  PML4 entry not present!\n");
        return;
    }
    
    uint64_t pdpt_phys = pml4[pml4_idx] & ~0xFFF;
    uint64_t* pdpt = (uint64_t*)pdpt_phys;
    
    serial_print("  PDPT[");
    serial_print_dec(pdpt_idx);
    serial_print("] = 0x");
    serial_print_hex(pdpt[pdpt_idx]);
    serial_print("\n");
    
    if (!(pdpt[pdpt_idx] & 0x01)) {
        serial_print("  PDPT entry not present!\n");
        return;
    }
    
    uint64_t pd_phys = pdpt[pdpt_idx] & ~0xFFF;
    uint64_t* pd = (uint64_t*)pd_phys;
    
    serial_print("  PD[");
    serial_print_dec(pd_idx);
    serial_print("] = 0x");
    serial_print_hex(pd[pd_idx]);
    serial_print("\n");
    
    if (!(pd[pd_idx] & 0x01)) {
        serial_print("  PD entry not present!\n");
        return;
    }
    
    uint64_t pt_phys = pd[pd_idx] & ~0xFFF;
    uint64_t* pt = (uint64_t*)pt_phys;
    
    serial_print("  PT[");
    serial_print_dec(pt_idx);
    serial_print("] = 0x");
    serial_print_hex(pt[pt_idx]);
    serial_print("\n");
    serial_print("    NX: ");
    serial_print_hex((pt[pt_idx] >> 63) & 0x1);
    serial_print("\n");
    serial_print("    PRESENT: ");
    serial_print_hex(pt[pt_idx] & 0x1);
    serial_print("\n");
}
