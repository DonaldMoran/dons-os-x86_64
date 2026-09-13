#include "include/scheduler.h"
#include "include/serial.h"
#include "include/vga.h"

#define KERNEL_BASE 0xFFFFFFFF80000000ULL

extern void kmain_shell_loop(void);

static pcb_t* ready_queue_head = NULL;
static pcb_t* ready_queue_tail = NULL;
static pcb_t* current_process = NULL;

static uint64_t schedule_count = 0;
static uint64_t yield_count = 0;

extern void context_switch(pcb_t* prev, pcb_t* next);

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

    /* Defensive: if the process has no prev and no next and is not
       the head, it is not on the queue. Removing it would set
       ready_queue_head = process->next = NULL and corrupt the
       queue. */
    if (!process->prev && !process->next && ready_queue_head != process) {
        return;
    }

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

    /* Disable interrupts for the entire exit sequence. The timer
       must not fire between the first state mutation and the
       context_switch:

         - Between scheduler_ready_queue_remove and setting state to
           TERMINATED, the timer's early check
           (current->state == PROC_STATE_TERMINATED) would not fire,
           and the timer could re-add the exiting process to the
           ready queue.

         - After current_process = next but before context_switch
           switches stacks, the timer would see current = next and
           save the *current* stack frame (which is still on the
           exiting process's stack) into next->rsp. That corrupts
           next's resume frame with a pointer into a stack that may
           soon be reused.

       Interrupts are re-enabled by the iretq in context_switch,
       which restores RFLAGS (IF=1) from the resumed process's
       frame. In the no-runnable-process fallback, we explicitly
       sti before jumping to the kernel shell, since the shell
       expects to run with interrupts on. */
    __asm__ volatile("cli");

    pcb_t* exiting = current_process;

    exiting->state = PROC_STATE_TERMINATED;
    scheduler_ready_queue_remove(exiting);

    if (process_get_current() == exiting) {
        process_set_current(NULL);
    }

    /* Reclaim the exiting process's resources so its PCB slot can be
       reused by a future process_create. This must happen before we
       switch away, while the exiting process's context is still
       current. Page tables (cr3) are not freed here; that teardown
       is deferred. */
    process_reclaim(exiting);

    pcb_t* next = scheduler_ready_queue_next();
    if (!next) {
        /* Nothing else runnable. Two cases:
           - exiting is a kernel process (diagnostic like runproc or
             testyield): return to the kernel shell so the operator
             can run more diagnostics.
           - exiting is a user process (the user shell): halt. The
             user shell is the terminal interactive console; there is
             no kernel shell to fall back to by design. */
        scheduler_reset();

        /* Restore TSS.RSP0 and g_syscall_stack_top to idle's kernel
           stack. The previous code zeroed only g_syscall_stack_top,
           leaving TSS.RSP0 pointing at whichever process ran last.
           The two must stay in lockstep (see tss.c); leaving RSP0
           pointing at a dead process's stack (or at 0) is a latent
           hazard for any Ring 3 -> Ring 0 transition that occurs
           before the next scheduler switch. Idle's stack is always
           mapped and always valid. */
        extern void tss_set_kernel_stack(uint64_t stack);
        extern void tss_set_syscall_stack(uint64_t stack);
        pcb_t* idle = process_find_by_pid(1);
        if (idle) {
            tss_set_kernel_stack(idle->kernel_stack_top);
            tss_set_syscall_stack(idle->kernel_stack_top);
        } else {
            tss_set_kernel_stack(0);
            tss_set_syscall_stack(0);
        }

        if (exiting->entry_point >= KERNEL_BASE) {
            /* Direct jump, not indirect through a register. The
               previous version loaded kmain_shell_loop into RAX,
               did sti, then jmp *RAX. If an IRQ1 fired between the
               sti and the jmp — which is possible, since irq1_stub
               historically did not preserve caller-saved registers —
               RAX could be clobbered by irq1_handler and the jmp
               would land at a garbage address. isr.asm now
               preserves all GPRs in every stub, but the direct
               jump is the belt-and-suspenders fix: no register is
               involved, so no interrupt can corrupt the target.
               The compiler emits jmp rel32; the CPU decodes it as
               a single instruction with no memory operand. */
            __asm__ volatile(
                "sti\n"
                "mov $0xFFFFFFFF8008FF00, %%rsp\n"
                "jmp kmain_shell_loop\n"
                : : : "memory"
            );
            /* not reached */
        }

        serial_print("process_exit: no runnable process, halting\n");
        __asm__ volatile("cli");
        while (1) __asm__ volatile("hlt");
    }

    current_process = next;
    process_set_current(next);
    next->state = PROC_STATE_RUNNING;
    next->total_ticks++;

    /* TSS.RSP0 and the syscall entry stack top must track `current`
       unconditionally. Gating this on entry_point < KERNEL_BASE was
       wrong: kernel-mode threads still need RSP0 correct so that a
       later switch to a user process does not inherit a stale kernel
       stack pointer, and the "both move in lockstep with current"
       invariant documented in tss.c must hold at every context
       switch. See scheduler_switch_to for the matching update. */
    extern void tss_set_kernel_stack(uint64_t stack);
    extern void tss_set_syscall_stack(uint64_t stack);
    tss_set_kernel_stack(next->kernel_stack_top);
    tss_set_syscall_stack(next->kernel_stack_top);

    context_switch(exiting, next);

    while(1) asm volatile("hlt");
}

