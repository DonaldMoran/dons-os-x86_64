#include "include/process.h"
#include "include/vmm.h"
#include "include/pmm.h"
#include "include/serial.h"
#include "include/vga.h"
#include "include/scheduler.h"
#include "include/heap.h"
#include "include/user_space.h"
#include "include/elf.h"  // Add this for elf_add_page_to_pcb
#include <string.h>

// Static kernel stack pool (already mapped in kernel's page table)
static uint8_t kernel_stack_pool[MAX_PROCESSES][PROC_STACK_SIZE] __attribute__((aligned(16)));

static pcb_t pcb_pool[MAX_PROCESSES];
static pcb_t* current_process = NULL;
static uint64_t next_pid = 1;
static uint64_t process_count = 0;

// Forward declarations
static pcb_t* get_free_pcb(void);
static void process_initialize_pcb(pcb_t* pcb);

// Kernel base address for user/kernel detection
#define KERNEL_BASE 0xFFFFFFFF80000000ULL

void process_init(void) {
    serial_print("PROCESS: Initializing...\n");
    vga_print("PROCESS: Initializing...\n");
    
    memset(pcb_pool, 0, sizeof(pcb_pool));
    
    pcb_t* idle = process_create("idle", 0, 0);
    if (idle) {
        idle->state = PROC_STATE_READY;
        current_process = idle;
        // Sync with scheduler's current process
        scheduler_set_current(idle);
    }
    
    serial_print("PROCESS: Initialization complete. ");
    serial_print_dec(process_count);
    serial_print(" processes ready.\n");
    vga_print("PROCESS: Initialization complete.\n");
}

static pcb_t* get_free_pcb(void) {
    for (int i = 0; i < MAX_PROCESSES; i++) {
        if (pcb_pool[i].state == PROC_STATE_UNUSED) {
            return &pcb_pool[i];
        }
    }
    serial_print("PROCESS: No free PCB slots!\n");
    return NULL;
}

static void process_initialize_pcb(pcb_t* pcb) {
    memset(pcb, 0, sizeof(pcb_t));
    pcb->pid = next_pid++;
    pcb->state = PROC_STATE_UNUSED;
    pcb->elf_page_list = NULL;
    pcb->elf_num_pages = 0;
    process_count++;
}

