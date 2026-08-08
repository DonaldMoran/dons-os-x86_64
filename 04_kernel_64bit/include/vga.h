#pragma once
#include <stdint.h>

#ifndef VGA_H
#define VGA_H

void vga_clear(void);
void vga_putc(char c);

void vga_print(const char *s);
void vga_print_at(int row, int col, const char *s);

void vga_set_cursor(int row, int col);
void vga_set_cursor_shape(uint8_t start_scanline, uint8_t end_scanline);
void vga_hide_cursor(void);

void vga_print_hex_cur(uint64_t val);
void vga_print_dec_cur(uint64_t val);

#endif
