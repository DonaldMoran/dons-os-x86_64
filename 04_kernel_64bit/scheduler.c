#include "include/scheduler.h"
#include "include/serial.h"
#include "include/vga.h"

// Ready queue head and tail
static pcb_t* ready_queue_head = NULL;
static pcb_t* ready_queue_tail = NULL;
static pcb_t* current_process = NULL;

// Scheduler statistics
static uint64_t schedule_count = 0;
static uint64_t yield_count = 0;

extern void context_switch(pcb_t* prev, pcb_t* next);
extern void kmain_shell_loop(void);

// ============================================================
// Ready Queue Functions
// ============================================================

void scheduler_ready_queue_add(pcb_t* process) {
    if (!process) return;
    if (process->state == PROC_STATE_RUNNING) {
        process->state = PROC_STATE_READY;
    }
    
    process->next = NULL;
    process->prev = ready_queue_tail;
    
    if (ready_queue_tail) {
        ready_queue_tail->next = process;
    } else {
        ready_queue_head = process;
    }
    ready_queue_tail = process;
}

void scheduler_ready_queue_remove(pcb_t* process) {
    if (!process) return;
    
    if (process->prev) {
        process->prev->next = process->next;
    } else {
        ready_queue_head = process->next;
    }
    
    if (process->next) {
        process->next->prev = process->prev;
    } else {
        ready_queue_tail = process->prev;
    }
    
    process->next = NULL;
    process->prev = NULL;
}

pcb_t* scheduler_ready_queue_next(void) {
    if (!ready_queue_head) return NULL;
    
    pcb_t* next = ready_queue_head;
    ready_queue_head = next->next;
    if (ready_queue_head) {
        ready_queue_head->prev = NULL;
    } else {
        ready_queue_tail = NULL;
    }
    next->next = NULL;
    next->prev = NULL;
    return next;
}

pcb_t* scheduler_ready_queue_peek(void) {
    return ready_queue_head;
}

int scheduler_ready_queue_empty(void) {
    return ready_queue_head == NULL;
}

// ============================================================
// Process Exit - Called when a process finishes
// ============================================================
void process_exit(void) {
    if (!current_process) {
        serial_print("PROCESS: exit called with no current process\n");
        return;
    }
    
    serial_print("PROCESS: Process ");
    serial_print_dec(current_process->pid);
    serial_print(" (");
    serial_print(current_process->name);
    serial_print(") exiting\n");
    
    // Remove from ready queue
    scheduler_ready_queue_remove(current_process);
    current_process->state = PROC_STATE_TERMINATED;
    
    // Get next process from ready queue
    pcb_t* next = scheduler_ready_queue_next();
    if (!next) {
        serial_print("PROCESS: No processes left, returning to shell\n");
        current_process = NULL;
        // Jump back to the shell - this function should NOT return!
        kmain_shell_loop();
        // Never reached
        while(1) asm volatile("hlt");
    }
    
    // Switch to next process
    pcb_t* old = current_process;
    current_process = next;
    next->state = PROC_STATE_RUNNING;
    next->total_ticks++;
    
    serial_print("SCHEDULER: Switching from PID ");
    if (old) {
        serial_print_dec(old->pid);
        serial_print(" (");
        serial_print(old->name);
        serial_print(")");
    } else {
        serial_print("NULL");
    }
    serial_print(" to PID ");
    serial_print_dec(next->pid);
    serial_print(" (");
    serial_print(next->name);
    serial_print(")\n");
    
    context_switch(old, next);
}

// ============================================================
// Scheduler Switch
// ============================================================
void scheduler_switch_to(pcb_t* next) {
    if (!next) return;
    if (next == current_process) return;
    
    // Don't switch to idle process (PID 1) - it has no entry point
    if (next->pid == 1) {
        serial_print("SCHEDULER: Skipping idle process switch\n");
        return;
    }
    
    pcb_t* prev = current_process;
    current_process = next;
    
    //~ if (prev && prev->state != PROC_STATE_TERMINATED) {
        //~ prev->state = PROC_STATE_READY;
    //~ // ============================================================
    //~ // CRITICAL: Add prev back to ready queue so it can be scheduled again
    //~ // ============================================================
       //~ scheduler_ready_queue_add(prev);
    //~ // ============================================================
    //~ }


if (prev && prev->state != PROC_STATE_TERMINATED) {
    prev->state = PROC_STATE_READY;
    // Only add back if this is the shell and we're switching to a user process
    if (prev->pid == 2) {
        scheduler_ready_queue_add(prev);
    }
}


    next->state = PROC_STATE_RUNNING;
    next->total_ticks++;
    
    serial_print("SCHEDULER: Switching from PID ");
    if (prev) {
        serial_print_dec(prev->pid);
        serial_print(" (");
        serial_print(prev->name);
        serial_print(")");
    } else {
        serial_print("NULL");
    }
    serial_print(" to PID ");
    serial_print_dec(next->pid);
    serial_print(" (");
    serial_print(next->name);
    serial_print(")\n");
    
    context_switch(prev, next);
    
    // NOTE: context_switch() never returns here!
    // The only way we get here is if context_switch() returned,
    // which means the process finished.
    // We handle this in process_yield() by calling process_exit().
}

