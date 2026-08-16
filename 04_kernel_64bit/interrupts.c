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

//~ // GP Fault handler with color
//~ void isr13_handler(exception_frame_t *frame) {
    //~ uint64_t *raw = (uint64_t *)frame;

    //~ vga_print("\n");
    //~ vga_print_color("=== GENERAL PROTECTION FAULT (#GP) ===\n", 0x0C);

    //~ vga_print("Error Code : 0x");
    //~ vga_print_hex_cur(raw[0]);
    //~ vga_print("\n");

    //~ vga_print("RIP        : 0x");
    //~ vga_print_hex_cur(raw[1]);
    //~ vga_print("\n");

    //~ vga_print("CS         : 0x");
    //~ vga_print_hex_cur(raw[2]);
    //~ vga_print("\n");

    //~ vga_print("RFLAGS     : 0x");
    //~ vga_print_hex_cur(raw[3]);
    //~ vga_print("\n");

    //~ vga_print("\nRaw Frame Dump:\n");
    //~ vga_print("  RAW[0] (error) : 0x"); vga_print_hex_cur(raw[0]); vga_print("\n");
    //~ vga_print("  RAW[1] (rip)   : 0x"); vga_print_hex_cur(raw[1]); vga_print("\n");
    //~ vga_print("  RAW[2] (cs)    : 0x"); vga_print_hex_cur(raw[2]); vga_print("\n");
    //~ vga_print("  RAW[3] (rflags): 0x"); vga_print_hex_cur(raw[3]); vga_print("\n");
    
    //~ uint64_t fault_rip = raw[1] & 0xFFFFFFFFFFFFULL;

    //~ serial_print("GP: fault RIP = 0x");
    //~ serial_print_hex(fault_rip);
    //~ serial_print("\n");

    
    //~ serial_print("\n=== GP FAULT FRAME (raw) ===\n");
    //~ for (int i = 0; i < 8; i++) {
        //~ serial_print("  raw[");
        //~ serial_print_hex(i);
        //~ serial_print("] = 0x");
        //~ serial_print_hex(raw[i]);
        //~ serial_print("\n");
    //~ }
    //~ serial_print("=== END GP FRAME ===\n");

    //~ while (1) __asm__ volatile("hlt");
//~ }