pcb_t* process_create(const char* name, uint64_t entry_point, uint64_t flags) {
    (void)flags;
    
    pcb_t* pcb = get_free_pcb();
    if (!pcb) {
        serial_print("PROCESS: Failed to allocate PCB for ");
        if (name) serial_print(name);
        else serial_print("unnamed");
        serial_print("\n");
        return NULL;
    }
    
    process_initialize_pcb(pcb);
    
    if (name) {
        strncpy(pcb->name, name, PROC_NAME_LEN - 1);
        pcb->name[PROC_NAME_LEN - 1] = '\0';
    } else {
        pcb->name[0] = 'p';
        pcb->name[1] = 'r';
        pcb->name[2] = 'o';
        pcb->name[3] = 'c';
        pcb->name[4] = '_';
        uint64_t pid = pcb->pid;
        int pos = 5;
        char temp[16];
        int len = 0;
        if (pid == 0) temp[len++] = '0';
        else {
            while (pid > 0) {
                temp[len++] = '0' + (pid % 10);
                pid /= 10;
            }
        }
        for (int i = 0; i < len; i++) {
            pcb->name[pos + i] = temp[len - 1 - i];
        }
        pcb->name[pos + len] = '\0';
    }
    
    pcb->entry_point = entry_point;
    pcb->state = PROC_STATE_READY;
    
    // Use kernel page table (temporary - no isolation)
    uint64_t current_cr3;
    asm volatile("mov %%cr3, %0" : "=r"(current_cr3));
    pcb->cr3 = current_cr3;
    
    // Determine if this is a user process or kernel process
    if (entry_point != 0 && entry_point < KERNEL_BASE) {
        // User process - allocate user stack (8KB = 2 pages)
        #define USER_STACK_PAGES 2
        #define USER_STACK_SIZE (USER_STACK_PAGES * 4096)
        
        pcb->user_stack_virt = USER_STACK_BASE;
        
        serial_print("PROCESS: Allocating user stack (");
        serial_print_dec(USER_STACK_PAGES);
        serial_print(" pages) at virt=0x");
        serial_print_hex(pcb->user_stack_virt);
        serial_print("\n");
        
        // Allocate and map each stack page
        for (int i = 0; i < USER_STACK_PAGES; i++) {
            uint64_t phys = pmm_alloc_page();
            if (!phys) {
                serial_print("PROCESS: Failed to allocate user stack page ");
                serial_print_dec(i);
                serial_print("\n");
                // Clean up previously allocated pages
                for (int j = 0; j < i; j++) {
                    uint64_t virt = pcb->user_stack_virt + (j * 4096);
                    uint64_t p = vmm_get_phys(virt);
                    if (p) {
                        vmm_unmap_page(virt);
                        pmm_free_page(p);
                    }
                }
                return NULL;
            }
            
            uint64_t virt = pcb->user_stack_virt + (i * 4096);
            uint64_t map_flags = PT_PRESENT | PT_WRITE | PT_USER;
            // Clear reserved bits
            map_flags &= ~(0x80ULL | 0x40ULL | 0x200ULL | 0x800ULL);
            
            serial_print("PROCESS: Mapping stack page ");
            serial_print_dec(i);
            serial_print(": virt=0x");
            serial_print_hex(virt);
            serial_print(" phys=0x");
            serial_print_hex(phys);
            serial_print("\n");
            
            vmm_map_page(virt, phys, map_flags);
            __asm__ volatile ("invlpg (%0)" : : "r" (virt) : "memory");
            
            // Zero the page via HHDM
            void* hhdm = (void*)(HHDM_START + phys);
            for (uint64_t j = 0; j < 4096 / 8; j++) {
                ((uint64_t*)hhdm)[j] = 0;
            }
            
            // Track the page for cleanup (using elf_page_list)
            elf_add_page_to_pcb(pcb, phys);
        }
        
        pcb->user_stack_top = pcb->user_stack_virt + USER_STACK_SIZE - 16;
        pcb->user_stack_top &= ~0xFULL;
        
        serial_print("PROCESS: User stack top=0x");
        serial_print_hex(pcb->user_stack_top);
        serial_print("\n");
    } else {
        // Kernel process - no user stack needed
        pcb->user_stack_phys = 0;
        pcb->user_stack_virt = 0;
        pcb->user_stack_top = 0;
    }
    
    // Use static kernel stack pool
    pcb->kernel_stack_virt = (uint64_t)&kernel_stack_pool[pcb->pid % MAX_PROCESSES][0];
    pcb->kernel_stack_phys = vmm_get_phys(pcb->kernel_stack_virt);
    pcb->kernel_stack_top = pcb->kernel_stack_virt + PROC_STACK_SIZE;
    
    ensure_hhdm_mapped(pcb->kernel_stack_phys);
    if (pcb->user_stack_phys) {
        ensure_hhdm_mapped(pcb->user_stack_phys);
    }
    
    // Initialize context registers for scheduler
    pcb->r15 = 0; pcb->r14 = 0; pcb->r13 = 0; pcb->r12 = 0;
    pcb->r11 = 0; pcb->r10 = 0; pcb->r9 = 0;  pcb->r8 = 0;
    pcb->rbp = 0; pcb->rdi = 0; pcb->rsi = 0; pcb->rdx = 0;
    pcb->rcx = 0; pcb->rbx = 0; pcb->rax = 0;
    
    // Set initial stack pointer and instruction pointer
    pcb->rsp = pcb->kernel_stack_top;
    pcb->rip = entry_point;
    
    // Initialize scheduling fields
    pcb->next = NULL;
    pcb->prev = NULL;
    pcb->timeslice_ticks = 0;
    pcb->total_ticks = 0;
    
    serial_print("PROCESS: Created process ");
    serial_print_dec(pcb->pid);
    serial_print(" (");
    serial_print(pcb->name);
    serial_print(") entry=0x");
    serial_print_hex(pcb->entry_point);
    serial_print(" cr3=0x");
    serial_print_hex(pcb->cr3);
    serial_print(" kernel_stack=0x");
    serial_print_hex(pcb->kernel_stack_top);
    if (pcb->user_stack_top) {
        serial_print(" user_stack_top=0x");
        serial_print_hex(pcb->user_stack_top);
    }
    serial_print("\n");
    
    // Add to ready queue
    scheduler_ready_queue_add(pcb);
    
    return pcb;
}