// ============================================================
// Scheduling Functions
// ============================================================

void scheduler_init(void) {
    serial_print("SCHEDULER: Initializing...\n");
    vga_print("SCHEDULER: Initializing...\n");
    
    ready_queue_head = NULL;
    ready_queue_tail = NULL;
    current_process = NULL;
    schedule_count = 0;
    yield_count = 0;
    
    // Create shell process - this will be the first process
    pcb_t* shell = process_create("shell", (uint64_t)kmain_shell_loop, 0);
    if (shell) {
        shell->state = PROC_STATE_READY;
        shell->timeslice_limit = 10;
        scheduler_ready_queue_add(shell);
        serial_print("SCHEDULER: Shell process created (PID ");
        serial_print_dec(shell->pid);
        serial_print(")\n");
    }
    
    serial_print("SCHEDULER: Initialization complete\n");
    vga_print("SCHEDULER: Initialization complete\n");
}

pcb_t* scheduler_schedule(void) {
    schedule_count++;
    
    pcb_t* next = scheduler_ready_queue_next();
    if (!next) {
        serial_print("SCHEDULER: No processes in ready queue!\n");
        return NULL;
    }
    
    return next;
}

void process_yield(void) {
    if (!current_process) {
        serial_print("SCHEDULER: process_yield called with no current process\n");
        return;
    }
    
    // Don't yield from idle process
    if (current_process->pid == 1) {
        serial_print("SCHEDULER: Idle process yielding - ignoring\n");
        return;
    }
    
    yield_count++;
    
    serial_print("SCHEDULER: process_yield from PID ");
    serial_print_dec(current_process->pid);
    serial_print(" (");
    serial_print(current_process->name);
    serial_print(")\n");
    
    // If current process is still running, move it to the END of the ready queue
    if (current_process->state == PROC_STATE_RUNNING) {
        current_process->state = PROC_STATE_READY;
        scheduler_ready_queue_remove(current_process);
        scheduler_ready_queue_add(current_process);
    }
    
    // Get next process from ready queue
    pcb_t* next = scheduler_ready_queue_next();
    if (!next) {
        // No processes in ready queue - run current process again
        serial_print("SCHEDULER: No processes in ready queue, continuing current\n");
        // Re-add current process
        if (current_process->state == PROC_STATE_READY) {
            scheduler_ready_queue_add(current_process);
            next = current_process;
        } else {
            serial_print("SCHEDULER: ERROR - current process not ready!\n");
            return;
        }
    }
    
    scheduler_switch_to(next);
}

pcb_t* scheduler_get_current(void) {
    return current_process;
}

void scheduler_stats(void) {
    serial_print("\n=== SCHEDULER STATS ===\n");
    serial_print("Schedule count: ");
    serial_print_dec(schedule_count);
    serial_print("\n");
    serial_print("Yield count: ");
    serial_print_dec(yield_count);
    serial_print("\n");
    serial_print("Current process: ");
    if (current_process) {
        serial_print_dec(current_process->pid);
        serial_print(" (");
        serial_print(current_process->name);
        serial_print(")");
    } else {
        serial_print("NULL");
    }
    serial_print("\n");
    serial_print("=== END SCHEDULER STATS ===\n\n");
}

// ============================================================
// Timer Tick - Called from timer interrupt (IRQ0)
// ============================================================
void scheduler_tick(uint64_t saved_rip) {
    if (!current_process) {
        return;
    }
    
    if (current_process->pid == 1) {
        return;
    }
    
    if (current_process->state != PROC_STATE_RUNNING) {
        return;
    }
    
    current_process->timeslice_ticks++;
    current_process->total_ticks++;
    
    if (current_process->timeslice_ticks >= current_process->timeslice_limit) {
        current_process->timeslice_ticks = 0;
        
        serial_print("SCHEDULER: Preempting PID ");
        serial_print_dec(current_process->pid);
        serial_print("\n");
        
        process_yield();
    }
}

// ============================================================
// Set current process without context switch
// ============================================================
void scheduler_set_current_process(pcb_t* proc) {
    if (!proc) {
        serial_print("SCHEDULER: set_current_process called with NULL\n");
        return;
    }
    
    // Remove from ready queue
    scheduler_ready_queue_remove(proc);
    
    // Set as current process
    current_process = proc;
    proc->state = PROC_STATE_RUNNING;
    
    serial_print("SCHEDULER: Set current process to PID ");
    serial_print_dec(proc->pid);
    serial_print(" state=");
    serial_print_dec(proc->state);
    serial_print(" limit=");
    serial_print_dec(proc->timeslice_limit);
    serial_print("\n");
}

// ============================================================
// Start the shell process - called from kmain
// ============================================================
void scheduler_start_shell(void) {
    pcb_t* shell = scheduler_ready_queue_peek();
    if (!shell) {
        serial_print("SCHEDULER: No shell process found!\n");
        kmain_shell_loop();
        return;
    }
    
    serial_print("SCHEDULER: Starting shell process (PID ");
    serial_print_dec(shell->pid);
    serial_print(")\n");
    
    // Set shell as current
    current_process = shell;
    shell->state = PROC_STATE_RUNNING;
    
    // Run the shell directly (it will never return)
    kmain_shell_loop();
}
