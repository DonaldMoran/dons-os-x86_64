#pragma once

#ifndef INTERRUPTS_H
#define INTERRUPTS_H

#include <stdint.h>

void pic_remap(void);
void irq0_handler(void);
void irq1_handler(void);
void pit_init(uint32_t freq);

// Declare g_ticks as extern so kmain can access it
extern volatile uint64_t g_ticks;

typedef struct exception_frame {
    uint64_t error_code;
    uint64_t rip;
    uint64_t cs;
    uint64_t rflags;
    uint64_t rsp;
    uint64_t ss;
} exception_frame_t;

/*
 * Default-handler frame.  This is what isr_default_common passes to
 * isr_default_handler().  It extends exception_frame_t with the vector
 * number, which is pushed by the default stub before the common
 * trampoline pushes all GPRs.
 *
 * The GPR fields are present for completeness; the default handler
 * prints only the control and vector fields.
 */
typedef struct default_frame {
    uint64_t r15, r14, r13, r12, r11, r10, r9, r8;
    uint64_t rbp, rdi, rsi, rdx, rcx, rbx, rax;
    uint64_t vector;
    uint64_t error_code;
    uint64_t rip, cs, rflags, rsp, ss;
} __attribute__((packed)) default_frame_t;

void isr8_handler(void);
void isr13_handler(exception_frame_t *frame);
void isr14_handler(exception_frame_t *frame);
void isr_default_handler(default_frame_t *frame);

#endif
