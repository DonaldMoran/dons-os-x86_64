#include <stdint.h>
#include "include/vga.h"
#include "include/keyboard.h"
#include "include/interrupts.h"
#include "include/serial.h"
#include "include/process.h"
#include "include/scheduler.h"

#define PIC1_CMD  0x20
#define PIC1_DATA 0x21
#define PIC2_CMD  0xA0
#define PIC2_DATA 0xA1

#define PIC_EOI   0x20

#define PIT_CH0      0x40
#define PIT_CMD      0x43
#define PIT_MODE     0x36

#define KBD_DATA   0x60
#define KBD_STATUS 0x64

typedef struct {
    uint64_t r15, r14, r13, r12, r11, r10, r9, r8;
    uint64_t rbp, rdi, rsi, rdx, rcx, rbx, rax;
    uint64_t rip, cs, rflags, rsp, ss;
} __attribute__((packed)) preempt_frame_t;

static inline void outb(uint16_t port, uint8_t val) {
    __asm__ volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
}

static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    __asm__ volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

void pic_remap(void) {
    uint8_t a1 = inb(PIC1_DATA);
    uint8_t a2 = inb(PIC2_DATA);

    outb(PIC1_CMD, 0x11);
    outb(PIC2_CMD, 0x11);

    outb(PIC1_DATA, 0x20);
    outb(PIC2_DATA, 0x28);

    outb(PIC1_DATA, 0x04);
    outb(PIC2_DATA, 0x02);

    outb(PIC1_DATA, 0x01);
    outb(PIC2_DATA, 0x01);

    outb(PIC1_DATA, a1);
    outb(PIC2_DATA, a2);
}

volatile uint64_t g_ticks = 0;
static int g_shift = 0;
static int g_caps  = 0;

/* Diagnostic gate: print the first N user-mode ticks that interrupt the
   shell (pid=2), then go quiet. */
static int g_user_tick_diag = 0;
#define USER_TICK_DIAG_MAX 40

/* Diagnostic gate: print the first N kernel-mode ticks that interrupt
   idle (pid=1), showing what idle sees in the ready queue. */
static int g_idle_diag = 0;
#define IDLE_DIAG_MAX 40


