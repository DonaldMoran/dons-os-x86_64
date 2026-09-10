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
#include <string.h>
#include "include/debug.h"

#ifndef SHN_UNDEF
#define SHN_UNDEF 0
#endif

typedef struct {
    uint32_t sh_name;
    uint32_t sh_type;
    uint64_t sh_flags;
    uint64_t sh_addr;
    uint64_t sh_offset;
    uint64_t sh_size;
    uint32_t sh_link;
    uint32_t sh_info;
    uint64_t sh_addralign;
    uint64_t sh_entsize;
} Elf64_Shdr;

static int elf_validate(const Elf64_Ehdr* ehdr) {
    /* PATCH: Explicitly index individual array elements to read byte values 
       instead of comparing the base array pointer address to integers! */
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

void elf_add_page_to_pcb(pcb_t* pcb, uint64_t phys) {
    if (!pcb) return;
    if (!pcb->elf_page_list) {
        pcb->elf_page_list = (uint64_t*)kmalloc(64 * sizeof(uint64_t));
        if (!pcb->elf_page_list) {
            serial_print("ELF: Failed to allocate page list!\n");
            return;
        }
        pcb->elf_num_pages = 0;
    }

    for (uint64_t i = 0; i < pcb->elf_num_pages; i++) {
        if (pcb->elf_page_list[i] == phys) {
            return;
        }
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

/* PATCHED: Now maps binary memory segments cleanly into a designated PCB container
   and returns the true parsed Entry Point address back to the kernel scheduler. */
uint64_t elf_load_into_process(pcb_t* pcb, const void* elf_data) {
    const Elf64_Ehdr* ehdr = (const Elf64_Ehdr*)elf_data;

    if (elf_validate(ehdr) < 0) {
        serial_print("ELF: Validation checks failed!\n");
        return 0;
    }

    const Elf64_Phdr* phdr = (const Elf64_Phdr*)((uintptr_t)elf_data + ehdr->e_phoff);

    for (int i = 0; i < ehdr->e_phnum; i++) {
        if (phdr[i].p_type != PT_LOAD)
            continue;

        uint64_t vaddr  = phdr[i].p_vaddr;
        uint64_t memsz  = phdr[i].p_memsz;
        uint64_t filesz = phdr[i].p_filesz;
        uint64_t offset = phdr[i].p_offset;

        uint64_t start_page = vaddr & ~0xFFFULL;
        uint64_t end_page   = (vaddr + memsz + 0xFFF) & ~0xFFFULL;

        for (uint64_t virt = start_page; virt < end_page; virt += 4096) {
            uint64_t phys = pmm_alloc_page(PAGE_USER_DATA);
            if (!phys) {
                serial_print("ELF: Out of physical page frames!\n");
                return 0;
            }

            /* PATCH: Hardcode the absolute standard x86_64 privilege bitmask (0x07) 
               combined with your system's 0x18 cache flags.
               0x01 (Present) | 0x02 (Writable) | 0x04 (User Space) | 0x18 (Uncacheable) = 0x1F */
            uint64_t map_flags = 0x1FULL;
            
            vmm_map_page_in_cr3(pcb->cr3, virt, phys, map_flags);
            ensure_hhdm_mapped(phys);
            elf_add_page_to_pcb(pcb, phys);
        }

        /* Copy file bytes securely using high-half direct maps */
        const uint8_t* src = (const uint8_t*)elf_data + offset;
        if (filesz > 0) {
            uint64_t copied = 0;
            while (copied < filesz) {
                uint64_t cur_virt = vaddr + copied;
                uint64_t phys = vmm_get_phys_from_cr3(pcb->cr3, cur_virt);
                uint64_t page_off = cur_virt & 0xFFF;
                size_t chunk = filesz - copied;
                size_t page_rem = 4096 - page_off;
                if (chunk > page_rem) chunk = page_rem;
                uint8_t* dest = (uint8_t*)(HHDM_START + phys);
                for (size_t j = 0; j < chunk; j++) {
                    dest[page_off + j] = src[copied + j];
                }
                copied += chunk;
            }
        }

        /* Initialize memory for zero-filled BSS spaces */
        if (memsz > filesz) {
            uint64_t bss_start = vaddr + filesz;
            uint64_t bss_bytes = memsz - filesz;
            uint64_t zeroed = 0;
            while (zeroed < bss_bytes) {
                uint64_t cur_virt = bss_start + zeroed;
                uint64_t phys = vmm_get_phys_from_cr3(pcb->cr3, cur_virt);
                uint64_t page_off = cur_virt & 0xFFF;
                size_t chunk = bss_bytes - zeroed;
                size_t page_rem = 4096 - page_off;
                if (chunk > page_rem) chunk = page_rem;
                uint8_t* dest = (uint8_t*)(HHDM_START + phys);
                for (size_t j = 0; j < chunk; j++) {
                    dest[page_off + j] = 0;
                }
                zeroed += chunk;
            }
        }
    }

    /* Return the clean, parsed authentic 64-bit Entry Point address string location */
    return ehdr->e_entry;
}
