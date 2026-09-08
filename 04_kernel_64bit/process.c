#include "include/process.h"
#include "include/vmm.h"
#include "include/pmm.h"
#include "include/serial.h"
#include "include/vga.h"
#include "include/scheduler.h"
#include "include/heap.h"
#include "include/user_space.h"
#include "include/elf.h"  // For elf_add_page_to_pcb
#include <string.h>

#define DBG 0   // 1 ON, 0 OFF

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
        pcb->name[0] = 'p'; pcb->name[1] = 'r'; pcb->name[2] = 'o'; pcb->name[3] = 'c'; pcb->name[4] = '_';
        uint64_t pid = pcb->pid;
        int pos = 5; char temp[16]; int len = 0;
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
    
    uint64_t current_cr3;
    asm volatile("mov %%cr3, %0" : "=r"(current_cr3));
    pcb->cr3 = vmm_clone_page_table(current_cr3); 
    
    if (entry_point != 0 && entry_point < KERNEL_BASE) {
        #define USER_STACK_PAGES 16
        #define USER_STACK_SIZE (USER_STACK_PAGES * 4096)
        
        uint64_t stack_top_anchor = 0x8000100000ULL; 
        uint64_t stack_bottom_anchor = stack_top_anchor - USER_STACK_SIZE;
        
        pcb->user_stack_virt = stack_bottom_anchor;
        
        serial_print("PROCESS: Realignment mapping user stack inside verified code segment: 0x");
        serial_print_hex(stack_top_anchor);
        serial_print("\n");
        
        for (int i = 0; i < USER_STACK_PAGES; i++) {
            uint64_t phys = pmm_alloc_page_for_elf();
            if (!phys) {
                serial_print("PROCESS: Failed to allocate user stack page\n");
                return NULL;
            }
            
            uint64_t virt = stack_bottom_anchor + (i * 4096);
            uint64_t map_flags = PT_PRESENT | PT_WRITE | PT_USER;
            map_flags &= ~(0x80ULL | 0x40ULL | 0x200ULL | 0x800ULL);
            
            vmm_map_page_in_cr3(pcb->cr3, virt, phys, map_flags);
            
            void* hhdm = (void*)(HHDM_START + phys);
            for (uint64_t j = 0; j < 4096 / 8; j++) {
                ((uint64_t*)hhdm)[j] = 0;
            }
            
            elf_add_page_to_pcb(pcb, phys);

            if (i == (USER_STACK_PAGES - 1)) {
                pcb->user_stack_phys = phys; 
            }
        }
        
        pcb->user_stack_top = stack_top_anchor - 32;
        pcb->user_stack_top &= ~0xFULL; 
        
        serial_print("PROCESS: User stack pointer finalized at RSP = 0x");
        serial_print_hex(pcb->user_stack_top);
        serial_print("\n");
    } else {
        pcb->user_stack_phys = 0;
        pcb->user_stack_virt = 0;
        pcb->user_stack_top = 0;
    }
    
    pcb->kernel_stack_virt = (uint64_t)&kernel_stack_pool[pcb->pid % MAX_PROCESSES];
    pcb->kernel_stack_phys = vmm_get_phys(pcb->kernel_stack_virt);
    pcb->kernel_stack_top = pcb->kernel_stack_virt + PROC_STACK_SIZE;
    
    ensure_hhdm_mapped(pcb->kernel_stack_phys);
    if (pcb->user_stack_phys) {
        ensure_hhdm_mapped(pcb->user_stack_phys);
    }
    
    pcb->r15 = 0; pcb->r14 = 0; pcb->r13 = 0; pcb->r12 = 0;
    pcb->r11 = 0; pcb->r10 = 0; pcb->r9 = 0;  pcb->r8 = 0;
    pcb->rbp = 0; pcb->rdi = 0; pcb->rsi = 0; pcb->rdx = 0;
    pcb->rcx = 0; pcb->rbx = 0; pcb->rax = 0;
    
    pcb->rsp = pcb->kernel_stack_top;
    pcb->rip = entry_point;
    
    pcb->next = NULL; pcb->prev = NULL;
    pcb->timeslice_ticks = 0; pcb->total_ticks = 0;
    
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

// FIX: Dual-routed process tracking charts to print directly to 
// both your background serial debugger port AND your physical VGA screen panels!
void process_dump_all(void) {
    serial_print("\n=== PROCESS LIST ===\n");
    serial_print("PID  Name                State    Entry     Kernel Stack\n");
    serial_print("---  -------------------  -------  ----------  ----------\n");

    vga_print("=== PROCESS LIST ===\n");
    vga_print("PID  Name                State    Entry       Kernel Stack\n");
    vga_print("---  -------------------  -------  ----------  ----------\n");
    
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
        
        // --- 1. TRANSMIT ROUTE: BACKGROUND SERIAL MONITOR ---
        serial_print_dec(p->pid); serial_print("  ");
        serial_print(p->name);
        int len = strlen(p->name);
        for (int j = len; j < 18; j++) serial_print(" ");
        serial_print("  ");
        serial_print(state_str); serial_print("  ");
        serial_print("0x"); serial_print_hex(p->entry_point); serial_print("  ");
        serial_print("0x"); serial_print_hex(p->kernel_stack_top);
        serial_print("\n");

        // --- 2. TRANSMIT ROUTE: VISUAL VGA SCREEN DISPLAY ---
        vga_print_dec_cur(p->pid); vga_print("  ");
        vga_print(p->name);
        for (int j = len; j < 18; j++) vga_print(" ");
        vga_print("  ");
        vga_print(state_str);
        int state_len = strlen(state_str);
        for (int j = state_len; j < 8; j++) vga_print(" ");
        vga_print(" 0x"); vga_print_hex_cur(p->entry_point);
        vga_print(" 0x"); vga_print_hex_cur(p->kernel_stack_top);
        vga_print("\n");
    }
    serial_print("=== END PROCESS LIST ===\n\n");
    vga_print("=== END PROCESS LIST ===\n");
}

