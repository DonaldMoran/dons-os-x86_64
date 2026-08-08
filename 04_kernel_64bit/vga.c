#include <stdint.h>
#include <stddef.h>
#include "include/vga.h"

#define VGA_WIDTH  80
#define VGA_HEIGHT 25
#define VGA_MEM    ((volatile uint16_t *)0xB8000)

#define RESERVED_ROWS 0

// VGA ports for cursor control
#define VGA_CRTC_INDEX  0x3D4
#define VGA_CRTC_DATA   0x3D5
#define CURSOR_HIGH     0x0E
#define CURSOR_LOW      0x0F

static int cursor_row = 0;
static int cursor_col = 0;
static uint8_t cursor_attr = 0x07;

static int clamp(int value, int min, int max) {
    if (value < min) return min;
    if (value > max) return max;
    return value;
}

static inline void outb(uint16_t port, uint8_t val) {
    __asm__ volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
}

static void vga_update_hardware_cursor(void) {
    uint16_t pos = cursor_row * VGA_WIDTH + cursor_col;
    outb(VGA_CRTC_INDEX, CURSOR_HIGH);
    outb(VGA_CRTC_DATA, (pos >> 8) & 0xFF);
    outb(VGA_CRTC_INDEX, CURSOR_LOW);
    outb(VGA_CRTC_DATA, pos & 0xFF);
}

void vga_set_cursor_shape(uint8_t start_scanline, uint8_t end_scanline) {
    __asm__ volatile("cli");
    outb(VGA_CRTC_INDEX, 0x0A);
    outb(VGA_CRTC_DATA, start_scanline);
    outb(VGA_CRTC_INDEX, 0x0B);
    outb(VGA_CRTC_DATA, end_scanline);
    __asm__ volatile("sti");
}

void vga_hide_cursor(void) {
    __asm__ volatile("cli");
    outb(VGA_CRTC_INDEX, CURSOR_HIGH);
    outb(VGA_CRTC_DATA, 0x20);
    outb(VGA_CRTC_INDEX, CURSOR_LOW);
    outb(VGA_CRTC_DATA, 0x00);
    __asm__ volatile("sti");
}

static void vga_scroll(void) {
    volatile uint16_t *vga = VGA_MEM;
    
    cursor_row = clamp(cursor_row, RESERVED_ROWS, VGA_HEIGHT - 1);
    cursor_col = clamp(cursor_col, 0, VGA_WIDTH - 1);
    
    for (int row = RESERVED_ROWS; row < (VGA_HEIGHT - 1); row++) {
        for (int col = 0; col < VGA_WIDTH; col++) {
            int src_idx = (row + 1) * VGA_WIDTH + col;
            int dst_idx = row * VGA_WIDTH + col;
            if (src_idx >= 0 && src_idx < (VGA_WIDTH * VGA_HEIGHT) &&
                dst_idx >= 0 && dst_idx < (VGA_WIDTH * VGA_HEIGHT)) {
                vga[dst_idx] = vga[src_idx];
            }
        }
    }
    
    uint16_t blank = ((uint16_t)cursor_attr << 8) | ' ';
    int last_row_start = (VGA_HEIGHT - 1) * VGA_WIDTH;
    for (int col = 0; col < VGA_WIDTH; col++) {
        int idx = last_row_start + col;
        if (idx >= 0 && idx < (VGA_WIDTH * VGA_HEIGHT)) {
            vga[idx] = blank;
        }
    }
    
    cursor_row = VGA_HEIGHT - 1;
    cursor_col = 0;
    vga_update_hardware_cursor();
}

