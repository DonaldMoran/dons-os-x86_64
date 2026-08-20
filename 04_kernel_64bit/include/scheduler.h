#ifndef SCHEDULER_H
#define SCHEDULER_H

#include "process.h"

// Scheduler functions
void scheduler_init(void);
void scheduler_ready_queue_add(pcb_t* process);
void scheduler_ready_queue_remove(pcb_t* process);
pcb_t* scheduler_ready_queue_next(void);
pcb_t* scheduler_ready_queue_peek(void);
int scheduler_ready_queue_empty(void);
pcb_t* scheduler_schedule(void);
void scheduler_switch_to(pcb_t* next);
void process_yield(void);
void process_exit(void);
pcb_t* scheduler_get_current(void);
void scheduler_stats(void);
void scheduler_tick(uint64_t saved_rip);
void scheduler_set_current_process(pcb_t* proc);
void scheduler_start_shell(void);

#endif