pcb_t* process_get_current(void) {
    return current_process;
}

void process_set_current(pcb_t* proc) {
    current_process = proc;
}

pcb_t* process_find_by_pid(uint64_t pid) {
    for (int i = 0; i < MAX_PROCESSES; i++) {
        if (pcb_pool[i].pid == pid && pcb_pool[i].state != PROC_STATE_UNUSED) {
            return &pcb_pool[i];
        }
    }
    return NULL;
}

void process_dump_all(void) {
    serial_print("\n=== PROCESS LIST ===\n");
    serial_print("PID  Name                State    Entry     Kernel Stack\n");
    serial_print("---  -------------------  -------  ----------  ----------\n");
    
    for (int i = 0; i < MAX_PROCESSES; i++) {
        pcb_t* p = &pcb_pool[i];
        if (p->state == PROC_STATE_UNUSED) continue;
        
        const char* state_str;
        switch (p->state) {
            case PROC_STATE_READY: state_str = "READY"; break;
            case PROC_STATE_RUNNING: state_str = "RUNNING"; break;
            case PROC_STATE_BLOCKED: state_str = "BLOCKED"; break;
            case PROC_STATE_TERMINATED: state_str = "TERMINATED"; break;
            default: state_str = "UNKNOWN"; break;
        }
        
        serial_print_dec(p->pid);
        serial_print("  ");
        serial_print(p->name);
        int len = strlen(p->name);
        for (int j = len; j < 18; j++) serial_print(" ");
        serial_print("  ");
        serial_print(state_str);
        serial_print("  ");
        serial_print("0x");
        serial_print_hex(p->entry_point);
        serial_print("  ");
        serial_print("0x");
        serial_print_hex(p->kernel_stack_top);
        serial_print("\n");
    }
    serial_print("=== END PROCESS LIST ===\n\n");
}

