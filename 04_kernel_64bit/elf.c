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
    uint64_t user_stack = USER_STACK_BASE;
    
    #define STACK_PAGES 1
    uint64_t user_stack_top = user_stack + (STACK_PAGES * 4096ULL) - 16ULL;
    user_stack_top &= ~0xFULL;

    int segments_loaded = 0;
    int total_phdrs = ehdr->e_phnum;
    
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
            //uint32_t flags  = phdr[i].p_flags;
            
            serial_print("ELF: LOAD vaddr=0x");
            serial_print_hex(vaddr);
            serial_print(" memsz=0x");
            serial_print_hex(memsz);
            serial_print(" filesz=0x");
            serial_print_hex(filesz);
            serial_print(" offset=0x");
            serial_print_hex(offset);
            serial_print("\n");
            
            if (vaddr == 0x200000) {
                serial_print("ELF: Skipping segment at 0x200000 (ELF headers)\n");
                continue;
            }
            
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
            
            uint64_t page_flags = PT_PRESENT | PT_WRITE | PT_USER | PT_EXEC; // 0xF
            
            serial_print("ELF: Page flags=0x");
            serial_print_hex(page_flags);
            serial_print("\n");
            
            for (uint64_t j = 0; j < num_pages; j++) {
                uint64_t virt = start_page + (j * 4096);
                
                if (vmm_is_mapped(virt)) {
                    serial_print("ELF: Page already mapped at 0x");
                    serial_print_hex(virt);
                    serial_print("\n");
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
                __asm__ volatile ("invlpg (%0)" : : "r" (virt) : "memory");
            }
            
            const uint8_t* src = (const uint8_t*)elf_data + offset;
            uint8_t* dst       = (uint8_t*)vaddr;
            serial_print("ELF: Copying filesz=0x");
            serial_print_hex(filesz);
            serial_print(" to 0x");
            serial_print_hex((uint64_t)dst);
            serial_print("\n");
            fast_memcpy(dst, src, filesz);
            
            if (memsz > filesz) {
                uint8_t* bss_start = dst + filesz;
                size_t   bss_size  = memsz - filesz;
                serial_print("ELF: Zeroing BSS of size 0x");
                serial_print_hex(bss_size);
                serial_print(" at 0x");
                serial_print_hex((uint64_t)bss_start);
                serial_print("\n");
                __asm__ volatile ("rep stosb" : "+D"(bss_start), "+c"(bss_size) : "a"(0) : "memory");
            }
            
            segments_loaded++;
            
            if (entry_point >= vaddr && entry_point < vaddr + memsz) {
                serial_print("ELF: Entry point is in this segment!\n");
            }
        }
    }
    
    if (segments_loaded == 0) {
        vga_print("No segments\n");
        return;
    }
    
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
        vmm_map_page(stack_virt, phys_stack, PT_PRESENT | PT_WRITE | PT_USER); // 0x7
        __asm__ volatile ("invlpg (%0)" : : "r" (stack_virt) : "memory");
    }
    
    serial_print("ELF: Stack clear\n");
    uint64_t* stack = (uint64_t*)user_stack;
    uint64_t count  = (STACK_PAGES * 4096) / 8;
    __asm__ volatile ("rep stosq" : "+D"(stack), "+c"(count) : "a"(0) : "memory");
    
    serial_print("ELF: Entry=0x");
    serial_print_hex(entry_point);
    serial_print(" Stack=0x");
    serial_print_hex(user_stack_top);
    serial_print("\n");
    
    serial_print("ELF: Skipping verification, jumping to user mode\n");
    vga_print("ELF loaded\n");
    
    vmm_dump_page_table(0x400000);              // user code
    vmm_dump_page_table(0x7FFFFFE00FF0);        // exact user stack top

    
    //extern void ring3_enter(uint64_t entry, uint64_t stack, uint64_t arg1, uint64_t arg2);
    extern uint64_t ring3_enter(uint64_t entry, uint64_t stack,
                            uint64_t arg1, uint64_t arg2);

    
    
    serial_print("ELF: Calling ring3_enter\n");
    // In elf_load, right before the ring3_enter call:
    
    debug_rsp("ELF before ring3_enter");
    serial_print("ELF: Calling ring3_enter\n");
    ring3_enter(entry_point, user_stack_top, 0, 0); 
}
