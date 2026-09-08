#ifndef SERIAL_H
#define SERIAL_H

#include <stddef.h>    // ADD THIS for size_t
#include <stdint.h>    // ADD THIS for uint64_t

void serial_init(void);
void serial_putc(char c);
void serial_print(const char* str);
void serial_print_hex(uint64_t value);
void serial_print_dec(uint64_t value);
void serial_write(const char* buf, size_t count);

#endif

