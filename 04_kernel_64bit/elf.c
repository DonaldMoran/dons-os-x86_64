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
    if (ehdr->e_ident[0] != ELF_MAGIC0 ||
        ehdr->e_ident[1] != ELF_MAGIC1 ||
        ehdr->e_ident[2] != ELF_MAGIC2 ||
        ehdr->e_ident[3] != ELF_MAGIC3) {
        return -1;
    }
    if (ehdr->e_ident[4] != 2) return -1;
    if (ehdr->e_ident[5] != 1) return -1;
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

/* ---------------------------------------------------------------------------
 * Diagnostic: dump the first 16 bytes at a given user virtual address
 * --------------------------------------------------------------------------- */
static void elf_verify_user_bytes(const char *label, uint64_t cr3, uint64_t virt, int n) {
    uint64_t phys = vmm_get_phys_from_cr3(cr3, virt);
    serial_print("ELF: VERIFY ");
    serial_print(label);
    serial_print(" vaddr=");
    serial_print_hex(virt);
    serial_print(" phys=");
    serial_print_hex(phys);
    serial_print(" bytes=");
    if (phys == 0) {
        serial_print("PAGE-NOT-MAPPED\n");
        return;
    }
    uint8_t* p = (uint8_t*)(HHDM_START + phys);
    for (int k = 0; k < n; k++) {
        serial_print_hex(p[k]);
    }
    serial_print("\n");
}

uint64_t elf_load_into_process(pcb_t* pcb, const void* elf_data) {
    const Elf64_Ehdr* ehdr = (const Elf64_Ehdr*)elf_data;

    if (elf_validate(ehdr) < 0) {
        serial_print("ELF: Validation checks failed!\n");
        return 0;
    }

    serial_print("ELF: elf_data=");
    serial_print_hex((uint64_t)(uintptr_t)elf_data);
    serial_print(" e_phoff=");
    serial_print_hex(ehdr->e_phoff);
    serial_print(" e_phnum=");
    serial_print_hex(ehdr->e_phnum);
    serial_print(" e_entry=");
    serial_print_hex(ehdr->e_entry);
    serial_print("\n");

    const Elf64_Phdr* phdr = (const Elf64_Phdr*)((uintptr_t)elf_data + ehdr->e_phoff);

    for (int i = 0; i < ehdr->e_phnum; i++) {
        if (phdr[i].p_type != PT_LOAD)
            continue;

        uint64_t vaddr  = phdr[i].p_vaddr;
        uint64_t memsz  = phdr[i].p_memsz;
        uint64_t filesz = phdr[i].p_filesz;
        uint64_t offset = phdr[i].p_offset;

        serial_print("ELF: PT_LOAD[");
        serial_print_hex((uint64_t)i);
        serial_print("] off=");
        serial_print_hex(offset);
        serial_print(" vaddr=");
        serial_print_hex(vaddr);
        serial_print(" filesz=");
        serial_print_hex(filesz);
        serial_print(" memsz=");
        serial_print_hex(memsz);
        serial_print("\n");

        uint64_t start_page = vaddr & ~0xFFFULL;
        uint64_t end_page   = (vaddr + memsz + 0xFFF) & ~0xFFFULL;

        for (uint64_t virt = start_page; virt < end_page; virt += 4096) {
            uint64_t phys = pmm_alloc_page(PAGE_USER_DATA);
            if (!phys) {
                serial_print("ELF: Out of physical page frames!\n");
                return 0;
            }

            uint64_t map_flags = 0x1FULL;
            vmm_map_page_in_cr3(pcb->cr3, virt, phys, map_flags);
            ensure_hhdm_mapped(phys);
            elf_add_page_to_pcb(pcb, phys);
        }

        /* -------------------------------------------------------------------
         * Copy file bytes.
         *
         * NOTE: vmm_get_phys_from_cr3() returns the PHYSICAL address INCLUDING
         * the page offset. So `dest = HHDM_START + phys` already points to the
         * exact byte at `cur_virt`. Do NOT add `page_off` again when indexing
         * `dest`.
         * ------------------------------------------------------------------- */
        const uint8_t* src = (const uint8_t*)elf_data + offset;
        if (filesz > 0) {
            uint64_t copied = 0;
            while (copied < filesz) {
                uint64_t cur_virt = vaddr + copied;
                uint64_t phys = vmm_get_phys_from_cr3(pcb->cr3, cur_virt);
                if (phys == 0) {
                    serial_print("ELF: COPY-FAIL phys=0 at vaddr=");
                    serial_print_hex(cur_virt);
                    serial_print("\n");
                    return 0;
                }
                uint64_t page_off = cur_virt & 0xFFF;
                size_t chunk = filesz - copied;
                size_t page_rem = 4096 - page_off;
                if (chunk > page_rem) chunk = page_rem;
                uint8_t* dest = (uint8_t*)(HHDM_START + phys);
                for (size_t j = 0; j < chunk; j++) {
                    dest[j] = src[copied + j];      /* FIX: no + page_off */
                }
                copied += chunk;
            }
        }

        /* -------------------------------------------------------------------
         * Zero-fill BSS region.
         *
         * Same caveat as above: `dest` already points to the exact byte.
         * ------------------------------------------------------------------- */
        if (memsz > filesz) {
            uint64_t bss_start = vaddr + filesz;
            uint64_t bss_bytes = memsz - filesz;
            uint64_t zeroed = 0;
            while (zeroed < bss_bytes) {
                uint64_t cur_virt = bss_start + zeroed;
                uint64_t phys = vmm_get_phys_from_cr3(pcb->cr3, cur_virt);
                if (phys == 0) {
                    serial_print("ELF: BSS-FAIL phys=0 at vaddr=");
                    serial_print_hex(cur_virt);
                    serial_print("\n");
                    return 0;
                }
                uint64_t page_off = cur_virt & 0xFFF;
                size_t chunk = bss_bytes - zeroed;
                size_t page_rem = 4096 - page_off;
                if (chunk > page_rem) chunk = page_rem;
                uint8_t* dest = (uint8_t*)(HHDM_START + phys);
                for (size_t j = 0; j < chunk; j++) {
                    dest[j] = 0;                    /* FIX: no + page_off */
                }
                zeroed += chunk;
            }
        }

        /* ============================================================
         * POST-COPY VERIFICATION
         * ============================================================ */
        if (i == 0) {
            elf_verify_user_bytes("memset_bytes", pcb->cr3, 0x800000118cULL, 16);
            elf_verify_user_bytes("__sinit_bytes", pcb->cr3, 0x8000000a50ULL, 16);
            elf_verify_user_bytes("_start_first8", pcb->cr3, 0x8000000000ULL, 8);
        }
        if (i == 1) {
            elf_verify_user_bytes("bss_3000", pcb->cr3, 0x8000003000ULL, 16);
            elf_verify_user_bytes("bss_3200", pcb->cr3, 0x8000003200ULL, 16);
            elf_verify_user_bytes("main_first16", pcb->cr3, 0x8000000530ULL, 16);
        }
    }

    return ehdr->e_entry;
}