void process_test_clone(void) {
    serial_print("DEBUG: process_test_clone() entered\n");
    
    // Get current CR3
    uint64_t current_cr3;
    asm volatile("mov %%cr3, %0" : "=r"(current_cr3));
    serial_print("DEBUG: current_cr3 = 0x");
    serial_print_hex(current_cr3);
    serial_print("\n");
    
    serial_print("PROCESS: Testing page table cloning...\n");
    vga_print("\n=== Page Table Clone Test ===\n");
    
    serial_print("DEBUG: Calling vmm_clone_page_table()\n");
    uint64_t new_cr3 = vmm_clone_page_table(current_cr3);
    serial_print("DEBUG: vmm_clone_page_table() returned 0x");
    serial_print_hex(new_cr3);
    serial_print("\n");
    
    if (new_cr3 == 0) {
        serial_print("DEBUG: Clone FAILED - new_cr3 is 0\n");
        vga_print("Clone FAILED!\n");
        vga_print("> ");
        return;
    }
    
    serial_print("New CR3: 0x");
    serial_print_hex(new_cr3);
    serial_print("\n");
    vga_print("New CR3: 0x");
    vga_print_hex_cur(new_cr3);
    vga_print("\n");
    
    serial_print("DEBUG: Verifying recursive entry\n");
    uint64_t* new_pml4 = (uint64_t*)ensure_hhdm_mapped(new_cr3);
    uint64_t recursive_entry = new_pml4[RECURSIVE_PML4_INDEX];
    serial_print("DEBUG: recursive_entry = 0x");
    serial_print_hex(recursive_entry);
    serial_print("\n");
    
    serial_print("Recursive entry: 0x");
    serial_print_hex(recursive_entry);
    serial_print("\n");
    vga_print("Recursive entry: 0x");
    vga_print_hex_cur(recursive_entry);
    vga_print("\n");
    
    if (recursive_entry & PT_PRESENT) {
        uint64_t recursive_phys = recursive_entry & ~0xFFF;
        if (recursive_phys == new_cr3) {
            serial_print("DEBUG: Recursive mapping is CORRECT!\n");
            vga_print("Recursive mapping is CORRECT!\n");
        } else {
            serial_print("DEBUG: Recursive mapping points to wrong address!\n");
            vga_print("Recursive mapping is INCORRECT!\n");
        }
    } else {
        serial_print("DEBUG: Recursive mapping is NOT present!\n");
        vga_print("Recursive mapping is NOT present!\n");
    }
    
    serial_print("DEBUG: process_test_clone() complete\n");
    vga_print("Clone test complete!\n");
}

void process_start(pcb_t* process) {
    if (!process) return;
    scheduler_switch_to(process);
}

void process_cleanup_elf_pages(pcb_t* pcb) {
    if (!pcb) return;
    if (!pcb->elf_page_list) return;
    
    serial_print("PROCESS: Cleaning up ELF pages for process ");
    serial_print_dec(pcb->pid);
    serial_print(" (");
    serial_print(pcb->name);
    serial_print(") - ");
    serial_print_dec(pcb->elf_num_pages);
    serial_print(" pages\n");
    
    for (uint64_t i = 0; i < pcb->elf_num_pages; i++) {
        uint64_t phys = pcb->elf_page_list[i];
        if (phys) {
            uint64_t virt = pcb->elf_base_virt + (i * 4096);
            serial_print("  Unmapping virt=0x");
            serial_print_hex(virt);
            serial_print(" phys=0x");
            serial_print_hex(phys);
            serial_print("\n");
            
            vmm_unmap_page(virt);
            __asm__ volatile ("invlpg (%0)" : : "r" (virt) : "memory");
            pmm_free_page(phys);
        }
    }
    
    kfree(pcb->elf_page_list);
    pcb->elf_page_list = NULL;
    pcb->elf_num_pages = 0;
    pcb->elf_base_virt = 0;
    pcb->elf_base_phys = 0;
}

void process_destroy(pcb_t* pcb) {
    if (!pcb) return;
    if (pcb->state == PROC_STATE_UNUSED) return;
    
    serial_print("PROCESS: Destroying process ");
    serial_print_dec(pcb->pid);
    serial_print(" (");
    serial_print(pcb->name);
    serial_print(")\n");
    
    process_cleanup_elf_pages(pcb);
    
    if (pcb->user_stack_phys) {
        serial_print("  Freeing user stack phys=0x");
        serial_print_hex(pcb->user_stack_phys);
        serial_print("\n");
        vmm_unmap_page(pcb->user_stack_virt);
        __asm__ volatile ("invlpg (%0)" : : "r" (pcb->user_stack_virt) : "memory");
        pmm_free_page(pcb->user_stack_phys);
        pcb->user_stack_phys = 0;
    }
    
    pcb->state = PROC_STATE_UNUSED;
    pcb->pid = 0;
    process_count--;
    
    scheduler_ready_queue_remove(pcb);
    
    // If this process was the current process, clear it
    if (current_process == pcb) {
        current_process = NULL;
    }
    
    serial_print("PROCESS: Process destroyed\n");
}