void process_test_clone(void) {
    // Output headers to both display channels
    vga_print("\n=== Virtual Memory Manager Page Table Clone Test ===\n");
    serial_print("\n=== Virtual Memory Manager Page Table Clone Test ===\n");
    
    // 1. Capture the currently running hardware page directory table root
    uint64_t current_cr3;
    asm volatile("mov %%cr3, %0" : "=r"(current_cr3));
    uint64_t clean_src_cr3 = current_cr3 & ~0xFFFULL; // Mask off attributes
    
    vga_print("  1. Source Paging Table Root (CR3) : 0x"); vga_print_hex_cur(clean_src_cr3); vga_print("\n");
    serial_print("  1. Source Paging Table Root (CR3) : 0x"); serial_print_hex(clean_src_cr3); serial_print("\n");
    
    // 2. Invoke your core clone routine to generate a copy of the address space
    uint64_t new_cr3 = vmm_clone_page_table(current_cr3);
    uint64_t clean_new_cr3 = new_cr3 & ~0xFFFULL;
    
    if (new_cr3 == 0) {
        vga_print("  [ERR] Page Table Cloning routine FAILED to allocate frames!\n");
        serial_print("  [ERR] Page Table Cloning routine FAILED to allocate frames!\n");
        return;
    }
    
    vga_print("  2. Cloned Paging Table Root (CR3) : 0x"); vga_print_hex_cur(clean_new_cr3); vga_print("\n");
    serial_print("  2. Cloned Paging Table Root (CR3) : 0x"); serial_print_hex(clean_new_cr3); serial_print("\n");
    
    // 3. HARDWARE PROBE: Read into the newly allocated PML4 frame via the HHDM 
    // to check if recursive index 510 was written correctly.
    uint64_t* new_pml4 = (uint64_t*)ensure_hhdm_mapped(clean_new_cr3);
    uint64_t recursive_entry = new_pml4[RECURSIVE_PML4_INDEX];
    
    vga_print("  3. Probing Cloned PML4 Entry: 0x"); vga_print_hex_cur(recursive_entry); vga_print("\n");
    serial_print("  3. Probing Cloned PML4 Entry: 0x"); serial_print_hex(recursive_entry); serial_print("\n");
    
    if (recursive_entry & PT_PRESENT) {
        uint64_t recursive_phys = recursive_entry & ~0xFFFULL;
        if (recursive_phys == clean_new_cr3) {
            vga_print("  Status: SUCCESS! Recursive mapping links back to the new root perfectly.\n");
            serial_print("  Status: SUCCESS! Recursive mapping links back to the new root perfectly.\n");
        } else {
            vga_print("  Status: FAILED! Recursive entry points to a stale memory root.\n");
            serial_print("  Status: FAILED! Recursive entry points to a stale memory root.\n");
        }
    } else {
        vga_print("  Status: FAILED! Recursive index entry flag is marked NOT PRESENT.\n");
        serial_print("  Status: FAILED! Recursive index entry flag is marked NOT PRESENT.\n");
    }

    // 4. CLEANUP SANBOX PASS: Free the testing PML4 root page frame from the PMM
    // to prevent memory leaks every time you type vmmclone
    extern void pmm_free_page(uint64_t phys_addr);
    pmm_free_page(clean_new_cr3);
    
    vga_print("  4. Testing sandbox page tables released back to PMM cleanly.\n");
    serial_print("  4. Testing sandbox page tables released back to PMM cleanly.\n");
}


void process_start(pcb_t* process) {
    if (!process) return;
    scheduler_switch_to(process);
}

void process_cleanup_elf_pages(pcb_t* pcb) {
    if (!pcb || !pcb->elf_page_list) return;
    for (uint64_t i = 0; i < pcb->elf_num_pages; i++) {
        uint64_t phys = pcb->elf_page_list[i];
        if (phys) pmm_free_page(phys);
    }
    kfree(pcb->elf_page_list);
    pcb->elf_page_list = NULL;
    pcb->elf_num_pages = 0;
}

void process_destroy(pcb_t* pcb) {
    if (!pcb || pcb->state == PROC_STATE_UNUSED) return;
    process_cleanup_elf_pages(pcb);
    pcb->user_stack_phys = 0;
    pcb->state = PROC_STATE_UNUSED;
    pcb->pid = 0;
    process_count--;
    scheduler_ready_queue_remove(pcb);
    if (current_process == pcb) current_process = NULL;
    serial_print("PROCESS: Process destroyed\n");
}