uint64_t timer_preempt_handler(uint64_t stack_pointer) {
    g_ticks++;
    if (g_ticks > 20 && g_ticks < 500) {
        serial_putc('.');
    }
    #define SCHED_QUANTUM 2

    pcb_t* current = process_get_current();
    preempt_frame_t* frame = (preempt_frame_t*)stack_pointer;

    /* Boot-sequence fingerprint: dump the interrupt frame for the
       first 20 ticks only. After that, stay quiet so the serial log
       isn't flooded once user processes are running. */
    if (g_ticks <= 20) {
        serial_print("TIMER[");
        serial_print_dec(g_ticks);
        serial_print("] frame=");
        serial_print_hex(stack_pointer);
        serial_print(" cs=");
        serial_print_hex(frame->cs);
        serial_print(" ss=");
        serial_print_hex(frame->ss);
        serial_print(" rip=");
        serial_print_hex(frame->rip);
        serial_print(" rsp=");
        serial_print_hex(frame->rsp);
        serial_print(" rflags=");
        serial_print_hex(frame->rflags);
        serial_print(" cur=");
        if (current) serial_print_dec(current->pid);
        else serial_print("NULL");
        serial_print("\n");
    }

    if (!current || current->state == PROC_STATE_TERMINATED) {
        if (g_ticks <= 20) serial_print("TIMER: no/terminated current, no switch\n");
        return stack_pointer;
    }

    current->total_ticks++;
    current->timeslice_ticks++;

    // =======================================================================
    // KERNEL-MODE INTERRUPT
    // =======================================================================
    if ((frame->cs & 3) == 0) {
        if (current->pid != 1) {
            if (g_ticks <= 20) serial_print("TIMER: kernel mode (non-idle), return\n");
            return stack_pointer;
        }

        // Idle: check if there's a user task waiting.
        extern pcb_t* scheduler_ready_queue_peek_next(void);
        pcb_t* peek = scheduler_ready_queue_peek_next();

        /* ---- NEW DIAGNOSTIC: idle's view of the ready queue ---- */
        if (g_idle_diag < IDLE_DIAG_MAX) {
            g_idle_diag++;
            serial_print("IDLE[");
            serial_print_dec(g_idle_diag);
            serial_print("] tick=");
            serial_print_dec(g_ticks);
            serial_print(" peek=");
            if (!peek) {
                serial_print("NULL");
            } else {
                serial_print("pid=");
                serial_print_dec(peek->pid);
                serial_print(" state=");
                serial_print_dec(peek->state);
            }
            serial_print("\n");
        }
        /* ---- END NEW DIAGNOSTIC ---- */

        if (!peek || peek->pid == 1) {
            if (g_ticks <= 20) serial_print("TIMER: idle, no user ready\n");
            return stack_pointer;
        }

        // Save idle's interrupt frame into its PCB so it can be resumed.
        current->r15 = frame->r15; current->r14 = frame->r14;
        current->r13 = frame->r13; current->r12 = frame->r12;
        current->r11 = frame->r11; current->r10 = frame->r10;
        current->r9  = frame->r9;  current->r8  = frame->r8;
        current->rbp = frame->rbp; current->rdi = frame->rdi;
        current->rsi = frame->rsi; current->rdx = frame->rdx;
        current->rcx = frame->rcx; current->rbx = frame->rbx;
        current->rax = frame->rax;
        current->rip = frame->rip;
        current->rsp = stack_pointer;

        current->timeslice_ticks = SCHED_QUANTUM;
    }

    if ((frame->cs & 3) == 3) {
        // User-mode interrupt: user tasks are always preemptible.

        /* ---- DIAGNOSTIC: first N user-mode ticks that hit the shell ---- */
        if (g_user_tick_diag < USER_TICK_DIAG_MAX && current->pid == 2) {
            g_user_tick_diag++;
            serial_print("UTICK[");
            serial_print_dec(g_user_tick_diag);
            serial_print("] rip=");
            serial_print_hex(frame->rip);
            serial_print(" rsp=");
            serial_print_hex(frame->rsp);
            serial_print(" rbp=");
            serial_print_hex(frame->rbp);
            serial_print(" cs=");
            serial_print_hex(frame->cs);
            serial_print(" ss=");
            serial_print_hex(frame->ss);
            serial_print("\n");
        }
        /* ---- END DIAGNOSTIC ---- */

        current->timeslice_ticks = SCHED_QUANTUM;
    }

    if (current->timeslice_ticks >= SCHED_QUANTUM) {
        if (g_ticks <= 20) serial_print("TIMER: quantum reached\n");
        current->timeslice_ticks = 0;

        // For user-mode current, save its frame now.
        if ((frame->cs & 3) == 3) {
            current->r15 = frame->r15; current->r14 = frame->r14;
            current->r13 = frame->r13; current->r12 = frame->r12;
            current->r11 = frame->r11; current->r10 = frame->r10;
            current->r9  = frame->r9;  current->r8  = frame->r8;
            current->rbp = frame->rbp; current->rdi = frame->rdi;
            current->rsi = frame->rsi; current->rdx = frame->rdx;
            current->rcx = frame->rcx; current->rbx = frame->rbx;
            current->rax = frame->rax;
            current->rip = frame->rip;
            current->rsp = stack_pointer;
        }

        // Pick next task.
        extern pcb_t* scheduler_ready_queue_next(void);
        pcb_t* next = scheduler_ready_queue_next();
        if (!next) {
            if (current->pid == 1) {
                current->timeslice_ticks = 0;
                return stack_pointer;
            }
            next = process_find_by_pid(1);
            if (!next) {
                return stack_pointer;
            }
        }

        if (current->pid != 1 && current->state != PROC_STATE_TERMINATED) {
            current->state = PROC_STATE_READY;
            scheduler_ready_queue_add(current);
        }

        next->state = PROC_STATE_RUNNING;
        scheduler_set_current(next);

        if (next->entry_point != 0 && next->entry_point < 0xFFFFFFFF80000000ULL) {
            extern void tss_set_kernel_stack(uint64_t stack);
            tss_set_kernel_stack(next->kernel_stack_top);
        }

        __asm__ volatile("mov %0, %%cr3" : : "r"(next->cr3));

        serial_print("TIMER: cur pid=");
        serial_print_dec(current->pid);
        serial_print(" rsp=");
        serial_print_hex(current->rsp);
        serial_print(" -> next pid=");
        serial_print_dec(next->pid);
        serial_print(" rsp=");
        serial_print_hex(next->rsp);
        serial_print("\n");

        return next->rsp;
    }

    return stack_pointer;
}