void scheduler_switch_to(pcb_t* next) {
    if (!next) return;
    if (next == current_process) return;

    if (next->pid == 1 && current_process &&
        current_process->state == PROC_STATE_RUNNING) {
        return;
    }

    /* Disable interrupts around the state mutation and the actual
       stack switch. Without this, a PIT tick that arrives between
       `current_process = next` and `context_switch(prev, next)` will
       see `current == next` (a process that hasn't actually been
       switched to yet) and save the *current* CPU state — which is
       still running on the previous process's stack — into `next`'s
       PCB fields. That overwrites `next->rsp` with a frame base that
       lives on the wrong stack, and the next time `next` is resumed
       the iretq fires on a corrupted frame.

       Interrupts are re-enabled by the iretq in context_switch,
       which restores RFLAGS (IF=1) from the target's saved frame. */
    __asm__ volatile("cli");

    pcb_t* prev = current_process;
    current_process = next;
    process_set_current(next);

    if (next->pid != 1) {
        scheduler_ready_queue_remove(next);
    }

    if (prev && prev->state == PROC_STATE_RUNNING) {
        prev->state = PROC_STATE_READY;
        if (prev->pid != 1) {
            scheduler_ready_queue_add(prev);
        }
    }

    next->state = PROC_STATE_RUNNING;
    next->total_ticks++;

    /* TSS.RSP0 and the syscall entry stack top must track `current`
       unconditionally. The previous gate (entry_point < KERNEL_BASE)
       caused g_syscall_stack_top to stay at 0 when switching to a
       kernel-mode process, breaking the lockstep invariant
       documented in tss.c. Update both, always. */
    extern void tss_set_kernel_stack(uint64_t stack);
    extern void tss_set_syscall_stack(uint64_t stack);
    tss_set_kernel_stack(next->kernel_stack_top);
    tss_set_syscall_stack(next->kernel_stack_top);

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
        /* The current process is RUNNING, so it is NOT on the ready
           queue (scheduler_switch_to removed it when it switched to
           it). Do not call scheduler_ready_queue_remove: on an
           off-queue process, its prev/next links are NULL, so remove
           would set ready_queue_head = NULL and corrupt the queue.
           Just add it to the tail. */
        current_process->state = PROC_STATE_READY;
        scheduler_ready_queue_add(current_process);
    }

    pcb_t* next = scheduler_ready_queue_next();
    if (!next) {
        if (current_process->state == PROC_STATE_READY) {
            scheduler_ready_queue_add(current_process);
            next = current_process;
        } else {
            next = process_find_by_pid(1);
            if (!next) return;
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
