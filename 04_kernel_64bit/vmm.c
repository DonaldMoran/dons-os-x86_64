// vmm.c - Using recursive mapping for page table access
#include <stdint.h>
#include <stddef.h>
#include "include/vmm.h"
#include "include/pmm.h"
#include "include/vga.h"
#include "include/serial.h"
#include "include/bootinfo.h"

#define RECURSIVE_PML4_INDEX 510
#define HHDM_START 0xFFFF800000000000ULL

// Global to track memory size
uint64_t vmm_max_physical = 0;

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

// Helper: Ensure a physical address is mapped in HHDM
void* ensure_hhdm_mapped(uint64_t phys) {
    uint64_t virt = HHDM_START + phys;
    
    // Check if the page is already mapped in the current page table
    if (!vmm_is_mapped(virt)) {
        // Map it into HHDM
        vmm_map_page(virt, phys, PT_PRESENT | PT_WRITE);
        serial_print("VMM: Dynamically mapped HHDM 0x");
        serial_print_hex(phys);
        serial_print(" -> 0x");
        serial_print_hex(virt);
        serial_print("\n");
    }
    
    return (void*)virt;
}

void vmm_init(BootInfo* info) {
    vga_print("VMM: Initializing...\n");
    serial_print("VMM: Initializing...\n");
    
    // Detect memory from BootInfo
    uint64_t max_phys = 0;
    uint64_t total_usable = 0;
    
    if (info && info->memory_map_count > 0) {
        MemoryMapEntry* entries = (MemoryMapEntry*)info->memory_map_addr;
        for (uint64_t i = 0; i < info->memory_map_count; i++) {
            if (entries[i].type == 1) { // Usable RAM
                uint64_t end = entries[i].base + entries[i].length;
                if (end > max_phys) max_phys = end;
                total_usable += entries[i].length;
            }
        }
        serial_print("VMM: Detected ");
        serial_print_dec(total_usable / (1024 * 1024));
        serial_print(" MB usable RAM\n");
        serial_print("VMM: Highest physical address: 0x");
        serial_print_hex(max_phys);
        serial_print("\n");
    } else {
        // Fallback to 128MB
        max_phys = 128ULL * 1024 * 1024;
        serial_print("VMM: No memory map, using fallback 128MB\n");
    }
    
    vmm_max_physical = max_phys;
    
    // Identity-map low memory so TSS at 0x5000 is accessible
    for (uint64_t addr = 0; addr < 0x200000; addr += 0x1000) {
        vmm_map_page(addr, addr, PT_PRESENT | PT_WRITE);
    }
    
    // Map all physical memory into HHDM
    serial_print("VMM: Mapping physical memory into HHDM...\n");
    for (uint64_t phys = 0; phys < max_phys; phys += PAGE_SIZE) {
        uint64_t virt = HHDM_START + phys;
        if (!vmm_is_mapped(virt)) {
            vmm_map_page(virt, phys, PT_PRESENT | PT_WRITE);
        }
    }
    
    serial_print("VMM: HHDM mapping complete.\n");
    
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
    phys = phys & ~0xFFFULL;
    
    uint64_t user_flag   = (flags & PT_USER) ? PT_USER : 0;
    uint64_t write_flag  = (flags & PT_WRITE) ? PT_WRITE : 0;
    uint64_t present_flag = PT_PRESENT;
    uint64_t nx_flag     = (flags & PT_NX) ? PT_NX : 0;
    uint64_t exec_flag   = (flags & PT_EXEC) ? PT_EXEC : 0;  // ← ADD THIS BACK
    
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
        if (flags & PT_USER) pml4[pml4_idx] |= exec_flag;  // ← ADDED
        serial_print("VMM: Allocated PDPT at 0x");
        serial_print_hex(new_pdpt_phys);
        serial_print("\n");
        pdpt = phys_to_virt(new_pdpt_phys);
    } else {
        uint64_t existing_pdpt_phys = pml4[pml4_idx] & ~0xFFF;
        pml4[pml4_idx] = existing_pdpt_phys | present_flag | write_flag | user_flag;
        if (flags & PT_USER) pml4[pml4_idx] |= exec_flag;  // ← ADDED
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
        if (flags & PT_USER) pdpt[pdpt_idx] |= exec_flag;  // ← ADDED
        serial_print("VMM: Allocated PD at 0x");
        serial_print_hex(new_pd_phys);
        serial_print("\n");
        pd = phys_to_virt(new_pd_phys);
    } else {
        uint64_t existing_pd_phys = pdpt[pdpt_idx] & ~0xFFF;
        pdpt[pdpt_idx] = existing_pd_phys | present_flag | write_flag | user_flag;
        if (flags & PT_USER) pdpt[pdpt_idx] |= exec_flag;  // ← ADDED
        pd = phys_to_virt(existing_pd_phys);
    }
    
    // Allocate/access PT
    uint64_t* pt;
    if (flags & PT_USER) {
        // For user pages, always allocate a new PT to avoid conflicts
        uint64_t new_pt_phys = pmm_alloc_page();
        if (!new_pt_phys) {
            serial_print("VMM: Failed to allocate PT for user page!\n");
            return;
        }
        pd[pd_idx] = new_pt_phys | present_flag | write_flag | user_flag;
        if (flags & PT_USER) pd[pd_idx] |= exec_flag;  // ← ADDED
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
    
    // Build PTE - include exec_flag (PWT bit)
    uint64_t pte = phys | present_flag | write_flag | user_flag;
    if (exec_flag) pte |= exec_flag;     // ← ADDED
    if (nx_flag) pte |= PT_NX;
    
    pt[pt_idx] = pte;
    
    // Flush TLB for this virtual address
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
        serial_print("    NX bit: CLEAR (executable)\n");
    }
    
    serial_print("=== END DUMP ===\n\n");
}