void irq1_handler(void) {
    uint8_t status = inb(KBD_STATUS);
    if (!(status & 0x01)) {
        outb(PIC1_CMD, PIC_EOI);
        return;
    }

    uint8_t sc = inb(KBD_DATA);
    switch (sc) {
        case 0x2A: case 0x36: g_shift = 1; outb(PIC1_CMD, PIC_EOI); return;
        case 0xAA: case 0xB6: g_shift = 0; outb(PIC1_CMD, PIC_EOI); return;
        case 0x3A: g_caps ^= 1; outb(PIC1_CMD, PIC_EOI); return;
    }

    char c = scancode_to_ascii(sc, g_shift, g_caps);
    if (c) {
        kbd_buffer_put(c);
    }
    outb(PIC1_CMD, PIC_EOI);
}

void isr0_handler(void) {
    vga_print("\n*** DIVIDE BY ZERO EXCEPTION (#DE) ***\n");
    while (1) __asm__ volatile("hlt");
}

void isr1_handler(void) {
    vga_print("\n*** DEBUG EXCEPTION (#DB) ***\n");
    while (1) __asm__ volatile("hlt");
}

void pit_init(uint32_t freq) {
    uint32_t divisor = 1193180 / freq;
    outb(PIT_CMD, PIT_MODE);
    outb(PIT_CH0, divisor & 0xFF);
    outb(PIT_CH0, (divisor >> 8) & 0xFF);
}

void isr8_handler(void) {
    vga_print("\n!!! DOUBLE FAULT !!!\n");
    serial_print("\n!!! DOUBLE FAULT !!!\n");
    while (1) __asm__ volatile("hlt");
}

void isr13_handler(exception_frame_t *frame) {
    uint64_t *raw = (uint64_t *)frame;
    uint64_t error_code = raw[0];
    uint64_t fault_rip  = raw[1];
    uint64_t fault_cs   = raw[2];
    uint64_t fault_rsp  = raw[4];

    vga_print("\n=== GENERAL PROTECTION FAULT (#GP) ===\n");
    vga_print("  Faulting RIP : 0x"); vga_print_hex_cur(fault_rip);  vga_print("\n");
    vga_print("  Code Seg (CS): 0x"); vga_print_hex_cur(fault_cs);   vga_print("\n");
    vga_print("  Stack (RSP)  : 0x"); vga_print_hex_cur(fault_rsp);  vga_print("\n");
    vga_print("  Error Code   : 0x"); vga_print_hex_cur(error_code); vga_print("\n");

    serial_print("\n=== GENERAL PROTECTION FAULT (#GP) ===\n");
    serial_print("  Faulting RIP : 0x"); serial_print_hex(fault_rip);  serial_print("\n");
    serial_print("  Code Seg (CS): 0x"); serial_print_hex(fault_cs);   serial_print("\n");
    serial_print("  Stack (RSP)  : 0x"); serial_print_hex(fault_rsp);  serial_print("\n");
    serial_print("  Error Code   : 0x"); serial_print_hex(error_code); serial_print("\n");

    while (1) __asm__ volatile("hlt");
}

