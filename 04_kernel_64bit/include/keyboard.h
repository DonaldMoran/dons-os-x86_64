#pragma once
#include <stdint.h>

#ifndef KEYBOARD_H
#define KEYBOARD_H

void keyboard_init(void);
void keyboard_isr(void);

int  kbd_buffer_put(char c);
int  kbd_buffer_get(char *c);

// Expose our new flush primitive tool to your kernel loader files
void keyboard_buffer_flush(void);

char scancode_to_ascii(uint8_t sc, int shift, int caps);

#endif
