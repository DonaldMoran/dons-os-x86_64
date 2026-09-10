#include <stdint.h>
#include <stddef.h>
#include "include/vga.h"
#include "serial.h"

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
static uint8_t cursor_attr = 0x01;   // blue on black
static uint8_t cursor_attr = 0x02;   // green on black
static uint8_t cursor_attr = 0x03;   // cyan on black
static uint8_t cursor_attr = 0x04;   // red on black
static uint8_t cursor_attr = 0x05;   // magenta on black
static uint8_t cursor_attr = 0x06;   // brown on black
static uint8_t cursor_attr = 0x07;   // light gray on black
static uint8_t cursor_attr = 0x08;   // dark gray on black
static uint8_t cursor_attr = 0x09;   // light blue on black
static uint8_t cursor_attr = 0x0A;   // light green on black
static uint8_t cursor_attr = 0x0B;   // light cyan on black
static uint8_t cursor_attr = 0x0C;   // light red on black
static uint8_t cursor_attr = 0x0D;   // light magenta on black
static uint8_t cursor_attr = 0x0E;   // yellow on black
static uint8_t cursor_attr = 0x0F;   // white on black 
*/

/* Background 1 — Blue
static uint8_t cursor_attr = 0x10;   // black on blue
static uint8_t cursor_attr = 0x11;   // blue on blue
static uint8_t cursor_attr = 0x12;   // green on blue
static uint8_t cursor_attr = 0x13;   // cyan on blue
static uint8_t cursor_attr = 0x14;   // red on blue
static uint8_t cursor_attr = 0x15;   // magenta on blue
static uint8_t cursor_attr = 0x16;   // brown on blue
static uint8_t cursor_attr = 0x17;   // light gray on blue
static uint8_t cursor_attr = 0x18;   // dark gray on blue
static uint8_t cursor_attr = 0x19;   // light blue on blue
static uint8_t cursor_attr = 0x1A;   // light green on blue
static uint8_t cursor_attr = 0x1B;   // light cyan on blue
static uint8_t cursor_attr = 0x1C;   // light red on blue
static uint8_t cursor_attr = 0x1D;   // light magenta on blue
static uint8_t cursor_attr = 0x1E;   // yellow on blue
static uint8_t cursor_attr = 0x1F;   // white on blue
*/

/* Background 2 — Green
static uint8_t cursor_attr = 0x20;   // black on green
static uint8_t cursor_attr = 0x21;   // blue on green
static uint8_t cursor_attr = 0x22;   // green on green
static uint8_t cursor_attr = 0x23;   // cyan on green
static uint8_t cursor_attr = 0x24;   // red on green
static uint8_t cursor_attr = 0x25;   // magenta on green
static uint8_t cursor_attr = 0x26;   // brown on green
static uint8_t cursor_attr = 0x27;   // light gray on green
static uint8_t cursor_attr = 0x28;   // dark gray on green
static uint8_t cursor_attr = 0x29;   // light blue on green
static uint8_t cursor_attr = 0x2A;   // light green on green
static uint8_t cursor_attr = 0x2B;   // light cyan on green
static uint8_t cursor_attr = 0x2C;   // light red on green
static uint8_t cursor_attr = 0x2D;   // light magenta on green
static uint8_t cursor_attr = 0x2E;   // yellow on green
static uint8_t cursor_attr = 0x2F;   // white on green
*/

/* Background 3 — Cyan
static uint8_t cursor_attr = 0x30;   // black on cyan
static uint8_t cursor_attr = 0x31;   // blue on cyan
static uint8_t cursor_attr = 0x32;   // green on cyan
static uint8_t cursor_attr = 0x33;   // cyan on cyan
static uint8_t cursor_attr = 0x34;   // red on cyan
static uint8_t cursor_attr = 0x35;   // magenta on cyan
static uint8_t cursor_attr = 0x36;   // brown on cyan
static uint8_t cursor_attr = 0x37;   // light gray on cyan
static uint8_t cursor_attr = 0x38;   // dark gray on cyan
static uint8_t cursor_attr = 0x39;   // light blue on cyan
static uint8_t cursor_attr = 0x3A;   // light green on cyan
static uint8_t cursor_attr = 0x3B;   // light cyan on cyan
static uint8_t cursor_attr = 0x3C;   // light red on cyan
static uint8_t cursor_attr = 0x3D;   // light magenta on cyan
static uint8_t cursor_attr = 0x3E;   // yellow on cyan
static uint8_t cursor_attr = 0x3F;   // white on cyan
*/

/* Background 4 — Red
static uint8_t cursor_attr = 0x40;   // black on red
static uint8_t cursor_attr = 0x41;   // blue on red
static uint8_t cursor_attr = 0x42;   // green on red
static uint8_t cursor_attr = 0x43;   // cyan on red
static uint8_t cursor_attr = 0x44;   // red on red
static uint8_t cursor_attr = 0x45;   // magenta on red
static uint8_t cursor_attr = 0x46;   // brown on red
static uint8_t cursor_attr = 0x47;   // light gray on red
static uint8_t cursor_attr = 0x48;   // dark gray on red
static uint8_t cursor_attr = 0x49;   // light blue on red
static uint8_t cursor_attr = 0x4A;   // light green on red
static uint8_t cursor_attr = 0x4B;   // light cyan on red
static uint8_t cursor_attr = 0x4C;   // light red on red
static uint8_t cursor_attr = 0x4D;   // light magenta on red
static uint8_t cursor_attr = 0x4E;   // yellow on red
static uint8_t cursor_attr = 0x4F;   // white on red
*/

