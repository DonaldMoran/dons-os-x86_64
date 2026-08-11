// vmm.c - Force full TLB flush
#include <stdint.h>
#include <stddef.h>
#include "include/vmm.h"
#include "include/pmm.h"
#include "include/vga.h"
#include "include/serial.h"

static uint64_t* get_pml4_virt(void) {
    return (uint64_t*)0xFFFFFF7FBFDFE000ULL;
}

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
    
    // Clear any NX bit from the physical address and flags
    phys = phys & ~0x8000000000000000ULL;
    flags = flags & ~0x8000000000000000ULL;
    
    // Determine if this is a user page
    // For user pages, ALL levels must have the USER bit set!
    uint64_t user_flag = (flags & PT_USER) ? PT_USER : 0;
    uint64_t write_flag = (flags & PT_WRITE) ? PT_WRITE : 0;
    uint64_t exec_flag = (flags & PT_EXEC) ? PT_EXEC : 0;
    uint64_t present_flag = PT_PRESENT;
    
    uint64_t* pml4 = get_pml4_virt();
    
    uint32_t pml4_idx = (virt >> 39) & 0x1FF;
    uint32_t pdpt_idx = (virt >> 30) & 0x1FF;
    uint32_t pd_idx = (virt >> 21) & 0x1FF;
    uint32_t pt_idx = (virt >> 12) & 0x1FF;
    
    // Allocate PDPT - MUST set USER bit for user pages!
    uint64_t* pdpt;
    if (!(pml4[pml4_idx] & 0x01)) {
        uint64_t pdpt_phys = pmm_alloc_page();
        if (!pdpt_phys) {
            serial_print("VMM: Failed to allocate PDPT!\n");
            return;
        }
        // CRITICAL: Set USER bit in PML4 entry!
        pml4[pml4_idx] = pdpt_phys | present_flag | write_flag | user_flag;
        serial_print("VMM: Allocated PDPT at 0x");
        serial_print_hex(pdpt_phys);
        serial_print(" with USER bit\n");
        pdpt = (uint64_t*)pdpt_phys;
    } else {
        // CRITICAL: If PML4 entry exists, ensure it has USER bit for user pages!
        if (user_flag) {
            pml4[pml4_idx] |= PT_USER;
            serial_print("VMM: Added USER bit to existing PML4 entry\n");
        }
        pdpt = (uint64_t*)(pml4[pml4_idx] & ~0xFFF);
    }
    
    // Allocate PD - MUST set USER bit for user pages!
    uint64_t* pd;
    if (!(pdpt[pdpt_idx] & 0x01)) {
        uint64_t pd_phys = pmm_alloc_page();
        if (!pd_phys) {
            serial_print("VMM: Failed to allocate PD!\n");
            return;
        }
        // CRITICAL: Set USER bit in PDPT entry!
        pdpt[pdpt_idx] = pd_phys | present_flag | write_flag | user_flag;
        serial_print("VMM: Allocated PD at 0x");
        serial_print_hex(pd_phys);
        serial_print(" with USER bit\n");
        pd = (uint64_t*)pd_phys;
    } else {
        // CRITICAL: If PDPT entry exists, ensure it has USER bit for user pages!
        if (user_flag) {
            pdpt[pdpt_idx] |= PT_USER;
            serial_print("VMM: Added USER bit to existing PDPT entry\n");
        }
        pd = (uint64_t*)(pdpt[pdpt_idx] & ~0xFFF);
    }
    
    // Allocate PT - MUST set USER bit for user pages!
    uint64_t* pt;
    if (!(pd[pd_idx] & 0x01)) {
        uint64_t pt_phys = pmm_alloc_page();
        if (!pt_phys) {
            serial_print("VMM: Failed to allocate PT!\n");
            return;
        }
        // CRITICAL: Set USER bit in PD entry!
        pd[pd_idx] = pt_phys | present_flag | write_flag | user_flag;
        serial_print("VMM: Allocated PT at 0x");
        serial_print_hex(pt_phys);
        serial_print(" with USER bit\n");
        pt = (uint64_t*)pt_phys;
    } else {
        // CRITICAL: If PD entry exists, ensure it has USER bit for user pages!
        if (user_flag) {
            pd[pd_idx] |= PT_USER;
            serial_print("VMM: Added USER bit to existing PD entry\n");
        }
        pt = (uint64_t*)(pd[pd_idx] & ~0xFFF);
    }
    
    // Map the page
    uint64_t pte = phys | flags;
    // Explicitly clear the NX bit (bit 63) to make the page executable
    pte = pte & ~0x8000000000000000ULL;
    
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
    
    // Check PML4 entry
    if (!(pml4[pml4_idx] & 0x01)) {
        return 0;
    }
    uint64_t pdpt_phys = pml4[pml4_idx] & ~0xFFF;
    uint64_t* pdpt = (uint64_t*)pdpt_phys;
    
    // Check PDPT entry
    if (!(pdpt[pdpt_idx] & 0x01)) {
        return 0;
    }
    uint64_t pd_phys = pdpt[pdpt_idx] & ~0xFFF;
    uint64_t* pd = (uint64_t*)pd_phys;
    
    // Check PD entry
    if (!(pd[pd_idx] & 0x01)) {
        return 0;
    }
    uint64_t pt_phys = pd[pd_idx] & ~0xFFF;
    uint64_t* pt = (uint64_t*)pt_phys;
    
    // Check PT entry
    if (!(pt[pt_idx] & 0x01)) {
        return 0;
    }
    
    return (pt[pt_idx] & ~0xFFF) | (virt & 0xFFF);
}

int vmm_is_mapped(uint64_t virt) {
    return vmm_get_phys(virt) != 0;
}

// Debug function to dump all page table entries for a given virtual address
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
    serial_print("    USER: ");
    serial_print_hex((pml4[pml4_idx] >> 2) & 0x1);
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
    serial_print("    USER: ");
    serial_print_hex((pdpt[pdpt_idx] >> 2) & 0x1);
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
    serial_print("    USER: ");
    serial_print_hex((pd[pd_idx] >> 2) & 0x1);
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
    serial_print("    USER: ");
    serial_print_hex((pt[pt_idx] >> 2) & 0x1);
    serial_print("\n");
    serial_print("    PRESENT: ");
    serial_print_hex(pt[pt_idx] & 0x1);
    serial_print("\n");
}
