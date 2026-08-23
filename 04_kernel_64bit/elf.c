#include "include/elf.h"
#include "include/vga.h"
#include "include/serial.h"
#include "include/pmm.h"
#include "include/vmm.h"
#include "include/process.h"
#include "include/scheduler.h"
#include "include/heap.h"
#include "include/user_space.h" 
#include <stddef.h>
#include <stdint.h>
#include "include/debug.h"

static int elf_validate(const Elf64_Ehdr* ehdr) {
    if (ehdr->e_ident[0] != ELF_MAGIC0 ||
        ehdr->e_ident[1] != ELF_MAGIC1 ||
        ehdr->e_ident[2] != ELF_MAGIC2 ||
        ehdr->e_ident[3] != ELF_MAGIC3) {
        return -1;
    }
    if (ehdr->e_ident[4] != 2) return -1; // 64-bit
    if (ehdr->e_ident[5] != 1) return -1; // little-endian
    return 0;
}

static void copy_bytes(uint8_t* dest, const uint8_t* src, size_t n) {
    for (size_t i = 0; i < n; i++) {
        dest[i] = src[i];
    }
}

static void elf_add_page_to_pcb(pcb_t* pcb, uint64_t phys) {
    if (!pcb) return;
    if (!pcb->elf_page_list) {
        pcb->elf_page_list = (uint64_t*)kmalloc(64 * sizeof(uint64_t));
        if (!pcb->elf_page_list) {
            serial_print("ELF: Failed to allocate page list!\n");
            return;
        }
        pcb->elf_num_pages = 0;
    }
    
    if ((pcb->elf_num_pages % 64) == 0 && pcb->elf_num_pages > 0) {
        uint64_t* new_list = (uint64_t*)kmalloc((pcb->elf_num_pages + 64) * sizeof(uint64_t));
        if (!new_list) {
            serial_print("ELF: Failed to reallocate page list!\n");
            return;
        }
        for (uint64_t i = 0; i < pcb->elf_num_pages; i++) {
            new_list[i] = pcb->elf_page_list[i];
        }
        kfree(pcb->elf_page_list);
        pcb->elf_page_list = new_list;
    }
    
    pcb->elf_page_list[pcb->elf_num_pages++] = phys;
    if (pcb->elf_num_pages == 1) {
        pcb->elf_base_phys = phys;
    }
}

