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
} exception_frame_t;





void isr8_handler(void);
void isr13_handler(exception_frame_t *frame);
void isr14_handler(exception_frame_t *frame);

#endif