void isr14_handler(exception_frame_t *frame) {
    uint64_t *raw = (uint64_t *)frame;
    uint64_t error_code = raw[0];
    uint64_t fault_rip  = raw[1];
    uint64_t fault_cs   = raw[2];
    uint64_t fault_rsp  = raw[4];
    uint64_t fault_addr;
    __asm__ volatile("mov %%cr2, %0" : "=r"(fault_addr));

    vga_print("\n=== PAGE FAULT (#PF) ===\n");
    vga_print("  CR2 (Bad Address) : 0x"); vga_print_hex_cur(fault_addr); vga_print("\n");
    vga_print("  Faulting RIP      : 0x"); vga_print_hex_cur(fault_rip);  vga_print("\n");
    vga_print("  Raw Error Code    : 0x"); vga_print_hex_cur(error_code); vga_print("\n");

    serial_print("\n=== PAGE FAULT (#PF) ===\n");
    serial_print("  CR2 (Bad Address) : 0x"); serial_print_hex(fault_addr); serial_print("\n");
    serial_print("  Faulting RIP      : 0x"); serial_print_hex(fault_rip);  serial_print("\n");
    serial_print("  Raw Error Code    : 0x"); serial_print_hex(error_code); serial_print("\n");
    serial_print("  CS                : 0x"); serial_print_hex(fault_cs);   serial_print("\n");
    serial_print("  RSP               : 0x"); serial_print_hex(fault_rsp);  serial_print("\n");

    while (1) __asm__ volatile("hlt");
}














//~ #include <stdint.h>
//~ #include "include/vga.h"
//~ #include "include/keyboard.h"
//~ #include "include/interrupts.h"
//~ #include "include/serial.h"
//~ #include "include/process.h"
//~ #include "include/scheduler.h"

//~ #define PIC1_CMD  0x20
//~ #define PIC1_DATA 0x21
//~ #define PIC2_CMD  0xA0
//~ #define PIC2_DATA 0xA1

//~ #define PIC_EOI   0x20

//~ #define PIT_CH0      0x40
//~ #define PIT_CMD      0x43
//~ #define PIT_MODE     0x36

//~ #define KBD_DATA   0x60
//~ #define KBD_STATUS 0x64

//~ typedef struct {
    //~ uint64_t r15, r14, r13, r12, r11, r10, r9, r8;
    //~ uint64_t rbp, rdi, rsi, rdx, rcx, rbx, rax;
    //~ uint64_t rip, cs, rflags, rsp, ss;
//~ } __attribute__((packed)) preempt_frame_t;

//~ static inline void outb(uint16_t port, uint8_t val) {
    //~ __asm__ volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
//~ }

//~ static inline uint8_t inb(uint16_t port) {
    //~ uint8_t ret;
    //~ __asm__ volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port));
    //~ return ret;
//~ }

//~ void pic_remap(void) {
    //~ uint8_t a1 = inb(PIC1_DATA);
    //~ uint8_t a2 = inb(PIC2_DATA);

    //~ outb(PIC1_CMD, 0x11);
    //~ outb(PIC2_CMD, 0x11);

    //~ outb(PIC1_DATA, 0x20);
    //~ outb(PIC2_DATA, 0x28);

    //~ outb(PIC1_DATA, 0x04);
    //~ outb(PIC2_DATA, 0x02);

    //~ outb(PIC1_DATA, 0x01);
    //~ outb(PIC2_DATA, 0x01);

    //~ outb(PIC1_DATA, a1);
    //~ outb(PIC2_DATA, a2);
//~ }

//~ volatile uint64_t g_ticks = 0;
//~ static int g_shift = 0;
//~ static int g_caps  = 0;

//~ /* Diagnostic gate: print the first N user-mode ticks that interrupt the
   //~ shell (pid=2), then go quiet. */
//~ static int g_user_tick_diag = 0;
//~ #define USER_TICK_DIAG_MAX 40


