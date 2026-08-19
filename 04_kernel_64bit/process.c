#include "include/process.h"
#include "include/vmm.h"
#include "include/pmm.h"
#include "include/serial.h"
#include "include/vga.h"
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

void process_init(void) {
    serial_print("PROCESS: Initializing...\n");
    vga_print("PROCESS: Initializing...\n");
    
    // Clear PCB pool
    memset(pcb_pool, 0, sizeof(pcb_pool));
    
    // Create idle process (PID 0)
    pcb_t* idle = process_create("idle", 0, 0);
    if (idle) {
        idle->state = PROC_STATE_READY;
        current_process = idle;
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
    
    // Set name
    if (name) {
        strncpy(pcb->name, name, PROC_NAME_LEN - 1);
        pcb->name[PROC_NAME_LEN - 1] = '\0';
    } else {
        // Simple numeric name
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
    
    // ============================================================
    // Step 1: Use kernel page table (temporary - no isolation)
    // This avoids PMM corruption from page table allocation
    // ============================================================
    uint64_t current_cr3;
    asm volatile("mov %%cr3, %0" : "=r"(current_cr3));
    pcb->cr3 = current_cr3;  // Use kernel's page table
    
    // ============================================================
    // Step 2: Allocate user stack
    // ============================================================
    pcb->user_stack_phys = pmm_alloc_page();
    if (!pcb->user_stack_phys) {
        serial_print("PROCESS: Failed to allocate user stack!\n");
        return NULL;
    }
    
    // Map user stack in the process's page table
    pcb->user_stack_virt = USER_STACK_BASE;
    vmm_map_page_in_cr3(pcb->cr3, pcb->user_stack_virt, 
                        pcb->user_stack_phys, 
                        PT_PRESENT | PT_WRITE | PT_USER);
    
    // Stack grows down: top is at the highest address
    pcb->user_stack_top = pcb->user_stack_virt + PROC_STACK_SIZE - 16;
    pcb->user_stack_top &= ~0xFULL;  // 16-byte alignment
    
    // ============================================================
    // Step 3: Use static kernel stack pool
    // ============================================================
    pcb->kernel_stack_virt = (uint64_t)&kernel_stack_pool[pcb->pid][0];
    pcb->kernel_stack_phys = vmm_get_phys(pcb->kernel_stack_virt);
    pcb->kernel_stack_top = pcb->kernel_stack_virt + PROC_STACK_SIZE;
    
    // Ensure stacks are mapped in HHDM for debugging
    ensure_hhdm_mapped(pcb->kernel_stack_phys);
    ensure_hhdm_mapped(pcb->user_stack_phys);
    
    // ============================================================
    // Step 4: Build iretq frame on kernel stack
    // ============================================================
    uint64_t* frame = (uint64_t*)(pcb->kernel_stack_virt + PROC_STACK_SIZE - 8*8);
    
    // SS (user data selector) - without RPL
    // frame[7] = 0x28;  // GDT_USER_DATA (0x28)
    frame[7] = 0x20;
    
    // RSP (user stack pointer)
    frame[6] = pcb->user_stack_top;
    
    // RFLAGS (IF=1, IOPL=0)
    frame[5] = 0x202;  // Bit 9 = IF (interrupts enabled)
    
    // CS (user code selector) - without RPL
    // frame[4] = 0x30;  // GDT_USER_CODE (0x30)
    frame[4] = 0x18;
    
    // RIP (entry point)
    frame[3] = entry_point;
    
    // Zero out other registers
    frame[2] = 0;  // RAX
    frame[1] = 0;  // RBX
    frame[0] = 0;  // RCX
    
    // The kernel stack pointer should point to the frame (top of stack)
    pcb->kernel_stack_top = (uint64_t)frame;
    
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
    serial_print("\n");
    
    return pcb;
}

pcb_t* process_get_current(void) {
    return current_process;
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
    serial_print("PROCESS: Testing page table cloning...\n");
    vga_print("\n=== Page Table Clone Test ===\n");
    
    // Get current CR3
    uint64_t current_cr3;
    asm volatile("mov %%cr3, %0" : "=r"(current_cr3));
    
    serial_print("Current CR3: 0x");
    serial_print_hex(current_cr3);
    serial_print("\n");
    vga_print("Current CR3: 0x");
    vga_print_hex_cur(current_cr3);
    vga_print("\n");
    
    // Clone the page table
    uint64_t new_cr3 = vmm_clone_page_table(current_cr3);
    
    if (new_cr3 == 0) {
        serial_print("PROCESS: Clone FAILED!\n");
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
    
    // Verify the recursive entry using HHDM (safe, doesn't switch CR3)
    uint64_t* new_pml4 = (uint64_t*)ensure_hhdm_mapped(new_cr3);
    uint64_t recursive_entry = new_pml4[RECURSIVE_PML4_INDEX];
    
    serial_print("Recursive entry: 0x");
    serial_print_hex(recursive_entry);
    serial_print("\n");
    vga_print("Recursive entry: 0x");
    vga_print_hex_cur(recursive_entry);
    vga_print("\n");
    
    if (recursive_entry & PT_PRESENT) {
        uint64_t recursive_phys = recursive_entry & ~0xFFF;
        if (recursive_phys == new_cr3) {
            serial_print("Recursive mapping is CORRECT!\n");
            vga_print("Recursive mapping is CORRECT!\n");
        } else {
            serial_print("Recursive mapping points to wrong address!\n");
            serial_print("Expected: 0x");
            serial_print_hex(new_cr3);
            serial_print(" Got: 0x");
            serial_print_hex(recursive_phys);
            serial_print("\n");
            vga_print("Recursive mapping is INCORRECT!\n");
        }
    } else {
        serial_print("Recursive mapping is NOT present!\n");
        vga_print("Recursive mapping is NOT present!\n");
    }
    
    serial_print("PROCESS: Clone test complete.\n");
    vga_print("Clone test complete!\n");
    vga_print("> ");
}

// ============================================================
// Process Start - Call entry function directly (kernel mode)
// ============================================================
void process_start(pcb_t* process) {
    if (!process) {
        serial_print("PROCESS: Cannot start NULL process!\n");
        return;
    }
    
    serial_print("PROCESS: Starting process ");
    serial_print_dec(process->pid);
    serial_print(" (");
    serial_print(process->name);
    serial_print(")\n");
    serial_print("PROCESS: CR3=0x");
    serial_print_hex(process->cr3);
    serial_print(" Kernel stack top=0x");
    serial_print_hex(process->kernel_stack_top);
    serial_print("\n");
    
    // Call the entry function directly (kernel mode)
    void (*entry)(void) = (void (*)(void))process->entry_point;
    entry();
    
    serial_print("PROCESS: Entry function returned\n");
    process_destroy(process);
}

// ============================================================
// Process Destroy - Clean up resources
// ============================================================
void process_destroy(pcb_t* pcb) {
    if (!pcb) return;
    if (pcb->state == PROC_STATE_UNUSED) return;
    
    serial_print("PROCESS: Destroying process ");
    serial_print_dec(pcb->pid);
    serial_print(" (");
    serial_print(pcb->name);
    serial_print(")\n");
    
    // Free user stack
    if (pcb->user_stack_phys) {
        pmm_free_page(pcb->user_stack_phys);
        serial_print("PROCESS: Freed user stack 0x");
        serial_print_hex(pcb->user_stack_phys);
        serial_print("\n");
    }
    
    // Note: With kernel page table (no clone), we don't need to free page tables
    
    // Mark PCB as unused
    pcb->state = PROC_STATE_UNUSED;
    pcb->pid = 0;
    process_count--;
    
    serial_print("PROCESS: Process destroyed\n");
}