void elf_load(const void* elf_data) {
    const Elf64_Ehdr* ehdr = (const Elf64_Ehdr*)elf_data;
    
    serial_print("ELF: Loading\n");
    
    if (elf_validate(ehdr) < 0) {
        vga_print("Invalid ELF\n");
        return;
    }
    
    serial_print("ELF: Entry=0x");
    serial_print_hex(ehdr->e_entry);
    serial_print("\n");
    serial_print("ELF: Program headers=");
    serial_print_dec(ehdr->e_phnum);
    serial_print("\n");
    
    const Elf64_Phdr* phdr = (const Elf64_Phdr*)((uintptr_t)elf_data + ehdr->e_phoff);
    
    uint64_t entry_point = ehdr->e_entry;

    int segments_loaded = 0;
    int total_phdrs = ehdr->e_phnum;
    
    // Create a process for this ELF program
    pcb_t* pcb = process_create("elf_prog", entry_point, 0);
    if (!pcb) {
        serial_print("ELF: Failed to create process!\n");
        vga_print("Failed to create process\n");
        return;
    }
    
    pcb->elf_base_virt = 0xFFFFFFFFFFFFFFFFULL;
    
    serial_print("ELF: Scanning program headers\n");
    
    for (int i = 0; i < total_phdrs; i++) {
        serial_print("ELF: Header ");
        serial_print_dec(i);
        serial_print(" type=");
        serial_print_dec(phdr[i].p_type);
        serial_print("\n");
        
        if (phdr[i].p_type == PT_LOAD) {
            uint64_t vaddr  = phdr[i].p_vaddr;
            uint64_t memsz  = phdr[i].p_memsz;
            uint64_t filesz = phdr[i].p_filesz;
            uint64_t offset = phdr[i].p_offset;
            
            serial_print("ELF: LOAD vaddr=0x");
            serial_print_hex(vaddr);
            serial_print(" memsz=0x");
            serial_print_hex(memsz);
            serial_print(" filesz=0x");
            serial_print_hex(filesz);
            serial_print(" offset=0x");
            serial_print_hex(offset);
            serial_print("\n");
            
            uint64_t start_page = vaddr & ~0xFFFULL;
            uint64_t end_page   = (vaddr + memsz + 0xFFF) & ~0xFFFULL;
            uint64_t num_pages  = (end_page - start_page) / 4096;
            
            if (pcb->elf_base_virt == 0xFFFFFFFFFFFFFFFFULL) {
                pcb->elf_base_virt = start_page;
            } else if (start_page < pcb->elf_base_virt) {
                pcb->elf_base_virt = start_page;
            }
            
            serial_print("ELF: start_page=0x");
            serial_print_hex(start_page);
            serial_print(" end_page=0x");
            serial_print_hex(end_page);
            serial_print(" num_pages=");
            serial_print_dec(num_pages);
            serial_print("\n");
            
            uint64_t page_flags = PT_PRESENT | PT_WRITE | PT_USER;
            
            for (uint64_t j = 0; j < num_pages; j++) {
                uint64_t virt = start_page + (j * 4096);
                
                uint64_t existing_phys = vmm_get_phys(virt);
                if (existing_phys != 0) {
                    serial_print("ELF: Page already mapped at 0x");
                    serial_print_hex(virt);
                    serial_print(" phys=0x");
                    serial_print_hex(existing_phys);
                    serial_print(" - remapping with user flags\n");
                    vmm_map_page(virt, existing_phys, page_flags);
                    ensure_hhdm_mapped(existing_phys);
                    __asm__ volatile ("invlpg (%0)" : : "r" (virt) : "memory");
                    elf_add_page_to_pcb(pcb, existing_phys);
                    continue;
                }
                
                uint64_t phys = pmm_alloc_page();
                if (!phys) {
                    serial_print("ELF: Failed to allocate physical page!\n");
                    vga_print("Failed alloc\n");
                    process_destroy(pcb);
                    return;
                }
                serial_print("ELF: Mapping new page virt=0x");
                serial_print_hex(virt);
                serial_print(" phys=0x");
                serial_print_hex(phys);
                serial_print("\n");
                vmm_map_page(virt, phys, page_flags);
                ensure_hhdm_mapped(phys);
                __asm__ volatile ("invlpg (%0)" : : "r" (virt) : "memory");
                elf_add_page_to_pcb(pcb, phys);
            }
        }
    }
    
    for (int i = 0; i < total_phdrs; i++) {
        if (phdr[i].p_type == PT_LOAD) {
            uint64_t vaddr  = phdr[i].p_vaddr;
            uint64_t memsz  = phdr[i].p_memsz;
            uint64_t filesz = phdr[i].p_filesz;
            uint64_t offset = phdr[i].p_offset;
            
            const uint8_t* src = (const uint8_t*)elf_data + offset;
            
            serial_print("ELF: Copying filesz=0x");
            serial_print_hex(filesz);
            serial_print(" to 0x");
            serial_print_hex(vaddr);
            serial_print(" via HHDM (paged)\n");
            
            uint64_t copied = 0;
            while (copied < filesz) {
                uint64_t cur_virt = vaddr + copied;
                uint64_t phys = vmm_get_phys(cur_virt);
                if (!phys) {
                    serial_print("ELF: Failed to get physical address for 0x");
                    serial_print_hex(cur_virt);
                    serial_print("\n");
                    process_destroy(pcb);
                    return;
                }
                
                uint64_t page_off = cur_virt & 0xFFF;
                size_t   chunk    = filesz - copied;
                size_t   page_rem = 4096 - page_off;
                if (chunk > page_rem) chunk = page_rem;
                
                void* hhdm_dest = (void*)(HHDM_START + phys + page_off);
                
                copy_bytes((uint8_t*)hhdm_dest, src + copied, chunk);
                copied += chunk;
            }
            
            serial_print("ELF: Verified data at 0x");
            serial_print_hex(vaddr);
            serial_print(": ");
            uint64_t verify_count = (filesz < 16) ? filesz : 16;
            for (uint64_t j = 0; j < verify_count; j++) {
                uint64_t cur_virt = vaddr + j;
                uint64_t phys = vmm_get_phys(cur_virt);
                if (!phys) break;
                uint64_t page_off = cur_virt & 0xFFF;
                uint8_t* hhdm_ptr = (uint8_t*)(HHDM_START + phys + page_off);
                serial_print_hex(hhdm_ptr[0]);
                serial_print(" ");
            }
            serial_print("\n");
            
            if (memsz > filesz) {
                serial_print("ELF: Zeroing BSS\n");
                uint64_t bss_start_virt = vaddr + filesz;
                uint64_t bss_bytes      = memsz - filesz;
                
                uint64_t zeroed = 0;
                while (zeroed < bss_bytes) {
                    uint64_t cur_virt = bss_start_virt + zeroed;
                    uint64_t phys = vmm_get_phys(cur_virt);
                    if (!phys) break;
                    
                    uint64_t page_off = cur_virt & 0xFFF;
                    size_t   chunk    = bss_bytes - zeroed;
                    size_t   page_rem = 4096 - page_off;
                    if (chunk > page_rem) chunk = page_rem;
                    
                    uint8_t* hhdm_ptr = (uint8_t*)(HHDM_START + phys + page_off);
                    __asm__ volatile ("rep stosb"
                                      : "+D"(hhdm_ptr), "+c"(chunk)
                                      : "a"(0)
                                      : "memory");
                    zeroed += chunk;
                }
            }
            
            segments_loaded++;
        }
    }
    
    if (segments_loaded == 0) {
        vga_print("No segments\n");
        process_destroy(pcb);
        return;
    }
    
    serial_print("ELF: Process created successfully\n");
    vga_print("ELF loaded\n");
    
    serial_print("ELF: Starting process via scheduler\n");
    
    // Switch to the process
    scheduler_switch_to(pcb);
    
    // The scheduler will return here when the process exits
    serial_print("ELF: Process returned, cleaning up\n");
    process_destroy(pcb);
    
    serial_print("ELF: Load complete, returning to shell\n");
}
