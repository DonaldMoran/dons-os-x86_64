#ifndef PROCESS_H
#define PROCESS_H

#include <stdint.h>
#include <stddef.h>

#define MAX_PROCESSES 32
#define PROC_NAME_LEN 32
#define PROC_STACK_SIZE 4096

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
    
    // Stacks
    uint64_t kernel_stack;     // Virtual address of kernel stack
    uint64_t user_stack;       // Virtual address of user stack
    
    // Entry point
    uint64_t entry_point;
    
    // For round-robin scheduling
    struct pcb* next;
    struct pcb* prev;
    uint64_t timeslice_ticks;
    uint64_t total_ticks;
    
    // For debugging
    uint64_t creation_time;
    uint64_t last_run_time;
} pcb_t;

// Function prototypes
void process_init(void);
pcb_t* process_create(const char* name, uint64_t entry_point, uint64_t flags);
pcb_t* process_get_current(void);
pcb_t* process_find_by_pid(uint64_t pid);
void process_dump_all(void);

// Debug function for Phase 2
void process_test_clone(void);

#endif
