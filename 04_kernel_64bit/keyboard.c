#include <stdint.h>
#include "include/keyboard.h"

#define KBD_BUFFER_SIZE 128

static char kbd_buffer[KBD_BUFFER_SIZE];
static int  kbd_head = 0;
static int  kbd_tail = 0;

void keyboard_init(void) {
    kbd_head = 0;
    kbd_tail = 0;
}

int kbd_buffer_put(char c) {
    int next = (kbd_head + 1) % KBD_BUFFER_SIZE;

    if (next == kbd_tail)
        return 0; // buffer full

    kbd_buffer[kbd_head] = c;
    kbd_head = next;
    return 1;
}

int kbd_buffer_get(char *c) {
    if (kbd_head == kbd_tail)
        return 0; // buffer empty

    *c = kbd_buffer[kbd_tail];
    kbd_tail = (kbd_tail + 1) % KBD_BUFFER_SIZE;
    return 1;
}

// Complete ASCII table for scancodes
static const char scancode_ascii[128] = {
    [0x02] = '1',
    [0x03] = '2',
    [0x04] = '3',
    [0x05] = '4',
    [0x06] = '5',
    [0x07] = '6',
    [0x08] = '7',
    [0x09] = '8',
    [0x0A] = '9',
    [0x0B] = '0',
    [0x0E] = '\b',  // backspace
    [0x10] = 'q',
    [0x11] = 'w',
    [0x12] = 'e',
    [0x13] = 'r',
    [0x14] = 't',
    [0x15] = 'y',
    [0x16] = 'u',
    [0x17] = 'i',
    [0x18] = 'o',
    [0x19] = 'p',
    [0x1C] = '\n',  // ENTER key!
    [0x1E] = 'a',
    [0x1F] = 's',
    [0x20] = 'd',
    [0x21] = 'f',
    [0x22] = 'g',
    [0x23] = 'h',
    [0x24] = 'j',
    [0x25] = 'k',
    [0x26] = 'l',
    [0x2C] = 'z',
    [0x2D] = 'x',
    [0x2E] = 'c',
    [0x2F] = 'v',
    [0x30] = 'b',
    [0x31] = 'n',
    [0x32] = 'm',
    [0x39] = ' ',  // space
};

// Shifted versions for numbers and symbols
static const char scancode_shift[128] = {
    [0x02] = '!',
    [0x03] = '@',
    [0x04] = '#',
    [0x05] = '$',
    [0x06] = '%',
    [0x07] = '^',
    [0x08] = '&',
    [0x09] = '*',
    [0x0A] = '(',
    [0x0B] = ')',
    [0x10] = 'Q',
    [0x11] = 'W',
    [0x12] = 'E',
    [0x13] = 'R',
    [0x14] = 'T',
    [0x15] = 'Y',
    [0x16] = 'U',
    [0x17] = 'I',
    [0x18] = 'O',
    [0x19] = 'P',
    [0x1E] = 'A',
    [0x1F] = 'S',
    [0x20] = 'D',
    [0x21] = 'F',
    [0x22] = 'G',
    [0x23] = 'H',
    [0x24] = 'J',
    [0x25] = 'K',
    [0x26] = 'L',
    [0x2C] = 'Z',
    [0x2D] = 'X',
    [0x2E] = 'C',
    [0x2F] = 'V',
    [0x30] = 'B',
    [0x31] = 'N',
    [0x32] = 'M',
};

char scancode_to_ascii(uint8_t sc, int shift, int caps) {
    // break code: ignore (key release)
    if (sc & 0x80)
        return 0;

    uint8_t code = sc & 0x7F;

    // Special keys we handle directly
    if (code == 0x0E)  // backspace
        return '\b';
    if (code == 0x1C)  // enter
        return '\n';
    if (code == 0x39)  // space
        return ' ';

    // If shift is pressed, use shifted table
    if (shift) {
        char ch = scancode_shift[code];
        if (ch)
            return ch;
    }

    // Otherwise use normal ASCII table
    char ch = scancode_ascii[code];
    if (!ch)
        return 0;

    // Letters: apply caps lock if shift is NOT pressed
    // (caps lock only affects letters, not numbers)
    if (ch >= 'a' && ch <= 'z') {
        if (caps && !shift) {
            // Caps lock on and shift off -> uppercase
            ch = (char)(ch - 'a' + 'A');
        }
    }

    return ch;
}