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

/*void isr13_handler(exception_frame_t *frame) {

    vga_print("\n*** GENERAL PROTECTION FAULT (#GP) ***\n");

    vga_print("RIP: ");    vga_print_hex_cur(frame->rip);    vga_print("\n");
    vga_print("CS:  ");    vga_print_hex_cur(frame->cs);     vga_print("\n");
    vga_print("RFLAGS: "); vga_print_hex_cur(frame->rflags); vga_print("\n");
    vga_print("RSP: ");    vga_print_hex_cur(frame->rsp);    vga_print("\n");
    vga_print("SS:  ");    vga_print_hex_cur(frame->ss);     vga_print("\n");
    vga_print("ERR: ");    vga_print_hex_cur(frame->error_code); vga_print("\n");

    vga_print("System halted.\n");
    while (1) __asm__ volatile("hlt");
}
*/

/*
void isr13_handler(exception_frame_gp_t *frame) {
    vga_print("\n*** GENERAL PROTECTION FAULT (#GP) ***\n");

    vga_print("RIP: ");    vga_print_hex_cur(frame->rip);    vga_print("\n");
    vga_print("CS:  ");    vga_print_hex_cur(frame->cs);     vga_print("\n");
    vga_print("RFLAGS: "); vga_print_hex_cur(frame->rflags); vga_print("\n");
    vga_print("RSP: ");    vga_print_hex_cur(frame->rsp);    vga_print("\n");
    vga_print("SS:  ");    vga_print_hex_cur(frame->ss);     vga_print("\n");
    vga_print("ERR: ");    vga_print_hex_cur(frame->error_code); vga_print("\n");

    vga_print("System halted.\n");
    while (1) __asm__ volatile("hlt");
}
*/


// TODO (#GP full decoding):
// This handler is intentionally minimal. Early in kernel development, decoding
// the full #GP exception frame (RIP, CS, RFLAGS, RSP, SS, error_code) caused
// stack layout and calling‑convention side effects that interfered with IRQ1,
// breaking keyboard input. The kernel is still too small and fragile for safe
// frame inspection.
//
// Once the kernel has:
//   - a stable interrupt pipeline,
//   - a scheduler or at least a controlled main loop,
//   - a more mature memory manager,
//   - and verified stack alignment rules,
// we will implement full #GP frame decoding here.
//
// For now, we only print a message and halt cleanly to avoid corrupting runtime
// state.
void isr13_handler(exception_frame_t *frame) {
    (void)frame;   // unused until full decoding is implemented
    vga_print("\n*** GENERAL PROTECTION FAULT (#GP) ***\n");
    vga_print("System halted.\n");
    while (1) __asm__ volatile("hlt");
}


void isr14_handler(exception_frame_t *frame) {

    uint64_t cr2;
    __asm__ volatile("mov %%cr2, %0" : "=r"(cr2));

    vga_print("\n*** PAGE FAULT (#PF) ***\n");

    vga_print("CR2 (fault addr): ");
    vga_print_hex_cur(cr2);
    vga_print("\n");

    vga_print("ERR: ");
    vga_print_hex_cur(frame->error_code);
    vga_print("\n");

    vga_print("RIP: ");
    vga_print_hex_cur(frame->rip);
    vga_print("\n");

    vga_print("System halted.\n");
    while (1) __asm__ volatile("hlt");

}
