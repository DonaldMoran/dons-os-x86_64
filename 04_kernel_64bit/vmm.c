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
    
    vga_print("VMM: PML4[288] = 0x");
    vga_print_hex_cur(pml4[288]);
    vga_print("\n");
    serial_print("VMM: PML4[288] = 0x");
    serial_print_hex(pml4[288]);
    serial_print("\n");
    
    vga_print("VMM: Initialization complete\n");
    serial_print("VMM: Initialization complete\n");
}

void vmm_map_page(uint64_t virt, uint64_t phys, uint64_t flags) {
    (void)flags;
    
    uint64_t* pml4 = get_pml4_virt();
    
    uint32_t pml4_idx = (virt >> 39) & 0x1FF;
    uint32_t pdpt_idx = (virt >> 30) & 0x1FF;
    uint32_t pd_idx = (virt >> 21) & 0x1FF;
    uint32_t pt_idx = (virt >> 12) & 0x1FF;
    
    // Allocate PDPT
    uint64_t* pdpt;
    if (!(pml4[pml4_idx] & 0x01)) {
        uint64_t pdpt_phys = pmm_alloc_page();
        if (!pdpt_phys) return;
        pml4[pml4_idx] = pdpt_phys | PT_PRESENT | PT_WRITE;
        pdpt = (uint64_t*)pdpt_phys;
    } else {
        pdpt = (uint64_t*)(pml4[pml4_idx] & ~0xFFF);
    }
    
    // Allocate PD
    uint64_t* pd;
    if (!(pdpt[pdpt_idx] & 0x01)) {
        uint64_t pd_phys = pmm_alloc_page();
        if (!pd_phys) return;
        pdpt[pdpt_idx] = pd_phys | PT_PRESENT | PT_WRITE;
        pd = (uint64_t*)pd_phys;
    } else {
        pd = (uint64_t*)(pdpt[pdpt_idx] & ~0xFFF);
    }
    
    // Allocate PT
    uint64_t* pt;
    if (!(pd[pd_idx] & 0x01)) {
        uint64_t pt_phys = pmm_alloc_page();
        if (!pt_phys) return;
        pd[pd_idx] = pt_phys | PT_PRESENT | PT_WRITE;
        pt = (uint64_t*)pt_phys;
    } else {
        pt = (uint64_t*)(pd[pd_idx] & ~0xFFF);
    }
    
    // Map the page
    pt[pt_idx] = phys | PT_PRESENT | PT_WRITE;
    
    // Full TLB flush - read CR3 and write it back
    uint64_t cr3;
    asm volatile("mov %%cr3, %0" : "=r"(cr3));
    asm volatile("mov %0, %%cr3" : : "r"(cr3));
    
    // Also invalidate the specific address
    asm volatile("invlpg (%0)" : : "r" (virt) : "memory");
}
