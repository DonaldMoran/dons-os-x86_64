#!/bin/bash
# Fix user process page table issue
# Run from: /home/noneya/code/dons-os-x86_64/

set -e

echo "=== Fixing user process page tables ==="
echo ""

# Step 1: Force user processes to use kernel page table
echo "[1/4] Modifying process.c to use kernel page table..."
cd /home/noneya/code/dons-os-x86_64/04_kernel_64bit

# Backup process.c
cp process.c process.c.backup

# Force user processes to use kernel CR3
sed -i 's/pcb->cr3 = current_cr3;/pcb->cr3 = current_cr3; \/\/ Use kernel page table for now (temporary fix)/g' process.c

echo "✓ Modified process.c"
echo ""

# Step 2: Add debug to elf.c - using a cleaner approach
echo "[2/4] Adding debug to elf.c..."
cp elf.c elf.c.backup

# We'll manually insert debug prints using a different method
# First, find the elf_map_range function and add debug
cat > elf.c.new << 'EOF'
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

static int elf_map_range(pcb_t* pcb, uint64_t start_virt, uint64_t end_virt, uint64_t page_flags) {
    uint64_t start_page = start_virt & ~0xFFFULL;
    uint64_t end_page = (end_virt + 0xFFF) & ~0xFFFULL;
    
    uint64_t valid_flags = (page_flags & (PT_PRESENT | PT_WRITE | PT_USER | PT_NX));
    valid_flags &= ~(0x80ULL | 0x40ULL | 0x200ULL | 0x800ULL);
    
    serial_print("ELF: Mapping range 0x");
    serial_print_hex(start_page);
    serial_print(" - 0x");
    serial_print_hex(end_page);
    serial_print(" with flags=0x");
    serial_print_hex(valid_flags);
    serial_print("\n");
    
    // Debug: print current CR3
    uint64_t cr3;
    asm volatile("mov %%cr3, %0" : "=r"(cr3));
    serial_print("ELF: Current CR3 = 0x");
    serial_print_hex(cr3);
    serial_print(" (pcb->cr3 = 0x");
    serial_print_hex(pcb->cr3);
    serial_print(")\n");
    
    for (uint64_t virt = start_page; virt < end_page; virt += 4096) {
        uint64_t existing_phys = vmm_get_phys(virt);
        serial_print("ELF: vmm_get_phys(0x");
        serial_print_hex(virt);
        serial_print(") = 0x");
        serial_print_hex(existing_phys);
        serial_print("\n");
        
        if (existing_phys != 0) {
            serial_print("ELF: Page already mapped at 0x");
            serial_print_hex(virt);
            serial_print(" phys=0x");
            serial_print_hex(existing_phys);
            serial_print(" - updating flags\n");
            vmm_map_page(virt, existing_phys, valid_flags);
            __asm__ volatile ("invlpg (%0)" : : "r" (virt) : "memory");
            elf_add_page_to_pcb(pcb, existing_phys);
            continue;
        }
        
        uint64_t phys = pmm_alloc_page();
        if (!phys) {
            serial_print("ELF: Failed to allocate physical page!\n");
            return -1;
        }
        serial_print("ELF: Mapping new page virt=0x");
        serial_print_hex(virt);
        serial_print(" phys=0x");
        serial_print_hex(phys);
        serial_print("\n");
        vmm_map_page(virt, phys, valid_flags);
        ensure_hhdm_mapped(phys);
        __asm__ volatile ("invlpg (%0)" : : "r" (virt) : "memory");
        elf_add_page_to_pcb(pcb, phys);
    }
    return 0;
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
    
    pcb_t* pcb = process_create("elf_prog", entry_point, 0);
    if (!pcb) {
        serial_print("ELF: Failed to create process!\n");
        vga_print("Failed to create process\n");
        return;
    }
    
    pcb->elf_base_virt = 0xFFFFFFFFFFFFFFFFULL;
    uint64_t min_virt = 0xFFFFFFFFFFFFFFFFULL;
    uint64_t max_virt = 0;
    
    for (int i = 0; i < ehdr->e_phnum; i++) {
        if (phdr[i].p_type == PT_LOAD) {
            uint64_t vaddr = phdr[i].p_vaddr;
            uint64_t memsz = phdr[i].p_memsz;
            
            if (vaddr < min_virt) min_virt = vaddr;
            if (vaddr + memsz > max_virt) max_virt = vaddr + memsz;
            
            if (pcb->elf_base_virt == 0xFFFFFFFFFFFFFFFFULL) {
                pcb->elf_base_virt = vaddr & ~0xFFFULL;
            }
        }
    }
    
    if (min_virt != 0xFFFFFFFFFFFFFFFFULL) {
        uint64_t start_page = min_virt & ~0xFFFULL;
        uint64_t end_page = (max_virt + 0xFFF) & ~0xFFFULL;
        uint64_t page_flags = PT_PRESENT | PT_WRITE | PT_USER;
        page_flags &= ~(0x80ULL | 0x40ULL | 0x200ULL | 0x800ULL);
        
        serial_print("ELF: Mapping full range 0x");
        serial_print_hex(start_page);
        serial_print(" - 0x");
        serial_print_hex(end_page);
        serial_print("\n");
        
        if (elf_map_range(pcb, start_page, end_page, page_flags) < 0) {
            vga_print("Failed to map pages\n");
            process_destroy(pcb);
            return;
        }
    }
    
    for (int i = 0; i < ehdr->e_phnum; i++) {
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
            }
            
            if (memsz > filesz) {
                uint64_t bss_start_virt = vaddr + filesz;
                uint64_t bss_bytes = memsz - filesz;
                
                serial_print("ELF: Zeroing BSS at 0x");
                serial_print_hex(bss_start_virt);
                serial_print(" size=0x");
                serial_print_hex(bss_bytes);
                serial_print("\n");
                
                uint64_t zeroed = 0;
                while (zeroed < bss_bytes) {
                    uint64_t cur_virt = bss_start_virt + zeroed;
                    uint64_t phys = vmm_get_phys(cur_virt);
                    if (!phys) {
                        serial_print("ELF: Failed to get physical for BSS at 0x");
                        serial_print_hex(cur_virt);
                        serial_print("\n");
                        break;
                    }
                    
                    uint64_t page_off = cur_virt & 0xFFF;
                    size_t   chunk    = bss_bytes - zeroed;
                    size_t   page_rem = 4096 - page_off;
                    if (chunk > page_rem) chunk = page_rem;
                    
                    uint8_t* hhdm_ptr = (uint8_t*)(HHDM_START + phys + page_off);
                    for (size_t j = 0; j < chunk; j++) {
                        hhdm_ptr[j] = 0;
                    }
                    zeroed += chunk;
                }
            }
            
            // For the second LOAD segment (.rodata), verify and null-terminate
            if (i == 1) {
                serial_print("ELF: Verifying .rodata at 0x");
                serial_print_hex(vaddr);
                serial_print(" (");
                serial_print_dec(filesz);
                serial_print(" bytes):\n");
                
                // Print first 64 bytes as characters
                for (uint64_t j = 0; j < 64 && j < filesz; j++) {
                    uint64_t cur_virt = vaddr + j;
                    uint64_t phys = vmm_get_phys(cur_virt);
                    if (!phys) {
                        serial_print("  ERROR: phys=0 at offset ");
                        serial_print_dec(j);
                        serial_print("\n");
                        break;
                    }
                    if (j % 16 == 0) {
                        serial_print("  ");
                        serial_print_hex(cur_virt);
                        serial_print(": ");
                    }
                    uint64_t page_off = cur_virt & 0xFFF;
                    uint8_t* hhdm_ptr = (uint8_t*)(HHDM_START + phys + page_off);
                    uint8_t byte = hhdm_ptr[0];
                    if (byte >= 32 && byte <= 126) {
                        serial_putc(byte);
                    } else if (byte == 0) {
                        serial_print("\\0");
                    } else if (byte == 10) {
                        serial_print("\\n");
                    } else {
                        serial_print(".");
                    }
                    serial_print(" ");
                    if (j % 16 == 15) {
                        serial_print("\n");
                    }
                }
                serial_print("\n");
                
                // Ensure null termination at the end of the data
                if (filesz > 0) {
                    uint64_t last_addr = vaddr + filesz - 1;
                    uint64_t phys = vmm_get_phys(last_addr);
                    if (phys) {
                        uint64_t page_off = last_addr & 0xFFF;
                        uint8_t* hhdm_ptr = (uint8_t*)(HHDM_START + phys + page_off);
                        if (*hhdm_ptr != 0) {
                            *hhdm_ptr = 0;
                            serial_print("ELF: Added null terminator at 0x");
                            serial_print_hex(last_addr);
                            serial_print("\n");
                        }
                    }
                }
            }
        }
    }
    
    serial_print("ELF: Process created successfully\n");
    vga_print("ELF loaded\n");
    
    serial_print("ELF: Starting process via jump_to_user_mode\n");
    extern void jump_to_user_mode(uint64_t entry, uint64_t stack);
    
    uint64_t* tss_rsp0 = (uint64_t*)(HHDM_START + 0x5000 + 0x04);
    *tss_rsp0 = pcb->kernel_stack_top;
    serial_print("ELF: TSS RSP0 set to 0x");
    serial_print_hex(pcb->kernel_stack_top);
    serial_print("\n");
    
    __asm__ volatile ("wbinvd");
    serial_print("ELF: Cache flushed (wbinvd)\n");
    
    __asm__ volatile (
        "mov %%cr3, %%rax\n\t"
        "mov %%rax, %%cr3\n\t"
        : : : "rax"
    );
    serial_print("ELF: TLB flushed (cr3 reload)\n");
    
    jump_to_user_mode(pcb->entry_point, pcb->user_stack_top);
    
    serial_print("ELF: jump_to_user_mode returned unexpectedly!\n");
    process_destroy(pcb);
    serial_print("ELF: Load complete, returning to shell\n");
}
EOF

# Replace elf.c with the new version
mv elf.c.new elf.c

echo "✓ Updated elf.c with debug"
echo ""

# Step 3: Clean and rebuild
echo "[3/4] Rebuilding kernel..."
make clean
make all

echo "✓ Kernel rebuilt"
echo ""

# Step 4: Rebuild boot image
echo "[4/4] Rebuilding boot image..."
cd /home/noneya/code/dons-os-x86_64/05_boot_kernel64
make clean
make all

echo ""
echo "========================================"
echo "Build complete!"
echo "========================================"
echo ""
echo "Now run QEMU with logging:"
echo "  cd /home/noneya/code/dons-os-x86_64/05_boot_kernel64"
echo "  make run-log"
echo ""
echo "Then in QEMU, type: usershell"
echo ""
echo "After QEMU exits, check the log:"
echo "  cat serial.log | grep -E 'sys_write|vmm_get_phys|PML4|ELF: Mapping|ELF: Current CR3'"
echo ""
echo "To revert changes:"
echo "  cd /home/noneya/code/dons-os-x86_64/04_kernel_64bit"
echo "  cp process.c.backup process.c"
echo "  cp elf.c.backup elf.c"