void vga_putc(char c) {
    volatile uint16_t *vga = VGA_MEM;
    
    __asm__ volatile("cli");
    
    cursor_row = clamp(cursor_row, RESERVED_ROWS, VGA_HEIGHT - 1);
    cursor_col = clamp(cursor_col, 0, VGA_WIDTH - 1);
    
    if (c == '\n') {
        cursor_row++;
        cursor_col = 0;
        if (cursor_row >= VGA_HEIGHT) {
            vga_scroll();
            cursor_row = VGA_HEIGHT - 1;
        }
        vga_update_hardware_cursor();
        __asm__ volatile("sti");
        return;
    }

    if (c == '\b') {
        if (cursor_col > 0) {
            cursor_col--;
            int idx = cursor_row * VGA_WIDTH + cursor_col;
            if (idx >= 0 && idx < (VGA_WIDTH * VGA_HEIGHT)) {
                vga[idx] = ((uint16_t)cursor_attr << 8) | ' ';
            }
        }
        vga_update_hardware_cursor();
        __asm__ volatile("sti");
        return;
    }

    if (c >= ' ' && c <= '~') {
        cursor_row = clamp(cursor_row, RESERVED_ROWS, VGA_HEIGHT - 1);
        cursor_col = clamp(cursor_col, 0, VGA_WIDTH - 1);
        
        int idx = cursor_row * VGA_WIDTH + cursor_col;
        if (idx >= 0 && idx < (VGA_WIDTH * VGA_HEIGHT)) {
            vga[idx] = ((uint16_t)cursor_attr << 8) | (uint8_t)c;
        }

        cursor_col++;
        if (cursor_col >= VGA_WIDTH) {
            cursor_col = 0;
            cursor_row++;
            if (cursor_row >= VGA_HEIGHT) {
                vga_scroll();
                cursor_row = VGA_HEIGHT - 1;
            }
        }
        vga_update_hardware_cursor();
    }
    
    __asm__ volatile("sti");
}

void vga_print(const char *s) {
    __asm__ volatile("cli");
    while (*s) {
        vga_putc(*s++);
    }
    __asm__ volatile("sti");
}

void vga_print_at(int row, int col, const char *s) {
    __asm__ volatile("cli");
    int saved_row = cursor_row;
    int saved_col = cursor_col;
    
    row = clamp(row, RESERVED_ROWS, VGA_HEIGHT - 1);
    col = clamp(col, 0, VGA_WIDTH - 1);
    cursor_row = row;
    cursor_col = col;
    vga_update_hardware_cursor();
    
    while (*s) {
        vga_putc(*s++);
    }
    
    cursor_row = clamp(saved_row, RESERVED_ROWS, VGA_HEIGHT - 1);
    cursor_col = clamp(saved_col, 0, VGA_WIDTH - 1);
    vga_update_hardware_cursor();
    __asm__ volatile("sti");
}

void vga_print_hex_cur(uint64_t val) {
    char buf[17];
    const char *hex = "0123456789ABCDEF";

    __asm__ volatile("cli");
    for (int i = 0; i < 16; i++) {
        buf[15 - i] = hex[(val >> (i * 4)) & 0xF];
    }
    buf[16] = 0;

    for (int i = 0; i < 16; i++) {
        vga_putc(buf[i]);
    }
    __asm__ volatile("sti");
}

void vga_print_dec_cur(uint64_t val) {
    char buf[32];
    int idx = 31;
    buf[idx--] = '\0';

    __asm__ volatile("cli");
    if (val == 0) {
        vga_putc('0');
    } else {
        while (val > 0 && idx >= 0) {
            buf[idx--] = '0' + (val % 10);
            val /= 10;
        }
        int start = idx + 1;
        while (buf[start] != '\0') {
            vga_putc(buf[start++]);
        }
    }
    __asm__ volatile("sti");
}

void vga_clear(void) {
    volatile uint16_t *vga = VGA_MEM;
    __asm__ volatile("cli");
    
    uint16_t blank = ((uint16_t)cursor_attr << 8) | ' ';
    for (int i = 0; i < VGA_WIDTH * VGA_HEIGHT; i++) {
        vga[i] = blank;
    }
    cursor_row = 0;
    cursor_col = 0;
    vga_update_hardware_cursor();
    
    __asm__ volatile("sti");
}

void vga_set_cursor(int row, int col) {
    __asm__ volatile("cli");
    cursor_row = clamp(row, RESERVED_ROWS, VGA_HEIGHT - 1);
    cursor_col = clamp(col, 0, VGA_WIDTH - 1);
    vga_update_hardware_cursor();
    __asm__ volatile("sti");
}
