#include "include/scheduler.h"
#include "include/serial.h"
#include "include/vga.h"

// Ready queue head and tail
static pcb_t* ready_queue_head = NULL;
static pcb_t* ready_queue_tail = NULL;
static pcb_t* current_process = NULL;

static uint64_t schedule_count = 0;
static uint64_t yield_count = 0;

extern void context_switch(pcb_t* prev, pcb_t* next);
extern void kmain_shell_loop(void);

// NEW SCHEDULER QUEUE UTILITY: Lets the timer interrupt check for waiting work nodes safely
pcb_t* scheduler_ready_queue_peek_next(void) {
    return ready_queue_head;
}

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

int scheduler_ready_queue_empty(void) {
    return ready_queue_head == NULL;
}

void scheduler_reset(void) {
    ready_queue_head = NULL;
    ready_queue_tail = NULL;
    current_process = NULL;
    process_set_current(NULL);
}

void scheduler_set_current(pcb_t* proc) {
    current_process = proc;
    process_set_current(proc);
}

void __attribute__((noreturn)) process_exit(void) {
    if (!current_process) {
        while(1) asm volatile("hlt");
    }
    
    pcb_t* exiting = current_process;
    scheduler_ready_queue_remove(exiting);
    exiting->state = PROC_STATE_TERMINATED;
    
    if (process_get_current() == exiting) {
        process_set_current(NULL);
    }
    
    pcb_t* next = scheduler_ready_queue_next();
    if (!next) {
        scheduler_reset();
        __asm__ volatile (
            "mov $0xFFFFFFFF8008FF00, %%rsp\n"
            "jmp *%0\n"
            : : "r"(kmain_shell_loop)
            : "memory"
        );
        while(1) asm volatile("hlt");
    }
    
    current_process = next;
    process_set_current(next);
    next->state = PROC_STATE_RUNNING;
    next->total_ticks++;
    
    context_switch(exiting, next);
    while(1) asm volatile("hlt");
}

void scheduler_switch_to(pcb_t* next) {
    if (!next) return;
    if (next == current_process) return;
    
    if (next->pid == 1) {
        return;
    }
    
    pcb_t* prev = current_process;
    current_process = next;
    process_set_current(next);
    
    scheduler_ready_queue_remove(next);
    
    if (prev && prev->state != PROC_STATE_TERMINATED) {
        prev->state = PROC_STATE_READY;
        if (prev->state == PROC_STATE_READY) {
            scheduler_ready_queue_add(prev);
        }
    }
    next->state = PROC_STATE_RUNNING;
    next->total_ticks++;
    
    context_switch(prev, next);
}

void scheduler_init(void) {
    ready_queue_head = NULL;
    ready_queue_tail = NULL;
    current_process = NULL;
    process_set_current(NULL);
    schedule_count = 0;
    yield_count = 0;
}

pcb_t* scheduler_schedule(void) {
    schedule_count++;
    pcb_t* next = scheduler_ready_queue_next();
    if (!next) return NULL;
    return next;
}

void process_yield(void) {
    if (!current_process) return;
    if (current_process->pid == 1) return;
    
    yield_count++;
    
    if (current_process->state == PROC_STATE_RUNNING) {
        current_process->state = PROC_STATE_READY;
        scheduler_ready_queue_remove(current_process);
        scheduler_ready_queue_add(current_process);
    }
    
    pcb_t* next = scheduler_ready_queue_next();
    if (!next) {
        if (current_process->state == PROC_STATE_READY) {
            scheduler_ready_queue_add(current_process);
            next = current_process;
        } else {
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
    serial_print("Schedule count: "); serial_print_dec(schedule_count); serial_print("\n");
    serial_print("Yield count: "); serial_print_dec(yield_count); serial_print("\n");
    serial_print("=== END SCHEDULER STATS ===\n\n");
}
