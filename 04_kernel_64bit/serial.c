#include "include/serial.h"
#include <stddef.h>    // ADD THIS for size_t
#include <stdint.h>    // ADD THIS for uint64_t

static inline void outb(uint16_t port, uint8_t val) {
    __asm__ volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
}
static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    __asm__ volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
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
    for (size_t i = 0; i < count; i++) {
        serial_putc(buf[i]);
    }
}

void serial_print(const char* str) {
    while (*str) serial_putc(*str++);
}

void serial_print_hex(uint64_t value) {
    char hex[] = "0123456789ABCDEF";
    char buf[17]; buf[16] = 0;
    if (value == 0) { serial_putc('0'); return; }
    int i = 15;
    while (value > 0 && i >= 0) { buf[i--] = hex[value & 0xF]; value >>= 4; }
    for (int j = i + 1; j < 16; j++) serial_putc(buf[j]);
}

void serial_print_dec(uint64_t value) {
    char buf[32];
    int idx = 31;
    buf[idx--] = '\0';
    if (value == 0) {
        serial_putc('0');
        return;
    }
    while (value > 0 && idx >= 0) {
        buf[idx--] = '0' + (value % 10);
        value /= 10;
    }
    for (int i = idx + 1; buf[i] != '\0'; i++) {
        serial_putc(buf[i]);
    }
}
