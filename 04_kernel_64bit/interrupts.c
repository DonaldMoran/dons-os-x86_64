#include <stdint.h>
#include "include/vga.h"
#include "include/keyboard.h"
#include "include/interrupts.h"  // Add this include

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

// Remove 'static' so kmain can access it
volatile uint64_t g_ticks = 0;
static int g_shift = 0;
static int g_caps  = 0;

void irq0_handler(void) {
    g_ticks++;
    outb(PIC1_CMD, PIC_EOI);
}

void irq1_handler(void) {
    uint8_t status = inb(KBD_STATUS);
    if (!(status & 0x01)) {
        outb(PIC1_CMD, PIC_EOI);
        return;
    }

    uint8_t sc = inb(KBD_DATA);

    switch (sc) {
        case 0x2A:
        case 0x36:
            g_shift = 1;
            outb(PIC1_CMD, PIC_EOI);
            return;
        case 0xAA:
        case 0xB6:
            g_shift = 0;
            outb(PIC1_CMD, PIC_EOI);
            return;
        case 0x3A:
            g_caps ^= 1;
            outb(PIC1_CMD, PIC_EOI);
            return;
    }

    char c = scancode_to_ascii(sc, g_shift, g_caps);
    if (c)
        kbd_buffer_put(c);

    outb(PIC1_CMD, PIC_EOI);
}

void isr0_handler(void) {
    while (1) { }
}

void isr1_handler(void) {
    while (1) { }
}

void pit_init(uint32_t freq) {
    uint32_t divisor = 1193180 / freq;
    outb(PIT_CMD, PIT_MODE);
    outb(PIT_CH0, divisor & 0xFF);
    outb(PIT_CH0, (divisor >> 8) & 0xFF);
}

void isr13_handler(void) {
    // General Protection Fault
    vga_print("\n*** GENERAL PROTECTION FAULT (#GP) ***\n");
    vga_print("System halted.\n");

    while (1) {
        __asm__ volatile("hlt");
    }
}
