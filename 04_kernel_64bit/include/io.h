#ifndef IO_H
#define IO_H

/*
 * x86_64 port I/O primitives.
 *
 * Note: serial.c, vga.c, and kmain.c currently each carry their own local
 * copies of inb/outb. Consolidating those onto this header is a follow-up
 * cleanup; this file exists so new drivers (starting with ata.c) do not
 * add a fourth and fifth copy.
 */

#include <stdint.h>

static inline void outb(uint16_t port, uint8_t val) {
    __asm__ volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
}

static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    __asm__ volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

static inline void outw(uint16_t port, uint16_t val) {
    __asm__ volatile ("outw %0, %1" : : "a"(val), "Nd"(port));
}

static inline uint16_t inw(uint16_t port) {
    uint16_t ret;
    __asm__ volatile ("inw %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

static inline void outl(uint16_t port, uint32_t val) {
    __asm__ volatile ("outl %0, %1" : : "a"(val), "Nd"(port));
}

static inline uint32_t inl(uint16_t port) {
    uint32_t ret;
    __asm__ volatile ("inl %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

/*
 * ~400ns delay on real hardware. Four reads from the ATA alternate status
 * port (0x3F6) is the traditional idiom. Each inb costs roughly 100ns.
 * On QEMU this is instantaneous, but the pattern is harmless and portable.
 */
static inline void io_wait(void) {
    __asm__ volatile ("outb %%al, $0x80" : : "a"(0));
}

#endif
