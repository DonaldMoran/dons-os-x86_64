#include <stdint.h>
#include <stddef.h>
#include "include/vga.h"
#include "include/serial.h"

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

/* Background 0 — Black
static uint8_t cursor_attr = 0x00;   // black on black
...
*/

/* Background 7 — Light Gray
...
*/

// static uint8_t cursor_attr = 0x07;   // light gray on black
static uint8_t cursor_attr = 0x1E;   // yellow on blue


static int clamp(int value, int min, int max) {
    if (value < min) return min;
    if (value > max) return max;
    return value;
}

static inline void outb(uint16_t port, uint8_t val) {
    __asm__ volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
}

/*
 * VGA print routines use the shared print lock defined in serial.c
 * (serial_lock / serial_unlock). The lock is a reentrant cli/sti
 * critical section, so nested lock/unlock pairs are safe.
 *
 * Design:
 *   - vga_putc and vga_update_hardware_cursor are UNLOCKED primitives.
 *     They must be called from within a locked region.
 *   - Every higher-level function (vga_print, vga_clear, etc.) takes
 *     the lock around its whole operation.
 *
 * This makes vga_print atomic against timer preemption: a timer tick
 * cannot fire between characters of a printed string. Without this,
 * the cursor state and the string output can be split by an interrupt
 * and the tail of the string lands in the wrong place.
 *
 * See include/serial.h for the full contract and MAINTENANCE.md for
 * the design discussion (items 4d and 4e).
 */

static void vga_update_hardware_cursor(void) {
    uint16_t pos = cursor_row * VGA_WIDTH + cursor_col;
    outb(VGA_CRTC_INDEX, CURSOR_HIGH);
    outb(VGA_CRTC_DATA, (pos >> 8) & 0xFF);
    outb(VGA_CRTC_INDEX, CURSOR_LOW);
    outb(VGA_CRTC_DATA, pos & 0xFF);
}

void vga_set_cursor_shape(uint8_t start_scanline, uint8_t end_scanline) {
    serial_lock();
    outb(VGA_CRTC_INDEX, 0x0A);
    outb(VGA_CRTC_DATA, start_scanline);
    outb(VGA_CRTC_INDEX, 0x0B);
    outb(VGA_CRTC_DATA, end_scanline);
    serial_unlock();
}

void vga_hide_cursor(void) {
    serial_lock();
    outb(VGA_CRTC_INDEX, CURSOR_HIGH);
    outb(VGA_CRTC_DATA, 0x20);
    outb(VGA_CRTC_INDEX, CURSOR_LOW);
    outb(VGA_CRTC_DATA, 0x00);
    serial_unlock();
}

/*
 * Unlocked primitive: scroll the screen up one line. Caller must hold
 * the print lock.
 */
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

/*
 * Unlocked primitive: write one character. Caller must hold the print
 * lock. See vga_putc_locked below for a convenience wrapper.
 */
static void vga_putc_unlocked(char c) {
    volatile uint16_t *vga = VGA_MEM;
    
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
}

/*
 * Public single-character write. Takes the lock so callers that write
 * one character at a time do not need to manage the lock themselves.
 * Prefer this over vga_putc_unlocked unless you are inside a locked
 * region.
 */
void vga_putc(char c) {
    serial_lock();
    vga_putc_unlocked(c);
    serial_unlock();
}

void vga_write(const char* data, size_t count) {
    serial_lock();
    for (size_t i = 0; i < count; i++) {
        vga_putc_unlocked(data[i]);
    }
    serial_unlock();
}

void vga_print(const char *s) {
    serial_lock();
    while (*s) {
        vga_putc_unlocked(*s++);
    }
    serial_unlock();
}

void vga_print_at(int row, int col, const char *s) {
    serial_lock();
    int saved_row = cursor_row;
    int saved_col = cursor_col;
    
    row = clamp(row, RESERVED_ROWS, VGA_HEIGHT - 1);
    col = clamp(col, 0, VGA_WIDTH - 1);
    cursor_row = row;
    cursor_col = col;
    vga_update_hardware_cursor();
    
    while (*s) {
        vga_putc_unlocked(*s++);
    }
    
    cursor_row = clamp(saved_row, RESERVED_ROWS, VGA_HEIGHT - 1);
    cursor_col = clamp(saved_col, 0, VGA_WIDTH - 1);
    vga_update_hardware_cursor();
    serial_unlock();
}

void vga_print_hex_cur(uint64_t val) {
    const char *hex = "0123456789ABCDEF";
    char buf[17];
    buf[16] = '\0';
    
    // Build the string from right to left
    for (int i = 0; i < 16; i++) {
        buf[15 - i] = hex[(val >> (i * 4)) & 0xF];
    }
    
    serial_lock();
    
    volatile uint16_t *vga = (volatile uint16_t *)0xB8000;
    int pos = cursor_row * 80 + cursor_col;
    uint16_t attr = (uint16_t)cursor_attr << 8;
    
    for (int i = 0; i < 16; i++) {
        if (pos + i < 80 * 25) {
            vga[pos + i] = attr | (uint8_t)buf[i];
        }
    }
    cursor_col += 16;
    if (cursor_col >= 80) {
        cursor_col = 0;
        cursor_row++;
        if (cursor_row >= 25) {
            // Need to scroll
            vga_scroll();
            cursor_row = 24;
        }
    }
    vga_update_hardware_cursor();
    
    serial_unlock();
}

void vga_print_dec_cur(uint64_t val) {
    char buf[32];
    int idx = 31;
    buf[idx--] = '\0';

    serial_lock();
    if (val == 0) {
        vga_putc_unlocked('0');
    } else {
        while (val > 0 && idx >= 0) {
            buf[idx--] = '0' + (val % 10);
            val /= 10;
        }
        int start = idx + 1;
        while (buf[start] != '\0') {
            vga_putc_unlocked(buf[start++]);
        }
    }
    serial_unlock();
}

void vga_clear(void) {
    volatile uint16_t *vga = VGA_MEM;
    serial_lock();
    
    uint16_t blank = ((uint16_t)cursor_attr << 8) | ' ';
    for (int i = 0; i < VGA_WIDTH * VGA_HEIGHT; i++) {
        vga[i] = blank;
    }
    cursor_row = 0;
    cursor_col = 0;
    vga_update_hardware_cursor();
    
    serial_unlock();
}

void vga_set_cursor(int row, int col) {
    serial_lock();
    cursor_row = clamp(row, RESERVED_ROWS, VGA_HEIGHT - 1);
    cursor_col = clamp(col, 0, VGA_WIDTH - 1);
    vga_update_hardware_cursor();
    serial_unlock();
}

void vga_print_color(const char *s, uint8_t color) {
    serial_lock();
    
    uint8_t old_attr = cursor_attr;
    cursor_attr = color;
    
    while (*s) {
        vga_putc_unlocked(*s++);
    }
    
    cursor_attr = old_attr;
    
    serial_unlock();
}
