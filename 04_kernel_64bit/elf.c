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

// ===============================
// Freestanding ELF64 section header
// ===============================
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

// ===============================
// NEW elf_load: map pages in kernel CR3, then clone
// ===============================
void elf_load(const void* elf_data) {
    const Elf64_Ehdr* ehdr = (const Elf64_Ehdr*)elf_data;

    serial_print("ELF: blob ptr = 0x");
    serial_print_hex((uint64_t)elf_data);
    serial_print("\n");

    serial_print("ELF: header bytes: ");
    const uint8_t *hdr = (const uint8_t *)elf_data;
    for (int i = 0; i < 16; i++) {
        serial_print_hex(hdr[i]);
        serial_print(" ");
    }
    serial_print("\n");

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

    // ===============================
    // 1. Create PCB (clones kernel CR3, allocates user stack)
    // ===============================
    pcb_t* pcb = process_create("elf_prog", entry_point, 0);
    if (!pcb) {
        serial_print("ELF: Failed to create process!\n");
        vga_print("Failed to create process\n");
        return;
    }

    // ===============================
    // 2. Map ELF pages directly into the process's CR3
    // ===============================
    for (int i = 0; i < ehdr->e_phnum; i++) {
        if (phdr[i].p_type != PT_LOAD)
            continue;

        uint64_t vaddr  = phdr[i].p_vaddr;
        uint64_t memsz  = phdr[i].p_memsz;
        uint64_t filesz = phdr[i].p_filesz;
        uint64_t offset = phdr[i].p_offset;

        uint64_t start_page = vaddr & ~0xFFFULL;
        uint64_t end_page   = (vaddr + memsz + 0xFFF) & ~0xFFFULL;

        serial_print("ELF: Mapping segment range 0x");
        serial_print_hex(start_page);
        serial_print(" - 0x");
        serial_print_hex(end_page);
        serial_print("\n");

        // Map pages in the process's CR3
        for (uint64_t virt = start_page; virt <= end_page; virt += 4096) {
            uint64_t phys = pmm_alloc_page(PAGE_USER_DATA);
            if (!phys) {
                serial_print("ELF: Failed to allocate physical page!\n");
                // TODO: cleanup
                return;
            }

            serial_print("ELF: Mapping new page virt=0x");
            serial_print_hex(virt);
            serial_print(" phys=0x");
            serial_print_hex(phys);
            serial_print("\n");

            vmm_map_page_in_cr3(pcb->cr3, virt, phys, PT_PRESENT | PT_WRITE | PT_USER);
            ensure_hhdm_mapped(phys);
            __asm__ volatile ("invlpg (%0)" : : "r" (virt) : "memory");
            elf_add_page_to_pcb(pcb, phys);
        }

        // Copy data and zero BSS using HHDM (physical addresses)
        const uint8_t* src = (const uint8_t*)elf_data + offset;
        if (filesz > 0) {
            serial_print("ELF: Copying filesz=0x");
            serial_print_hex(filesz);
            serial_print(" to 0x");
            serial_print_hex(vaddr);
            serial_print("\n");

            uint64_t copied = 0;
            while (copied < filesz) {
                uint64_t cur_virt = vaddr + copied;
                uint64_t phys = vmm_get_phys_from_cr3(pcb->cr3, cur_virt);
                if (!phys) {
                    serial_print("ELF: Failed to get physical address for 0x");
                    serial_print_hex(cur_virt);
                    serial_print("\n");
                    return;
                }
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
            serial_print("ELF: Copy complete\n");
        }

        if (memsz > filesz) {
            uint64_t bss_start = vaddr + filesz;
            uint64_t bss_bytes = memsz - filesz;
            serial_print("ELF: Zeroing BSS at 0x");
            serial_print_hex(bss_start);
            serial_print(" size=0x");
            serial_print_hex(bss_bytes);
            serial_print("\n");

            uint64_t zeroed = 0;
            while (zeroed < bss_bytes) {
                uint64_t cur_virt = bss_start + zeroed;
                uint64_t phys = vmm_get_phys_from_cr3(pcb->cr3, cur_virt);
                if (!phys) {
                    serial_print("ELF: Failed to get physical for BSS at 0x");
                    serial_print_hex(cur_virt);
                    serial_print("\n");
                    break;
                }
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

    serial_print("ELF: Process created successfully\n");
    vga_print("ELF loaded\n");

    // ===============================
    // 3. Switch to user mode
    // ===============================
    serial_print("ELF: Starting process via jump_to_user_mode\n");
    extern void jump_to_user_mode(uint64_t entry, uint64_t stack);

    uint64_t* tss_rsp0 = (uint64_t*)(HHDM_START + 0x5000 + 0x04);
    *tss_rsp0 = pcb->kernel_stack_top;
    serial_print("ELF: TSS RSP0 set to 0x");
    serial_print_hex(pcb->kernel_stack_top);
    serial_print("\n");

    // Switch to process's CR3
    asm volatile("mov %0, %%cr3" : : "r"(pcb->cr3));
    serial_print("ELF: Switched to process CR3\n");

    jump_to_user_mode(pcb->entry_point, pcb->user_stack_top);

    serial_print("ELF: jump_to_user_mode returned unexpectedly!\n");
    process_destroy(pcb);
    serial_print("ELF: Load complete, returning to shell\n");
}
