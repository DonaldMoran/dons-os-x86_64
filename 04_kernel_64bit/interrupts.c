#include <stdint.h>
#include "include/vga.h"
#include "include/keyboard.h"
#include "include/interrupts.h"  // Add this include
#include "include/serial.h"

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

// Divide by Zero handler with color
void isr0_handler(void) {
    vga_print("\n");
    vga_print_color("*** DIVIDE BY ZERO EXCEPTION (#DE) ***\n", 0x0C);
    vga_print("System halted.\n");
    while (1) __asm__ volatile("hlt");
}

// Debug handler with color
void isr1_handler(void) {
    vga_print("\n");
    vga_print_color("*** DEBUG EXCEPTION (#DB) ***\n", 0x0C);
    vga_print("System halted.\n");
    while (1) __asm__ volatile("hlt");
}

void pit_init(uint32_t freq) {
    uint32_t divisor = 1193180 / freq;
    outb(PIT_CMD, PIT_MODE);
    outb(PIT_CH0, divisor & 0xFF);
    outb(PIT_CH0, (divisor >> 8) & 0xFF);
}

// Double Fault handler with color
void isr8_handler(void) {
    vga_print("\n");
    vga_print_color("!!! DOUBLE FAULT !!!\n", 0x0C);
    vga_print("System halted.\n");
    while (1) __asm__ volatile("hlt");
}

// GP Fault handler with color
void isr13_handler(exception_frame_t *frame) {
    uint64_t *raw = (uint64_t *)frame;

    vga_print("\n");
    vga_print_color("=== GENERAL PROTECTION FAULT (#GP) ===\n", 0x0C);

    vga_print("Error Code : 0x");
    vga_print_hex_cur(raw[0]);
    vga_print("\n");

    vga_print("RIP        : 0x");
    vga_print_hex_cur(raw[1]);
    vga_print("\n");

    vga_print("CS         : 0x");
    vga_print_hex_cur(raw[2]);
    vga_print("\n");

    vga_print("RFLAGS     : 0x");
    vga_print_hex_cur(raw[3]);
    vga_print("\n");

    vga_print("\nRaw Frame Dump:\n");
    vga_print("  RAW[0] (error) : 0x"); vga_print_hex_cur(raw[0]); vga_print("\n");
    vga_print("  RAW[1] (rip)   : 0x"); vga_print_hex_cur(raw[1]); vga_print("\n");
    vga_print("  RAW[2] (cs)    : 0x"); vga_print_hex_cur(raw[2]); vga_print("\n");
    vga_print("  RAW[3] (rflags): 0x"); vga_print_hex_cur(raw[3]); vga_print("\n");
    
    uint64_t fault_rip = raw[1] & 0xFFFFFFFFFFFFULL;

    serial_print("GP: fault RIP = 0x");
    serial_print_hex(fault_rip);
    serial_print("\n");

    
        serial_print("\n=== GP FAULT FRAME (raw) ===\n");
    for (int i = 0; i < 8; i++) {
        serial_print("  raw[");
        serial_print_hex(i);
        serial_print("] = 0x");
        serial_print_hex(raw[i]);
        serial_print("\n");
    }
    serial_print("=== END GP FRAME ===\n");

    while (1) __asm__ volatile("hlt");
}



// Page Fault handler with color
void isr14_handler(exception_frame_t *frame) {
    uint64_t *raw = (uint64_t *)frame;

    uint64_t cr2;
    __asm__ volatile("mov %%cr2, %0" : "=r"(cr2));

    vga_print("\n");
    vga_print_color("=== PAGE FAULT (#PF) ===\n", 0x0C);

    vga_print("CR2 (addr) : 0x");
    vga_print_hex_cur(cr2);
    vga_print("\n");

    vga_print("Error Code : 0x");
    vga_print_hex_cur(raw[0]);
    vga_print("\n");

    vga_print("RIP        : 0x");
    vga_print_hex_cur(raw[1]);
    vga_print("\n");

    vga_print("CS         : 0x");
    vga_print_hex_cur(raw[2]);
    vga_print("\n");

    vga_print("RFLAGS     : 0x");
    vga_print_hex_cur(raw[3]);
    vga_print("\n");

    vga_print("\nRaw Frame Dump:\n");
    vga_print("  RAW[0] (error) : 0x"); vga_print_hex_cur(raw[0]); vga_print("\n");
    vga_print("  RAW[1] (rip)   : 0x"); vga_print_hex_cur(raw[1]); vga_print("\n");
    vga_print("  RAW[2] (cs)    : 0x"); vga_print_hex_cur(raw[2]); vga_print("\n");
    vga_print("  RAW[3] (rflags): 0x"); vga_print_hex_cur(raw[3]); vga_print("\n");

    while (1) __asm__ volatile("hlt");
}