//~ uint64_t timer_preempt_handler(uint64_t stack_pointer) {
    //~ g_ticks++;
    //~ #define SCHED_QUANTUM 2

    //~ pcb_t* current = process_get_current();
    //~ preempt_frame_t* frame = (preempt_frame_t*)stack_pointer;

    //~ /* Boot-sequence fingerprint: dump the interrupt frame for the
       //~ first 20 ticks only. After that, stay quiet so the serial log
       //~ isn't flooded once user processes are running. */
    //~ if (g_ticks <= 20) {
        //~ serial_print("TIMER[");
        //~ serial_print_dec(g_ticks);
        //~ serial_print("] frame=");
        //~ serial_print_hex(stack_pointer);
        //~ serial_print(" cs=");
        //~ serial_print_hex(frame->cs);
        //~ serial_print(" ss=");
        //~ serial_print_hex(frame->ss);
        //~ serial_print(" rip=");
        //~ serial_print_hex(frame->rip);
        //~ serial_print(" rsp=");
        //~ serial_print_hex(frame->rsp);
        //~ serial_print(" rflags=");
        //~ serial_print_hex(frame->rflags);
        //~ serial_print(" cur=");
        //~ if (current) serial_print_dec(current->pid);
        //~ else serial_print("NULL");
        //~ serial_print("\n");
    //~ }

    //~ if (!current || current->state == PROC_STATE_TERMINATED) {
        //~ if (g_ticks <= 20) serial_print("TIMER: no/terminated current, no switch\n");
        //~ return stack_pointer;
    //~ }

    //~ current->total_ticks++;
    //~ current->timeslice_ticks++;

    //~ // =======================================================================
    //~ // KERNEL-MODE INTERRUPT
    //~ // =======================================================================
    //~ if ((frame->cs & 3) == 0) {
        //~ if (current->pid != 1) {
            //~ if (g_ticks <= 20) serial_print("TIMER: kernel mode (non-idle), return\n");
            //~ return stack_pointer;
        //~ }

        //~ // Idle: check if there's a user task waiting.
        //~ extern pcb_t* scheduler_ready_queue_peek_next(void);
        //~ pcb_t* peek = scheduler_ready_queue_peek_next();
        //~ if (!peek || peek->pid == 1) {
            //~ if (g_ticks <= 20) serial_print("TIMER: idle, no user ready\n");
            //~ return stack_pointer;
        //~ }

        //~ // Save idle's interrupt frame into its PCB so it can be resumed.
        //~ current->r15 = frame->r15; current->r14 = frame->r14;
        //~ current->r13 = frame->r13; current->r12 = frame->r12;
        //~ current->r11 = frame->r11; current->r10 = frame->r10;
        //~ current->r9  = frame->r9;  current->r8  = frame->r8;
        //~ current->rbp = frame->rbp; current->rdi = frame->rdi;
        //~ current->rsi = frame->rsi; current->rdx = frame->rdx;
        //~ current->rcx = frame->rcx; current->rbx = frame->rbx;
        //~ current->rax = frame->rax;
        //~ current->rip = frame->rip;
        //~ current->rsp = stack_pointer;

        //~ current->timeslice_ticks = SCHED_QUANTUM;
    //~ }

    //~ if ((frame->cs & 3) == 3) {
        //~ // User-mode interrupt: user tasks are always preemptible.

        //~ /* ---- NEW DIAGNOSTIC ---- */
        //~ /* Print the first USER_TICK_DIAG_MAX user-mode interrupts that
           //~ hit the shell (pid=2). We want to see the RIP/RSP/RBP the
           //~ CPU pushed, so we can compare RBP before and after resume.
           //~ If RBP here is a normal user-stack address (0x80000FFFxx),
           //~ then the corruption happens on restore. If it's already
           //~ wrong here, the corruption happened earlier. */
        //~ if (g_user_tick_diag < USER_TICK_DIAG_MAX && current->pid == 2) {
            //~ g_user_tick_diag++;
            //~ serial_print("UTICK[");
            //~ serial_print_dec(g_user_tick_diag);
            //~ serial_print("] rip=");
            //~ serial_print_hex(frame->rip);
            //~ serial_print(" rsp=");
            //~ serial_print_hex(frame->rsp);
            //~ serial_print(" rbp=");
            //~ serial_print_hex(frame->rbp);
            //~ serial_print(" cs=");
            //~ serial_print_hex(frame->cs);
            //~ serial_print(" ss=");
            //~ serial_print_hex(frame->ss);
            //~ serial_print("\n");
        //~ }
        //~ /* ---- END NEW DIAGNOSTIC ---- */

        //~ current->timeslice_ticks = SCHED_QUANTUM;
    //~ }

    //~ if (current->timeslice_ticks >= SCHED_QUANTUM) {
        //~ if (g_ticks <= 20) serial_print("TIMER: quantum reached\n");
        //~ current->timeslice_ticks = 0;

        //~ // For user-mode current, save its frame now.
        //~ if ((frame->cs & 3) == 3) {
            //~ current->r15 = frame->r15; current->r14 = frame->r14;
            //~ current->r13 = frame->r13; current->r12 = frame->r12;
            //~ current->r11 = frame->r11; current->r10 = frame->r10;
            //~ current->r9  = frame->r9;  current->r8  = frame->r8;
            //~ current->rbp = frame->rbp; current->rdi = frame->rdi;
            //~ current->rsi = frame->rsi; current->rdx = frame->rdx;
            //~ current->rcx = frame->rcx; current->rbx = frame->rbx;
            //~ current->rax = frame->rax;
            //~ current->rip = frame->rip;
            //~ current->rsp = stack_pointer;
        //~ }

        //~ // Pick next task.
        //~ extern pcb_t* scheduler_ready_queue_next(void);
        //~ pcb_t* next = scheduler_ready_queue_next();
        //~ if (!next) {
            //~ if (current->pid == 1) {
                //~ current->timeslice_ticks = 0;
                //~ return stack_pointer;
            //~ }
            //~ next = process_find_by_pid(1);
            //~ if (!next) {
                //~ return stack_pointer;
            //~ }
        //~ }

        //~ if (current->pid != 1 && current->state != PROC_STATE_TERMINATED) {
            //~ current->state = PROC_STATE_READY;
            //~ scheduler_ready_queue_add(current);
        //~ }

        //~ next->state = PROC_STATE_RUNNING;
        //~ scheduler_set_current(next);

        //~ if (next->entry_point != 0 && next->entry_point < 0xFFFFFFFF80000000ULL) {
            //~ extern void tss_set_kernel_stack(uint64_t stack);
            //~ tss_set_kernel_stack(next->kernel_stack_top);
        //~ }

        //~ __asm__ volatile("mov %0, %%cr3" : : "r"(next->cr3));

        //~ serial_print("TIMER: cur pid=");
        //~ serial_print_dec(current->pid);
        //~ serial_print(" rsp=");
        //~ serial_print_hex(current->rsp);
        //~ serial_print(" -> next pid=");
        //~ serial_print_dec(next->pid);
        //~ serial_print(" rsp=");
        //~ serial_print_hex(next->rsp);
        //~ serial_print("\n");

        //~ return next->rsp;
    //~ }

    //~ return stack_pointer;
