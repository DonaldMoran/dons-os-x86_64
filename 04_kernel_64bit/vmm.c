// vmm.c - Using recursive mapping for page table access
#include <stdint.h>
#include <stddef.h>
#include "include/vmm.h"
#include "include/pmm.h"
#include "include/vga.h"
#include "include/serial.h"

#define RECURSIVE_PML4_INDEX 510
#define HHDM_START 0xFFFF800000000000ULL

// Recursive mapping address for PML4
static uint64_t* get_pml4_virt(void) {
    return (uint64_t*)(HHDM_START | 
                       ((uint64_t)RECURSIVE_PML4_INDEX << 39) |
                       ((uint64_t)RECURSIVE_PML4_INDEX << 30) |
                       ((uint64_t)RECURSIVE_PML4_INDEX << 21) |
                       ((uint64_t)RECURSIVE_PML4_INDEX << 12));
}

// Convert physical to virtual using HHDM
static inline uint64_t* phys_to_virt(uint64_t phys) {
    return (uint64_t*)(HHDM_START + phys);
}

void vmm_init(void) {
    vga_print("VMM: Initializing...\n");
    serial_print("VMM: Initializing...\n");
    
    // Identity-map low memory so TSS at 0x5000 is accessible
    for (uint64_t addr = 0; addr < 0x200000; addr += 0x1000) {
        vmm_map_page(addr, addr, PT_PRESENT | PT_WRITE | PT_EXEC);
    }
    
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
    phys = phys & ~PT_NX;
    
    uint64_t user_flag   = (flags & PT_USER) ? PT_USER : 0;
    uint64_t write_flag  = (flags & PT_WRITE) ? PT_WRITE : 0;
    uint64_t present_flag= PT_PRESENT;
    uint64_t nx_flag     = (flags & PT_NX) ? PT_NX : 0;
    uint64_t exec_flag   = (flags & PT_EXEC) ? PT_EXEC : 0;
    
    uint64_t* pml4 = get_pml4_virt();
    
    uint32_t pml4_idx = (virt >> 39) & 0x1FF;
    uint32_t pdpt_idx = (virt >> 30) & 0x1FF;
    uint32_t pd_idx   = (virt >> 21) & 0x1FF;
    uint32_t pt_idx   = (virt >> 12) & 0x1FF;
    
    // Allocate/access PDPT
    uint64_t* pdpt;
    if (!(pml4[pml4_idx] & 0x01)) {
        uint64_t new_pdpt_phys = pmm_alloc_page();
        if (!new_pdpt_phys) {
            serial_print("VMM: Failed to allocate PDPT!\n");
            return;
        }
        pml4[pml4_idx] = new_pdpt_phys | present_flag | write_flag | user_flag;
        if (flags & PT_USER) pml4[pml4_idx] |= exec_flag;
        serial_print("VMM: Allocated PDPT at 0x");
        serial_print_hex(new_pdpt_phys);
        serial_print("\n");
        pdpt = phys_to_virt(new_pdpt_phys);
    } else {
        uint64_t existing_pdpt_phys = pml4[pml4_idx] & ~0xFFF;
        pml4[pml4_idx] = existing_pdpt_phys | present_flag | write_flag | user_flag;
        if (flags & PT_USER) pml4[pml4_idx] |= exec_flag;
        pdpt = phys_to_virt(existing_pdpt_phys);
    }
    
    // Allocate/access PD
    uint64_t* pd;
    if (!(pdpt[pdpt_idx] & 0x01)) {
        uint64_t new_pd_phys = pmm_alloc_page();
        if (!new_pd_phys) {
            serial_print("VMM: Failed to allocate PD!\n");
            return;
        }
        pdpt[pdpt_idx] = new_pd_phys | present_flag | write_flag | user_flag;
        if (flags & PT_USER) pdpt[pdpt_idx] |= exec_flag;
        serial_print("VMM: Allocated PD at 0x");
        serial_print_hex(new_pd_phys);
        serial_print("\n");
        pd = phys_to_virt(new_pd_phys);
    } else {
        uint64_t existing_pd_phys = pdpt[pdpt_idx] & ~0xFFF;
        pdpt[pdpt_idx] = existing_pd_phys | present_flag | write_flag | user_flag;
        if (flags & PT_USER) pdpt[pdpt_idx] |= exec_flag;
        pd = phys_to_virt(existing_pd_phys);
    }
    
    // Allocate/access PT
    uint64_t* pt;
    if (flags & PT_USER) {
        uint64_t new_pt_phys = pmm_alloc_page();
        if (!new_pt_phys) {
            serial_print("VMM: Failed to allocate PT for user page!\n");
            return;
        }
        pd[pd_idx] = new_pt_phys | present_flag | write_flag | user_flag;
        if (flags & PT_USER) pd[pd_idx] |= exec_flag;
        serial_print("VMM: Allocated NEW PT for user page at 0x");
        serial_print_hex(new_pt_phys);
        serial_print("\n");
        pt = phys_to_virt(new_pt_phys);
    } else {
        if (!(pd[pd_idx] & 0x01)) {
            uint64_t new_pt_phys = pmm_alloc_page();
            if (!new_pt_phys) {
                serial_print("VMM: Failed to allocate PT!\n");
                return;
            }
            pd[pd_idx] = new_pt_phys | present_flag | write_flag | user_flag;
            serial_print("VMM: Allocated PT at 0x");
            serial_print_hex(new_pt_phys);
            serial_print("\n");
            pt = phys_to_virt(new_pt_phys);
        } else {
            uint64_t existing_pt_phys = pd[pd_idx] & ~0xFFF;
            pt = phys_to_virt(existing_pt_phys);
        }
    }
    
    uint64_t pte = phys | present_flag | write_flag | user_flag;
    if (exec_flag) pte |= exec_flag;
    if (nx_flag) {
        pte |= PT_NX;
        serial_print("VMM: NX bit set for page\n");
    }
    
    pt[pt_idx] = pte;
    
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
    uint32_t pd_idx   = (virt >> 21) & 0x1FF;
    uint32_t pt_idx   = (virt >> 12) & 0x1FF;
    
    if (!(pml4[pml4_idx] & 0x01)) return 0;
    uint64_t pdpt_phys = pml4[pml4_idx] & ~0xFFF;
    uint64_t* pdpt = phys_to_virt(pdpt_phys);
    
    if (!(pdpt[pdpt_idx] & 0x01)) return 0;
    uint64_t pd_phys = pdpt[pdpt_idx] & ~0xFFF;
    uint64_t* pd = phys_to_virt(pd_phys);
    
    if (!(pd[pd_idx] & 0x01)) return 0;
    uint64_t pt_phys = pd[pd_idx] & ~0xFFF;
    uint64_t* pt = phys_to_virt(pt_phys);
    
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
    uint32_t pd_idx   = (virt >> 21) & 0x1FF;
    uint32_t pt_idx   = (virt >> 12) & 0x1FF;
    
    serial_print("\n=== PAGE TABLE DUMP for 0x");
    serial_print_hex(virt);
    serial_print(" ===\n");
    serial_print("  Indices: PML4=");
    serial_print_dec(pml4_idx);
    serial_print(" PDPT=");
    serial_print_dec(pdpt_idx);
    serial_print(" PD=");
    serial_print_dec(pd_idx);
    serial_print(" PT=");
    serial_print_dec(pt_idx);
    serial_print("\n");
    
    serial_print("  PML4[");
    serial_print_dec(pml4_idx);
    serial_print("] = 0x");
    serial_print_hex(pml4[pml4_idx]);
    serial_print("\n");
    
    if (!(pml4[pml4_idx] & 0x01)) {
        serial_print("  PML4 entry NOT PRESENT!\n");
        serial_print("=== END DUMP ===\n\n");
        return;
    }
    serial_print("    Present: YES\n");
    serial_print("    Flags: 0x");
    serial_print_hex(pml4[pml4_idx] & 0xFFF);
    serial_print("\n");
    
    uint64_t pdpt_phys = pml4[pml4_idx] & ~0xFFF;
    uint64_t* pdpt = phys_to_virt(pdpt_phys);
    
    serial_print("  PDPT[");
    serial_print_dec(pdpt_idx);
    serial_print("] = 0x");
    serial_print_hex(pdpt[pdpt_idx]);
    serial_print("\n");
    
    if (!(pdpt[pdpt_idx] & 0x01)) {
        serial_print("  PDPT entry NOT PRESENT!\n");
        serial_print("=== END DUMP ===\n\n");
        return;
    }
    serial_print("    Present: YES\n");
    serial_print("    Flags: 0x");
    serial_print_hex(pdpt[pdpt_idx] & 0xFFF);
    serial_print("\n");
    
    uint64_t pd_phys = pdpt[pdpt_idx] & ~0xFFF;
    uint64_t* pd = phys_to_virt(pd_phys);
    
    serial_print("  PD[");
    serial_print_dec(pd_idx);
    serial_print("] = 0x");
    serial_print_hex(pd[pd_idx]);
    serial_print("\n");
    
    if (!(pd[pd_idx] & 0x01)) {
        serial_print("  PD entry NOT PRESENT!\n");
        serial_print("=== END DUMP ===\n\n");
        return;
    }
    serial_print("    Present: YES\n");
    serial_print("    Flags: 0x");
    serial_print_hex(pd[pd_idx] & 0xFFF);
    serial_print("\n");
    
    uint64_t pt_phys = pd[pd_idx] & ~0xFFF;
    uint64_t* pt = phys_to_virt(pt_phys);
    
    serial_print("  PT[");
    serial_print_dec(pt_idx);
    serial_print("] = 0x");
    serial_print_hex(pt[pt_idx]);
    serial_print("\n");
    
    if (!(pt[pt_idx] & 0x01)) {
        serial_print("  PT entry NOT PRESENT!\n");
        serial_print("=== END DUMP ===\n\n");
        return;
    }
    serial_print("    Present: YES\n");
    serial_print("    Physical address: 0x");
    serial_print_hex(pt[pt_idx] & ~0xFFF);
    serial_print("\n");
    serial_print("    Flags: 0x");
    serial_print_hex(pt[pt_idx] & 0xFFF);
    serial_print("\n");
    
    if (pt[pt_idx] & PT_NX) {
        serial_print("    NX bit: SET\n");
    } else {
        serial_print("    NX bit: CLEAR\n");
    }
    
    serial_print("=== END DUMP ===\n\n");
}