/* Background 5 — Magenta
static uint8_t cursor_attr = 0x50;   // black on magenta
static uint8_t cursor_attr = 0x51;   // blue on magenta
static uint8_t cursor_attr = 0x52;   // green on magenta
static uint8_t cursor_attr = 0x53;   // cyan on magenta
static uint8_t cursor_attr = 0x54;   // red on magenta
static uint8_t cursor_attr = 0x55;   // magenta on magenta
static uint8_t cursor_attr = 0x56;   // brown on magenta
static uint8_t cursor_attr = 0x57;   // light gray on magenta
static uint8_t cursor_attr = 0x58;   // dark gray on magenta
static uint8_t cursor_attr = 0x59;   // light blue on magenta
static uint8_t cursor_attr = 0x5A;   // light green on magenta
static uint8_t cursor_attr = 0x5B;   // light cyan on magenta
static uint8_t cursor_attr = 0x5C;   // light red on magenta
static uint8_t cursor_attr = 0x5D;   // light magenta on magenta
static uint8_t cursor_attr = 0x5E;   // yellow on magenta
static uint8_t cursor_attr = 0x5F;   // white on magenta
*/

/* Background 6 — Brown
static uint8_t cursor_attr = 0x60;   // black on brown
static uint8_t cursor_attr = 0x61;   // blue on brown
static uint8_t cursor_attr = 0x62;   // green on brown
static uint8_t cursor_attr = 0x63;   // cyan on brown
static uint8_t cursor_attr = 0x64;   // red on brown
static uint8_t cursor_attr = 0x65;   // magenta on brown
static uint8_t cursor_attr = 0x66;   // brown on brown
static uint8_t cursor_attr = 0x67;   // light gray on brown
static uint8_t cursor_attr = 0x68;   // dark gray on brown
static uint8_t cursor_attr = 0x69;   // light blue on brown
static uint8_t cursor_attr = 0x6A;   // light green on brown
static uint8_t cursor_attr = 0x6B;   // light cyan on brown
static uint8_t cursor_attr = 0x6C;   // light red on brown
static uint8_t cursor_attr = 0x6D;   // light magenta on brown
static uint8_t cursor_attr = 0x6E;   // yellow on brown
static uint8_t cursor_attr = 0x6F;   // white on brown
*/

/* Background 7 — Light Gray
static uint8_t cursor_attr = 0x70;   // black on light gray
static uint8_t cursor_attr = 0x71;   // blue on light gray
static uint8_t cursor_attr = 0x72;   // green on light gray
static uint8_t cursor_attr = 0x73;   // cyan on light gray
static uint8_t cursor_attr = 0x74;   // red on light gray
static uint8_t cursor_attr = 0x75;   // magenta on light gray
static uint8_t cursor_attr = 0x76;   // brown on light gray
static uint8_t cursor_attr = 0x77;   // light gray on light gray
static uint8_t cursor_attr = 0x78;   // dark gray on light gray
static uint8_t cursor_attr = 0x79;   // light blue on light gray
static uint8_t cursor_attr = 0x7A;   // light green on light gray
static uint8_t cursor_attr = 0x7B;   // light cyan on light gray
static uint8_t cursor_attr = 0x7C;   // light red on light gray
static uint8_t cursor_attr = 0x7D;   // light magenta on light gray
static uint8_t cursor_attr = 0x7E;   // yellow on light gray
static uint8_t cursor_attr = 0x7F;   // white on light gray
*/

// static uint8_t cursor_attr = 0x07;   // light gray on black
static uint8_t cursor_attr = 0x1E;   // yellow on blue


void vga_write(const char* data, size_t count) {
    for (size_t i = 0; i < count; i++) {
        vga_putc(data[i]);  // Use vga_putc, not vga_putchar
    }
}

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
    const char *hex = "0123456789ABCDEF";
    char buf[17];
    buf[16] = '\0';
    
    // Build the string from right to left
    for (int i = 0; i < 16; i++) {
        buf[15 - i] = hex[(val >> (i * 4)) & 0xF];
    }
    
    // Write directly to VGA memory
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
}

//~ void vga_print_hex_cur(uint64_t val) {
    //~ const char *hex = "0123456789ABCDEF";
    //~ char buf[17];
    //~ buf[16] = 0;
    
    //~ // Debug: print to serial first
    //~ serial_print("DEBUG vga_print_hex_cur: val=0x");
    //~ serial_print_hex(val);
    //~ serial_print("\n");
    
    //~ // Build the string
    //~ for (int i = 0; i < 16; i++) {
        //~ buf[15 - i] = hex[(val >> (i * 4)) & 0xF];
    //~ }
    
    //~ // Debug: print the buffer to serial
    //~ serial_print("DEBUG buffer: ");
    //~ for (int i = 0; i < 16; i++) {
        //~ serial_putc(buf[i]);
    //~ }
    //~ serial_print("\n");
    
    //~ // Print to VGA
    //~ for (int i = 0; i < 16; i++) {
        //~ vga_putc(buf[i]);
    //~ }
//~ }

//~ void vga_print_hex_cur(uint64_t val) {
    //~ char buf[17];
    //~ const char *hex = "0123456789ABCDEF";

    //~ __asm__ volatile("cli");
    //~ for (int i = 0; i < 16; i++) {
        //~ buf[15 - i] = hex[(val >> (i * 4)) & 0xF];
    //~ }
    //~ buf[16] = 0;

    //~ for (int i = 0; i < 16; i++) {
        //~ vga_putc(buf[i]);
    //~ }
    //~ __asm__ volatile("sti");
//~ }

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

// In vga.c - add this function
void vga_print_color(const char *s, uint8_t color) {
    __asm__ volatile("cli");
    
    uint8_t old_attr = cursor_attr;
    cursor_attr = color;
    
    while (*s) {
        vga_putc(*s++);
    }
    
    cursor_attr = old_attr;
    
    __asm__ volatile("sti");
}
