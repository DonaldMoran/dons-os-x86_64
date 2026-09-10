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

uint64_t timer_preempt_handler(uint64_t stack_pointer) {
    g_ticks++;
    #define SCHED_QUANTUM 2

    pcb_t* current = process_get_current();
    preempt_frame_t* frame = (preempt_frame_t*)stack_pointer;

    if (!current || current->state == PROC_STATE_TERMINATED) {
        return stack_pointer; 
    }

    current->total_ticks++;
    current->timeslice_ticks++;

    // =======================================================================
    // CRITICAL CORE FIX: PRIVILEGE FENCE GUARD
    // =======================================================================
    // If the interrupted code selector belongs to Ring 0 (Kernel/Syscall path),
    // we MUST immediately return without modifying thread states or executing
    // context switches. This keeps the kernel stack safe from nested drift!
    if ((frame->cs & 3) == 0) {
        return stack_pointer;
    }

    if ((frame->cs & 3) == 3) {
        uint64_t active_cr3;
        __asm__ volatile("mov %%cr3, %0" : "=r"(active_cr3));
        uint64_t resolved_pid = current->pid;
        
        for (int idx = 0; idx < MAX_PROCESSES; idx++) {
            extern pcb_t* process_find_by_pid(uint64_t pid);
            pcb_t* p = process_find_by_pid(idx);
            if (p && p->state != PROC_STATE_UNUSED && p->cr3 == active_cr3) {
                resolved_pid = p->pid;
                break;
            }
        }
        current->timeslice_ticks = SCHED_QUANTUM;
    }

    if (current->timeslice_ticks >= SCHED_QUANTUM) {
        current->timeslice_ticks = 0;

        if (current->pid == 1) {
            extern pcb_t* scheduler_ready_queue_peek_next(void);
            pcb_t* next = scheduler_ready_queue_peek_next();
            if (next && next->pid != 1) {
                extern pcb_t* scheduler_ready_queue_next(void);
                next = scheduler_ready_queue_next();
                current->state = PROC_STATE_READY;
                next->state = PROC_STATE_RUNNING;
                scheduler_set_current(next);
                
                if (next->entry_point != 0 && next->entry_point < 0xFFFFFFFF80000000ULL) {
                    extern void tss_set_kernel_stack(uint64_t stack);
                    tss_set_kernel_stack(next->kernel_stack_top);
                }
                __asm__ volatile("mov %0, %%cr3" : : "r"(next->cr3));
                return next->rsp;
            }
            return stack_pointer;
        }

        extern pcb_t* scheduler_ready_queue_peek_next(void);
        pcb_t* check_next = scheduler_ready_queue_peek_next();
        if (!check_next || check_next == current) {
            return stack_pointer;
        }

        current->r15 = frame->r15; current->r14 = frame->r14; current->r13 = frame->r13;
        current->r12 = frame->r12; current->r11 = frame->r11; current->r10 = frame->r10;
        current->r9  = frame->r9;  current->r8  = frame->r8;
        current->rbp = frame->rbp; current->rdi = frame->rdi; current->rsi = frame->rsi;
        current->rdx = frame->rdx; current->rcx = frame->rcx; current->rbx = frame->rbx;
        current->rax = frame->rax;
        
        current->rip = frame->rip;
        current->rsp = stack_pointer; 

        current->state = PROC_STATE_READY;
        scheduler_ready_queue_add(current);

        extern pcb_t* scheduler_ready_queue_next(void);
        pcb_t* next = scheduler_ready_queue_next();
        if (!next) {
            next = process_find_by_pid(1);
        }

        next->state = PROC_STATE_RUNNING;
        scheduler_set_current(next);

        if (next->entry_point != 0 && next->entry_point < 0xFFFFFFFF80000000ULL) {
            extern void tss_set_kernel_stack(uint64_t stack);
            tss_set_kernel_stack(next->kernel_stack_top);
        }

        __asm__ volatile("mov %0, %%cr3" : : "r"(next->cr3));
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
    while (1) __asm__ volatile("hlt");
}