//~ }




//~ void irq1_handler(void) {
    //~ uint8_t status = inb(KBD_STATUS);
    //~ if (!(status & 0x01)) {
        //~ outb(PIC1_CMD, PIC_EOI);
        //~ return;
    //~ }

    //~ uint8_t sc = inb(KBD_DATA);
    //~ switch (sc) {
        //~ case 0x2A: case 0x36: g_shift = 1; outb(PIC1_CMD, PIC_EOI); return;
        //~ case 0xAA: case 0xB6: g_shift = 0; outb(PIC1_CMD, PIC_EOI); return;
        //~ case 0x3A: g_caps ^= 1; outb(PIC1_CMD, PIC_EOI); return;
    //~ }

    //~ char c = scancode_to_ascii(sc, g_shift, g_caps);
    //~ if (c) {
        //~ kbd_buffer_put(c);
    //~ }
    //~ outb(PIC1_CMD, PIC_EOI);
//~ }

//~ void isr0_handler(void) {
    //~ vga_print("\n*** DIVIDE BY ZERO EXCEPTION (#DE) ***\n");
    //~ while (1) __asm__ volatile("hlt");
//~ }

//~ void isr1_handler(void) {
    //~ vga_print("\n*** DEBUG EXCEPTION (#DB) ***\n");
    //~ while (1) __asm__ volatile("hlt");