void isr13_handler(exception_frame_t *frame) {
    uint64_t *raw = (uint64_t *)frame;
    uint64_t cr3;
    __asm__ volatile("mov %%cr3, %0" : "=r"(cr3));

    vga_print_color("=== GENERAL PROTECTION FAULT (#GP) ===\n", 0x0C);
    vga_print("Error Code : 0x"); vga_print_hex_cur(raw[0]); vga_print("\n");
    vga_print("RIP        : 0x"); vga_print_hex_cur(raw[1]); vga_print("\n");
    vga_print("CS         : 0x"); vga_print_hex_cur(raw[2]); vga_print("\n");
    vga_print("RFLAGS     : 0x"); vga_print_hex_cur(raw[3]); vga_print("\n");
    vga_print("RSP        : 0x"); vga_print_hex_cur(raw[4]); vga_print("\n");
    vga_print("SS         : 0x"); vga_print_hex_cur(raw[5]); vga_print("\n");
    vga_print("CR3        : 0x"); vga_print_hex_cur(cr3);   vga_print("\n");
    
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


void isr14_handler(exception_frame_t *frame) {
    uint64_t cr2;
    __asm__ volatile("mov %%cr2, %0" : "=r"(cr2));

    vga_print("\n");
    vga_print_color("=== PAGE FAULT (#PF) ===\n", 0x0C);

    vga_print("CR2 (addr) : 0x");
    vga_print_hex_cur(cr2);
    vga_print("\n");

    vga_print("Error Code : 0x");
    vga_print_hex_cur(frame->error_code);
    vga_print("\n");

    vga_print("RIP        : 0x");
    vga_print_hex_cur(frame->rip);
    vga_print("\n");

    vga_print("CS         : 0x");
    vga_print_hex_cur(frame->cs);
    vga_print("\n");

    vga_print("RFLAGS     : 0x");
    vga_print_hex_cur(frame->rflags);
    vga_print("\n");

    vga_print("RSP        : 0x");
    vga_print_hex_cur(frame->rsp);
    vga_print("\n");

    vga_print("SS         : 0x");
    vga_print_hex_cur(frame->ss);
    vga_print("\n");

    // Decode error code
    vga_print("\nError Code Decode:\n");
    vga_print("  P  (bit 0) Present?        : ");
    vga_print((frame->error_code & 1) ? "YES\n" : "NO\n");

    vga_print("  W/R(bit 1) Write access?   : ");
    vga_print((frame->error_code & 2) ? "WRITE\n" : "READ\n");

    vga_print("  U/S(bit 2) From usermode?  : ");
    vga_print((frame->error_code & 4) ? "USER\n" : "KERNEL\n");

    vga_print("  RSVD(bit 3) Reserved bit?  : ");
    vga_print((frame->error_code & 8) ? "YES\n" : "NO\n");

    vga_print("  I/D(bit 4) Instr fetch?    : ");
    vga_print((frame->error_code & 16) ? "YES\n" : "NO\n");

    vga_print("\nRaw Frame Dump:\n");
    vga_print("  RAW[0] (error) : 0x"); vga_print_hex_cur(frame->error_code); vga_print("\n");
    vga_print("  RAW[1] (rip)   : 0x"); vga_print_hex_cur(frame->rip);        vga_print("\n");
    vga_print("  RAW[2] (cs)    : 0x"); vga_print_hex_cur(frame->cs);         vga_print("\n");
    vga_print("  RAW[3] (rflags): 0x"); vga_print_hex_cur(frame->rflags);     vga_print("\n");
    vga_print("  RAW[4] (rsp)   : 0x"); vga_print_hex_cur(frame->rsp);        vga_print("\n");
    vga_print("  RAW[5] (ss)    : 0x"); vga_print_hex_cur(frame->ss);         vga_print("\n");
    
    // Print to the console as well
    serial_print("\n");
    serial_print("=== PAGE FAULT (#PF) ===\n");

    serial_print("CR2 (addr) : 0x");
    serial_print_hex(cr2);
    serial_print("\n");

    serial_print("Error Code : 0x");
    serial_print_hex(frame->error_code);
    serial_print("\n");

    serial_print("RIP        : 0x");
    serial_print_hex(frame->rip);
    serial_print("\n");

    serial_print("CS         : 0x");
    serial_print_hex(frame->cs);
    serial_print("\n");

    serial_print("RFLAGS     : 0x");
    serial_print_hex(frame->rflags);
    serial_print("\n");

    serial_print("RSP        : 0x");
    serial_print_hex(frame->rsp);
    serial_print("\n");

    serial_print("SS         : 0x");
    serial_print_hex(frame->ss);
    serial_print("\n");

    // Decode error code
    serial_print("\nError Code Decode:\n");
    serial_print("  P  (bit 0) Present?        : ");
    serial_print((frame->error_code & 1) ? "YES\n" : "NO\n");

    serial_print("  W/R(bit 1) Write access?   : ");
    serial_print((frame->error_code & 2) ? "WRITE\n" : "READ\n");

    serial_print("  U/S(bit 2) From usermode?  : ");
    serial_print((frame->error_code & 4) ? "USER\n" : "KERNEL\n");

    serial_print("  RSVD(bit 3) Reserved bit?  : ");
    serial_print((frame->error_code & 8) ? "YES\n" : "NO\n");

    serial_print("  I/D(bit 4) Instr fetch?    : ");
    serial_print((frame->error_code & 16) ? "YES\n" : "NO\n");

    serial_print("\nRaw Frame Dump:\n");
    serial_print("  RAW[0] (error) : 0x"); serial_print_hex(frame->error_code); serial_print("\n");
    serial_print("  RAW[1] (rip)   : 0x"); serial_print_hex(frame->rip);        serial_print("\n");
    serial_print("  RAW[2] (cs)    : 0x"); serial_print_hex(frame->cs);         serial_print("\n");
    serial_print("  RAW[3] (rflags): 0x"); serial_print_hex(frame->rflags);     serial_print("\n");
    serial_print("  RAW[4] (rsp)   : 0x"); serial_print_hex(frame->rsp);        serial_print("\n");
    serial_print("  RAW[5] (ss)    : 0x"); serial_print_hex(frame->ss);         serial_print("\n");

    while (1) __asm__ volatile("hlt");
}

