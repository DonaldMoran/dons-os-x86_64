// vmm.c - Using recursive mapping for page table access
#include <stdint.h>
#include <stddef.h>
#include "vmm.h"
#include "pmm.h"
#include "vga.h"
#include "serial.h"
#include "bootinfo.h"
#include "string.h"

#define RECURSIVE_PML4_INDEX 510
#define HHDM_START 0xFFFF800000000000ULL

// Cache control flags
#define PAGE_PCD (1ULL << 4)  // Cache Disable
#define PAGE_PWT (1ULL << 3)  // Write-Through

// Force Uncacheable for all mappings to avoid coherency issues
#define PAGE_UNCACHED (PAGE_PCD | PAGE_PWT)  // UC in PAT

uint64_t vmm_max_physical = 0;
static BootInfo* vmm_bootinfo = NULL;

static uint64_t* get_pml4_virt(void) {
    if (vmm_bootinfo && vmm_bootinfo->pml4_virt) {
        return (uint64_t*)vmm_bootinfo->pml4_virt;
    }
    return (uint64_t*)0xFFFFFF7FBFDFE000ULL;
}

static inline uint64_t* phys_to_virt(uint64_t phys) {
    return (uint64_t*)(HHDM_START + phys);
}

void* ensure_hhdm_mapped(uint64_t phys) {
    uint64_t virt = HHDM_START + phys;

    if (!vmm_is_mapped(virt)) {
        vmm_map_page(virt, phys, PT_PRESENT | PT_WRITE | PAGE_UNCACHED);
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

    vmm_bootinfo = info;

    uint64_t max_phys = 0;
    uint64_t total_usable = 0;

    if (info && info->memory_map_count > 0) {
        MemoryMapEntry* entries = (MemoryMapEntry*)info->memory_map_addr;
        for (uint64_t i = 0; i < info->memory_map_count; i++) {
            if (entries[i].type == 1) {
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
        max_phys = 128ULL * 1024 * 1024;
        serial_print("VMM: No memory map, using fallback 128MB\n");
    }

    vmm_max_physical = max_phys;

    for (uint64_t addr = 0; addr < 0x200000; addr += 0x1000) {
        vmm_map_page(addr, addr, PT_PRESENT | PT_WRITE | PAGE_UNCACHED);
    }

    serial_print("VMM: Physical memory into HHDM is already mapped by bootloader.\n");

    uint64_t bootinfo_phys = (uint64_t)info;
    serial_print("VMM: BootInfo page at phys=");
    serial_print_hex(bootinfo_phys);
    serial_print("\n");

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
    phys &= ~0xFFFULL;

    uint64_t user_flag    = (flags & PT_USER) ? PT_USER : 0;
    uint64_t write_flag   = (flags & PT_WRITE) ? PT_WRITE : 0;
    uint64_t present_flag = PT_PRESENT;
    uint64_t nx_flag      = (flags & PT_NX) ? PT_NX : 0;
    uint64_t cache_flags  = flags & (PAGE_PCD | PAGE_PWT);

    uint64_t* pml4 = get_pml4_virt();

    uint32_t pml4_idx = (virt >> 39) & 0x1FF;
    uint32_t pdpt_idx = (virt >> 30) & 0x1FF;
    uint32_t pd_idx   = (virt >> 21) & 0x1FF;
    uint32_t pt_idx   = (virt >> 12) & 0x1FF;

    uint64_t dir_flags = PT_PRESENT | PT_WRITE | PT_USER | cache_flags;

    uint64_t* pdpt;
    if (!(pml4[pml4_idx] & PT_PRESENT)) {
        uint64_t new_pdpt_phys = pmm_alloc_page_for_tables();
        if (!new_pdpt_phys) return;
        memset(phys_to_virt(new_pdpt_phys), 0, PAGE_SIZE);
        pml4[pml4_idx] = new_pdpt_phys | dir_flags;
        pdpt = phys_to_virt(new_pdpt_phys);
    } else {
        pml4[pml4_idx] |= (PT_WRITE | PT_USER);
        pdpt = phys_to_virt(pml4[pml4_idx] & ~0xFFFULL);
    }

    uint64_t* pd;
    if (!(pdpt[pdpt_idx] & PT_PRESENT)) {
        uint64_t new_pd_phys = pmm_alloc_page_for_tables();
        if (!new_pd_phys) return;
        memset(phys_to_virt(new_pd_phys), 0, PAGE_SIZE);
        pdpt[pdpt_idx] = new_pd_phys | dir_flags;
        pd = phys_to_virt(new_pd_phys);
    } else {
        pdpt[pdpt_idx] |= (PT_WRITE | PT_USER);
        pd = phys_to_virt(pdpt[pdpt_idx] & ~0xFFFULL);
    }

    uint64_t* pt;
    if (!(pd[pd_idx] & PT_PRESENT)) {
        uint64_t new_pt_phys = pmm_alloc_page_for_tables();
        if (!new_pt_phys) return;
        memset(phys_to_virt(new_pt_phys), 0, PAGE_SIZE);
        pd[pd_idx] = new_pt_phys | dir_flags;
        pt = phys_to_virt(new_pt_phys);
    } else {
        pd[pd_idx] |= (PT_WRITE | PT_USER);
        pt = phys_to_virt(pd[pd_idx] & ~0xFFFULL);
    }

    uint64_t pte = phys | present_flag | write_flag | user_flag | cache_flags;
    if (nx_flag) pte |= PT_NX;

    pt[pt_idx] = pte;
    asm volatile("invlpg (%0)" : : "r"(virt) : "memory");
}

void vmm_unmap_page(uint64_t virt) {
    uint64_t* pml4 = get_pml4_virt();

    uint32_t pml4_idx = (virt >> 39) & 0x1FF;
    uint32_t pdpt_idx = (virt >> 30) & 0x1FF;
    uint32_t pd_idx   = (virt >> 21) & 0x1FF;
    uint32_t pt_idx   = (virt >> 12) & 0x1FF;

    if (!(pml4[pml4_idx] & PT_PRESENT)) return;
    uint64_t* pdpt = phys_to_virt(pml4[pml4_idx] & ~0xFFFULL);

    if (!(pdpt[pdpt_idx] & PT_PRESENT)) return;
    uint64_t* pd = phys_to_virt(pdpt[pdpt_idx] & ~0xFFFULL);

    if (!(pd[pd_idx] & PT_PRESENT)) return;
    uint64_t* pt = phys_to_virt(pd[pd_idx] & ~0xFFFULL);

    pt[pt_idx] = 0;
    asm volatile("invlpg (%0)" : : "r"(virt) : "memory");
}

uint64_t vmm_get_phys(uint64_t virt) {
    uint64_t* pml4 = get_pml4_virt();

    uint32_t pml4_idx = (virt >> 39) & 0x1FF;
    uint32_t pdpt_idx = (virt >> 30) & 0x1FF;
    uint32_t pd_idx   = (virt >> 21) & 0x1FF;
    uint32_t pt_idx   = (virt >> 12) & 0x1FF;

    if (!(pml4[pml4_idx] & PT_PRESENT)) return 0;
    uint64_t* pdpt = phys_to_virt(pml4[pml4_idx] & ~0xFFFULL);

    if (!(pdpt[pdpt_idx] & PT_PRESENT)) return 0;
    uint64_t* pd = phys_to_virt(pdpt[pdpt_idx] & ~0xFFFULL);

    if (!(pd[pd_idx] & PT_PRESENT)) return 0;
    uint64_t* pt = phys_to_virt(pd[pd_idx] & ~0xFFFULL);

    if (!(pt[pt_idx] & PT_PRESENT)) return 0;
    return (pt[pt_idx] & ~0xFFFULL) | (virt & 0xFFFULL);
}

int vmm_is_mapped(uint64_t virt) {
    uint64_t* pml4 = get_pml4_virt();

    uint32_t pml4_idx = (virt >> 39) & 0x1FF;
    uint32_t pdpt_idx = (virt >> 30) & 0x1FF;
    uint32_t pd_idx   = (virt >> 21) & 0x1FF;
    uint32_t pt_idx   = (virt >> 12) & 0x1FF;

    if (!(pml4[pml4_idx] & PT_PRESENT)) return 0;
    uint64_t* pdpt = phys_to_virt(pml4[pml4_idx] & ~0xFFFULL);

    if (!(pdpt[pdpt_idx] & PT_PRESENT)) return 0;
    uint64_t* pd = phys_to_virt(pdpt[pdpt_idx] & ~0xFFFULL);

    if (!(pd[pd_idx] & PT_PRESENT)) return 0;
    uint64_t* pt = phys_to_virt(pd[pd_idx] & ~0xFFFULL);

    if (!(pt[pt_idx] & PT_PRESENT)) return 0;
    return 1;
}

void vmm_dump_page_table(uint64_t virt) {
    uint64_t* pml4 = get_pml4_virt();
    uint32_t pml4_idx = (virt >> 39) & 0x1FF;
    uint32_t pdpt_idx = (virt >> 30) & 0x1FF;
    uint32_t pd_idx   = (virt >> 21) & 0x1FF;
    uint32_t pt_idx   = (virt >> 12) & 0x1FF;

    serial_print("\n=== PAGE TABLE DUMP ===\n");
    if (!(pml4[pml4_idx] & PT_PRESENT)) return;
    uint64_t* pdpt = phys_to_virt(pml4[pml4_idx] & ~0xFFFULL);
    if (!(pdpt[pdpt_idx] & PT_PRESENT)) return;
    uint64_t* pd = phys_to_virt(pdpt[pdpt_idx] & ~0xFFFULL);
    if (!(pd[pd_idx] & PT_PRESENT)) return;
    uint64_t* pt = phys_to_virt(pd[pd_idx] & ~0xFFFULL);
    
    serial_print("  PTE Entry Found: 0x");
    serial_print_hex(pt[pt_idx]);
    serial_print("\n");
}

uint64_t vmm_clone_page_table(uint64_t src_cr3) {
    uint64_t new_pml4_phys = pmm_alloc_page_for_tables();
    if (!new_pml4_phys) return 0;

    uint64_t* src_pml4 = (uint64_t*)phys_to_virt(src_cr3);
    uint64_t* new_pml4 = (uint64_t*)phys_to_virt(new_pml4_phys);

    memset(new_pml4, 0, PAGE_SIZE);

    for (int i = 0; i < 512; i++) {
        if (i == RECURSIVE_PML4_INDEX) continue;
        if (src_pml4[i] & PT_PRESENT) {
            new_pml4[i] = src_pml4[i];
        }
    }
    new_pml4[RECURSIVE_PML4_INDEX] = new_pml4_phys | PT_PRESENT | PT_WRITE;
    return new_pml4_phys;
}

uint64_t vmm_get_phys_from_cr3(uint64_t cr3, uint64_t virt) {
    uint64_t* pml4 = (uint64_t*)phys_to_virt(cr3);
    uint32_t pml4_idx = (virt >> 39) & 0x1FF;
    uint32_t pdpt_idx = (virt >> 30) & 0x1FF;
    uint32_t pd_idx   = (virt >> 21) & 0x1FF;
    uint32_t pt_idx   = (virt >> 12) & 0x1FF;

    if (!(pml4[pml4_idx] & PT_PRESENT)) return 0;
    uint64_t* pdpt = (uint64_t*)phys_to_virt(pml4[pml4_idx] & ~0xFFFULL);

    if (!(pdpt[pdpt_idx] & PT_PRESENT)) return 0;
    uint64_t* pd = (uint64_t*)phys_to_virt(pdpt[pdpt_idx] & ~0xFFFULL);

    if (!(pd[pd_idx] & PT_PRESENT)) return 0;
    uint64_t* pt = (uint64_t*)phys_to_virt(pd[pd_idx] & ~0xFFFULL);

    if (!(pt[pt_idx] & PT_PRESENT)) return 0;
    return (pt[pt_idx] & ~0xFFFULL) | (virt & 0xFFFULL);
}

/* ---------------------------------------------------------------------------
 * ROBUST CROSS-CR3 MAPPING ROUTINE (WITH HIERARCHICAL PERMISSION FIX)
 * This traverses any target process's page table directly via HHDM offsets,
 * keeping the active kernel visible during multi-page segment mapping operations.
 * --------------------------------------------------------------------------- */
void vmm_map_page_in_cr3(uint64_t cr3, uint64_t virt, uint64_t phys, uint64_t flags) {
    phys &= ~0xFFFULL;

    uint64_t user_flag    = (flags & PT_USER) ? PT_USER : 0;
    uint64_t write_flag   = (flags & PT_WRITE) ? PT_WRITE : 0;
    uint64_t present_flag = PT_PRESENT;
    uint64_t nx_flag      = (flags & PT_NX) ? PT_NX : 0;
    uint64_t cache_flags  = flags & (PAGE_PCD | PAGE_PWT);

    uint64_t* pml4 = (uint64_t*)phys_to_virt(cr3);

    uint32_t pml4_idx = (virt >> 39) & 0x1FF;
    uint32_t pdpt_idx = (virt >> 30) & 0x1FF;
    uint32_t pd_idx   = (virt >> 21) & 0x1FF;
    uint32_t pt_idx   = (virt >> 12) & 0x1FF;

    uint64_t dir_flags = PT_PRESENT | PT_WRITE | PT_USER | cache_flags;

    uint64_t* pdpt;
    if (!(pml4[pml4_idx] & PT_PRESENT)) {
        uint64_t new_pdpt_phys = pmm_alloc_page_for_tables();
        if (!new_pdpt_phys) return;
        memset(phys_to_virt(new_pdpt_phys), 0, PAGE_SIZE);
        pml4[pml4_idx] = new_pdpt_phys | dir_flags;
        pdpt = (uint64_t*)phys_to_virt(new_pdpt_phys);
    } else {
        /* PATCH: Explicitly enforce hierarchical user privileges on existing entries */
        pml4[pml4_idx] |= (PT_WRITE | PT_USER);
        pdpt = (uint64_t*)phys_to_virt(pml4[pml4_idx] & ~0xFFFULL);
    }

    uint64_t* pd;
    if (!(pdpt[pdpt_idx] & PT_PRESENT)) {
        uint64_t new_pd_phys = pmm_alloc_page_for_tables();
        if (!new_pd_phys) return;
        memset(phys_to_virt(new_pd_phys), 0, PAGE_SIZE);
        pdpt[pdpt_idx] = new_pd_phys | dir_flags;
        pd = (uint64_t*)phys_to_virt(new_pd_phys);
    } else {
        /* PATCH: Explicitly enforce hierarchical user privileges on existing entries */
        pdpt[pdpt_idx] |= (PT_WRITE | PT_USER);
        pd = (uint64_t*)phys_to_virt(pdpt[pdpt_idx] & ~0xFFFULL);
    }

    uint64_t* pt;
    if (!(pd[pd_idx] & PT_PRESENT)) {
        uint64_t new_pt_phys = pmm_alloc_page_for_tables();
        if (!new_pt_phys) return;
        memset(phys_to_virt(new_pt_phys), 0, PAGE_SIZE);
        pd[pd_idx] = new_pt_phys | dir_flags;
        pt = (uint64_t*)phys_to_virt(new_pt_phys);
    } else {
        /* PATCH: Explicitly enforce hierarchical user privileges on existing entries */
        pd[pd_idx] |= (PT_WRITE | PT_USER);
        pt = (uint64_t*)phys_to_virt(pd[pd_idx] & ~0xFFFULL);
    }

    uint64_t pte = phys | present_flag | write_flag | user_flag | cache_flags;
    if (nx_flag) pte |= PT_NX;

    pt[pt_idx] = pte;

    /* Flush the specific TLB entry if this table corresponds to the active CPU context */
    uint64_t active_cr3;
    asm volatile("mov %%cr3, %0" : "=r"(active_cr3));
    if ((active_cr3 & ~0xFFFULL) == (cr3 & ~0xFFFULL)) {
        asm volatile("invlpg (%0)" : : "r"(virt) : "memory");
    }
}


void vmm_unmap_page_in_cr3(uint64_t cr3, uint64_t virt) {
    uint64_t* pml4 = (uint64_t*)phys_to_virt(cr3);
    uint32_t pml4_idx = (virt >> 39) & 0x1FF;
    uint32_t pdpt_idx = (virt >> 30) & 0x1FF;
    uint32_t pd_idx   = (virt >> 21) & 0x1FF;
    uint32_t pt_idx   = (virt >> 12) & 0x1FF;

    if (!(pml4[pml4_idx] & PT_PRESENT)) return;
    uint64_t* pdpt = (uint64_t*)phys_to_virt(pml4[pml4_idx] & ~0xFFFULL);

    if (!(pdpt[pdpt_idx] & PT_PRESENT)) return;
    uint64_t* pd = (uint64_t*)phys_to_virt(pdpt[pdpt_idx] & ~0xFFFULL);

    if (!(pd[pd_idx] & PT_PRESENT)) return;
    uint64_t* pt = (uint64_t*)phys_to_virt(pd[pd_idx] & ~0xFFFULL);

    pt[pt_idx] = 0;

    uint64_t active_cr3;
    asm volatile("mov %%cr3, %0" : "=r"(active_cr3));
    if ((active_cr3 & ~0xFFFULL) == (cr3 & ~0xFFFULL)) {
        asm volatile("invlpg (%0)" : : "r"(virt) : "memory");
    }
}