//~ }

//~ void pit_init(uint32_t freq) {
    //~ uint32_t divisor = 1193180 / freq;
    //~ outb(PIT_CMD, PIT_MODE);
    //~ outb(PIT_CH0, divisor & 0xFF);
    //~ outb(PIT_CH0, (divisor >> 8) & 0xFF);
//~ }

//~ void isr8_handler(void) {
    //~ vga_print("\n!!! DOUBLE FAULT !!!\n");
    //~ serial_print("\n!!! DOUBLE FAULT !!!\n");
    //~ while (1) __asm__ volatile("hlt");
//~ }

//~ void isr13_handler(exception_frame_t *frame) {
    //~ uint64_t *raw = (uint64_t *)frame;
    //~ uint64_t error_code = raw[0];
    //~ uint64_t fault_rip  = raw[1];
    //~ uint64_t fault_cs   = raw[2];
    //~ uint64_t fault_rsp  = raw[4];

    //~ vga_print("\n=== GENERAL PROTECTION FAULT (#GP) ===\n");
    //~ vga_print("  Faulting RIP : 0x"); vga_print_hex_cur(fault_rip);  vga_print("\n");
    //~ vga_print("  Code Seg (CS): 0x"); vga_print_hex_cur(fault_cs);   vga_print("\n");
    //~ vga_print("  Stack (RSP)  : 0x"); vga_print_hex_cur(fault_rsp);  vga_print("\n");
    //~ vga_print("  Error Code   : 0x"); vga_print_hex_cur(error_code); vga_print("\n");

    //~ serial_print("\n=== GENERAL PROTECTION FAULT (#GP) ===\n");
    //~ serial_print("  Faulting RIP : 0x"); serial_print_hex(fault_rip);  serial_print("\n");
    //~ serial_print("  Code Seg (CS): 0x"); serial_print_hex(fault_cs);   serial_print("\n");
    //~ serial_print("  Stack (RSP)  : 0x"); serial_print_hex(fault_rsp);  serial_print("\n");
    //~ serial_print("  Error Code   : 0x"); serial_print_hex(error_code); serial_print("\n");

    //~ while (1) __asm__ volatile("hlt");
//~ }

//~ void isr14_handler(exception_frame_t *frame) {
    //~ uint64_t *raw = (uint64_t *)frame;
    //~ uint64_t error_code = raw[0];
    //~ uint64_t fault_rip  = raw[1];
    //~ uint64_t fault_cs   = raw[2];
    //~ uint64_t fault_rsp  = raw[4];
    //~ uint64_t fault_addr;
    //~ __asm__ volatile("mov %%cr2, %0" : "=r"(fault_addr));

    //~ vga_print("\n=== PAGE FAULT (#PF) ===\n");
    //~ vga_print("  CR2 (Bad Address) : 0x"); vga_print_hex_cur(fault_addr); vga_print("\n");
    //~ vga_print("  Faulting RIP      : 0x"); vga_print_hex_cur(fault_rip);  vga_print("\n");
    //~ vga_print("  Raw Error Code    : 0x"); vga_print_hex_cur(error_code); vga_print("\n");

    //~ serial_print("\n=== PAGE FAULT (#PF) ===\n");
    //~ serial_print("  CR2 (Bad Address) : 0x"); serial_print_hex(fault_addr); serial_print("\n");
    //~ serial_print("  Faulting RIP      : 0x"); serial_print_hex(fault_rip);  serial_print("\n");
    //~ serial_print("  Raw Error Code    : 0x"); serial_print_hex(error_code); serial_print("\n");
    //~ serial_print("  CS                : 0x"); serial_print_hex(fault_cs);   serial_print("\n");
    //~ serial_print("  RSP               : 0x"); serial_print_hex(fault_rsp);  serial_print("\n");

    //~ while (1) __asm__ volatile("hlt");
//~ }
