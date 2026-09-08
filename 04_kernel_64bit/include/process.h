#ifndef PROCESS_H
#define PROCESS_H

#include <stdint.h>
#include <stddef.h>

#define MAX_PROCESSES 32
#define PROC_NAME_LEN 32
#define PROC_STACK_SIZE  8192   // 8KB

// Process states
typedef enum {
    PROC_STATE_UNUSED = 0,
    PROC_STATE_READY,
    PROC_STATE_RUNNING,
    PROC_STATE_BLOCKED,
    PROC_STATE_TERMINATED
} proc_state_t;

// Process Control Block
typedef struct pcb {
    uint64_t pid;
    char name[PROC_NAME_LEN];
    proc_state_t state;
    
    // Page table
    uint64_t cr3;
    
    // Entry point
    uint64_t entry_point;
    
    // Stack fields
    uint64_t kernel_stack_phys;
    uint64_t kernel_stack_virt;
    uint64_t kernel_stack_top;
    uint64_t user_stack_phys;
    uint64_t user_stack_virt;
    uint64_t user_stack_top;
    
    // ELF loading tracking - for cleanup
    uint64_t elf_base_virt;
    uint64_t elf_base_phys;
    uint64_t elf_num_pages;
    uint64_t* elf_page_list;  // Array of physical addresses allocated for ELF
    
    // ===== Context switching registers (for scheduler) =====
    uint64_t r15, r14, r13, r12, r11, r10, r9, r8;
    uint64_t rbp, rdi, rsi, rdx, rcx, rbx, rax;
    uint64_t rsp, rip;
    // =======================================================
    
    // For round-robin scheduling
    struct pcb* next;
    struct pcb* prev;
    uint64_t timeslice_ticks;
    uint64_t total_ticks;
    
    // For debugging
    uint64_t creation_time;
    uint64_t last_run_time;
    
    // Heap management (per-process)
    uint64_t brk_virt;  // Current break position for sbrk()
} pcb_t;

// Function prototypes
void process_init(void);
pcb_t* process_create(const char* name, uint64_t entry_point, uint64_t flags);
pcb_t* process_get_current(void);
void process_set_current(pcb_t* proc);
pcb_t* process_find_by_pid(uint64_t pid);
void process_dump_all(void);
void process_test_clone(void);
void process_start(pcb_t* process);
void process_destroy(pcb_t* process);
void process_cleanup_elf_pages(pcb_t* pcb);
void process_exit(void) __attribute__((noreturn));

#endif
