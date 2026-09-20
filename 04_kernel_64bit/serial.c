#include "include/serial.h"
#include <stddef.h>
#include <stdint.h>

static inline void outb(uint16_t port, uint8_t val) {
    __asm__ volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
}
static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    __asm__ volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

/*
 * Shared print lock. See include/serial.h for the contract.
 *
 * The lock saves and restores RFLAGS on the outermost lock/unlock
 * pair. This means:
 *   - At shell level, serial_lock disables interrupts, serial_unlock
 *     re-enables them.
 *   - Inside an interrupt or exception handler (interrupts already
 *     off), serial_lock is a no-op at the CPU level, and serial_unlock
 *     leaves interrupts off. The handler remains undisturbed.
 * Nested lock/unlock pairs are safe: only the outermost pair touches
 * RFLAGS.
 */
static uint32_t g_print_lock_depth = 0;
static uint64_t g_print_lock_saved_rflags = 0;

void serial_lock(void) {
    if (g_print_lock_depth == 0) {
        __asm__ volatile(
            "pushfq\n\t"
            "popq %0\n\t"
            "cli"
            : "=r"(g_print_lock_saved_rflags)
            :
            : "memory"
        );
    } else {
        __asm__ volatile("cli" ::: "memory");
    }
    g_print_lock_depth++;
}

void serial_unlock(void) {
    if (g_print_lock_depth > 0) {
        g_print_lock_depth--;
        if (g_print_lock_depth == 0) {
            uint64_t saved = g_print_lock_saved_rflags;
            __asm__ volatile(
                "pushq %0\n\t"
                "popfq"
                :
                : "r"(saved)
                : "memory", "cc"
            );
        }
    }
}

void serial_init(void) {
    outb(0x3F8 + 1, 0x00);
    outb(0x3F8 + 3, 0x80);
    outb(0x3F8 + 0, 0x03);
    outb(0x3F8 + 1, 0x00);
    outb(0x3F8 + 3, 0x03);
    outb(0x3F8 + 2, 0xC7);
    outb(0x3F8 + 4, 0x0B);
}

void serial_putc(char c) {
    while ((inb(0x3F8 + 5) & 0x20) == 0);
    outb(0x3F8, c);
}

void serial_write(const char* buf, size_t count) {
    serial_lock();
    for (size_t i = 0; i < count; i++) {
        serial_putc(buf[i]);
    }
    serial_unlock();
}

void serial_print(const char* str) {
    serial_lock();
    while (*str) serial_putc(*str++);
    serial_unlock();
}

void serial_print_hex(uint64_t value) {
    char hex[] = "0123456789ABCDEF";
    char buf[16];

    serial_lock();
    for (int i = 15; i >= 0; i--) {
        buf[i] = hex[value & 0xF];
        value >>= 4;
    }
    for (int i = 0; i < 16; i++) serial_putc(buf[i]);
    serial_unlock();
}

void serial_print_dec(uint64_t value) {
    char buf[32];
    int idx = 31;
    buf[idx--] = '\0';

    serial_lock();
    if (value == 0) {
        serial_putc('0');
        serial_unlock();
        return;
    }
    while (value > 0 && idx >= 0) {
        buf[idx--] = '0' + (value % 10);
        value /= 10;
    }
    for (int i = idx + 1; buf[i] != '\0'; i++) {
        serial_putc(buf[i]);
    }
    serial_unlock();
}
