#include "include/elf.h"
#include "include/vga.h"
#include "include/serial.h"
#include "include/pmm.h"
#include "include/vmm.h"
#include "include/ring3.h"
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

static void fast_memcpy(void* dest, const void* src, size_t n) {
    __asm__ volatile ("rep movsb" : "+D"(dest), "+S"(src), "+c"(n) : : "memory");
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
    uint64_t user_stack  = USER_STACK_BASE;
    
    #define STACK_PAGES 1
    uint64_t user_stack_top = user_stack + (STACK_PAGES * 4096ULL) - 16ULL;
    user_stack_top &= ~0xFULL;

    int segments_loaded = 0;
    int total_phdrs = ehdr->e_phnum;
    
    serial_print("ELF: Scanning program headers\n");
    
    // STEP 1: Map all pages for all loadable segments
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
            
            serial_print("ELF: start_page=0x");
            serial_print_hex(start_page);
            serial_print(" end_page=0x");
            serial_print_hex(end_page);
            serial_print(" num_pages=");
            serial_print_dec(num_pages);
            serial_print("\n");
            
            uint64_t page_flags = PT_PRESENT | PT_WRITE | PT_USER;
            
            serial_print("ELF: Page flags=0x");
            serial_print_hex(page_flags);
            serial_print("\n");
            
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
                    continue;
                }
                
                uint64_t phys = pmm_alloc_page();
                if (!phys) {
                    serial_print("ELF: Failed to allocate physical page!\n");
                    vga_print("Failed alloc\n");
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
            }
        }
    }
    
    // STEP 2: Copy data to mapped pages using HHDM (page-aware)
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
                    return;
                }
                
                uint64_t page_off = cur_virt & 0xFFF;
                size_t   chunk    = filesz - copied;
                size_t   page_rem = 4096 - page_off;
                if (chunk > page_rem) chunk = page_rem;
                
                void* hhdm_dest = (void*)(HHDM_START + phys + page_off);
                
                fast_memcpy(hhdm_dest, src + copied, chunk);
                copied += chunk;
            }
            
            // Debug: verify first bytes
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
        return;
    }
    
    // STEP 3: Allocate and initialize stack
    serial_print("ELF: Allocating stack pages\n");
    
    for (int i = 0; i < STACK_PAGES; i++) {
        uint64_t phys_stack = pmm_alloc_page();
        if (!phys_stack) {
            serial_print("Stack alloc failed\n");
            return;
        }
        uint64_t stack_virt = user_stack + (i * 4096);
        serial_print("ELF: Stack page ");
        serial_print_dec(i);
        serial_print(" virt=0x");
        serial_print_hex(stack_virt);
        serial_print(" phys=0x");
        serial_print_hex(phys_stack);
        serial_print("\n");
        vmm_map_page(stack_virt, phys_stack, PT_PRESENT | PT_WRITE | PT_USER);
        __asm__ volatile ("invlpg (%0)" : : "r" (stack_virt) : "memory");
    }
    
    serial_print("ELF: Stack clear via HHDM\n");
    uint64_t stack_phys = vmm_get_phys(user_stack);
    if (stack_phys) {
        void* stack_hhdm = (void*)(HHDM_START + stack_phys);
        uint64_t* clear_ptr = (uint64_t*)stack_hhdm;
        uint64_t count = (STACK_PAGES * 4096) / 8;
        for (uint64_t j = 0; j < count; j++) {
            clear_ptr[j] = 0;
        }
    }
    serial_print("ELF: Stack clear complete\n");
    
    // STEP 4: Optional msg check
    serial_print("ELF: Checking msg at 0x");
    serial_print_hex(phdr[1].p_vaddr);
    serial_print("\n");
    uint64_t msg_phys = vmm_get_phys(phdr[1].p_vaddr);
    if (msg_phys) {
        void* msg_hhdm = (void*)(HHDM_START + msg_phys + (phdr[1].p_vaddr & 0xFFF));
        serial_print("ELF: msg = '");
        for (int i = 0; i < 64; i++) {
            char c = ((char*)msg_hhdm)[i];
            if (c == '\n') {
                serial_print("\\n");
            } else if (c == '\0') {
                break;
            } else if (c >= 32 && c <= 126) {
                serial_putc(c);
            } else {
                serial_print("?");
            }
        }
        serial_print("'\n");
    } else {
        serial_print("ELF: ERROR - msg not mapped!\n");
    }
    
    serial_print("ELF: Entry=0x");
    serial_print_hex(entry_point);
    serial_print(" Stack=0x");
    serial_print_hex(user_stack_top);
    serial_print("\n");
    
    vga_print("ELF loaded\n");
    
    vmm_dump_page_table(entry_point);
    vmm_dump_page_table(user_stack_top);

    extern uint64_t ring3_enter(uint64_t entry, uint64_t stack,
                                uint64_t arg1, uint64_t arg2);
    
    serial_print("ELF: Calling ring3_enter\n");
    debug_rsp("ELF before ring3_enter");
    ring3_enter(entry_point, user_stack_top, 0, 0); 
}