// Helper: Get PML4 from a CR3 value using HHDM
static uint64_t* get_pml4_from_cr3(uint64_t cr3) {
    return (uint64_t*)(HHDM_START + cr3);
}

uint64_t vmm_clone_page_table(uint64_t src_cr3) {
    if (src_cr3 == 0) {
        serial_print("VMM: Cannot clone NULL page table!\n");
        return 0;
    }
    
    serial_print("VMM: Cloning page table CR3=0x");
    serial_print_hex(src_cr3);
    serial_print("\n");
    
    // Allocate new PML4
    uint64_t new_pml4_phys = pmm_alloc_page();
    if (!new_pml4_phys) {
        serial_print("VMM: Failed to allocate new PML4!\n");
        return 0;
    }
    serial_print("VMM: Allocated new PML4 at 0x");
    serial_print_hex(new_pml4_phys);
    serial_print("\n");
    
    // Ensure both physical addresses are mapped in HHDM
    uint64_t* src_pml4 = (uint64_t*)ensure_hhdm_mapped(src_cr3);
    uint64_t* new_pml4 = (uint64_t*)ensure_hhdm_mapped(new_pml4_phys);
    
    // Clear the new PML4
    for (int i = 0; i < 512; i++) {
        new_pml4[i] = 0;
    }
    
    // Copy kernel entries (indices 256-511)
    for (int i = 256; i < 512; i++) {
        if (src_pml4[i] & PT_PRESENT) {
            // Skip the recursive entry - we'll set it properly
            if (i == RECURSIVE_PML4_INDEX) {
                continue;
            }
            new_pml4[i] = src_pml4[i];
            serial_print("VMM: Copied PML4[");
            serial_print_dec(i);
            serial_print("] = 0x");
            serial_print_hex(src_pml4[i]);
            serial_print("\n");
        }
    }
    
    // Set up recursive mapping for the new table
    new_pml4[RECURSIVE_PML4_INDEX] = new_pml4_phys | PT_PRESENT | PT_WRITE;
    
    serial_print("VMM: Page table cloned successfully. New CR3=0x");
    serial_print_hex(new_pml4_phys);
    serial_print("\n");
    
    return new_pml4_phys;
}

uint64_t vmm_get_phys_from_cr3(uint64_t cr3, uint64_t virt) {
    if (cr3 == 0) return 0;
    
    // Ensure the PML4 is mapped
    uint64_t* pml4 = (uint64_t*)ensure_hhdm_mapped(cr3);
    
    uint32_t pml4_idx = (virt >> 39) & 0x1FF;
    uint32_t pdpt_idx = (virt >> 30) & 0x1FF;
    uint32_t pd_idx   = (virt >> 21) & 0x1FF;
    uint32_t pt_idx   = (virt >> 12) & 0x1FF;
    
    if (!(pml4[pml4_idx] & 0x01)) return 0;
    uint64_t pdpt_phys = pml4[pml4_idx] & ~0xFFF;
    uint64_t* pdpt = (uint64_t*)ensure_hhdm_mapped(pdpt_phys);
    
    if (!(pdpt[pdpt_idx] & 0x01)) return 0;
    uint64_t pd_phys = pdpt[pdpt_idx] & ~0xFFF;
    uint64_t* pd = (uint64_t*)ensure_hhdm_mapped(pd_phys);
    
    if (!(pd[pd_idx] & 0x01)) return 0;
    uint64_t pt_phys = pd[pd_idx] & ~0xFFF;
    uint64_t* pt = (uint64_t*)ensure_hhdm_mapped(pt_phys);
    
    if (!(pt[pt_idx] & 0x01)) return 0;
    return (pt[pt_idx] & ~0xFFF) | (virt & 0xFFF);
}
