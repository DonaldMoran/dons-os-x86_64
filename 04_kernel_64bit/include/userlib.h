#ifndef USERLIB_H
#define USERLIB_H

// ============================================================
// Standard definitions for freestanding C
// ============================================================

#define NULL 0

// ============================================================
// System calls (Ring 3 -> Kernel)
// ============================================================

long sys_write(int fd, const void* buf, unsigned long count);
long sys_read(int fd, void* buf, unsigned long count);
void sys_exit(int status);

// ============================================================
// String functions
// ============================================================

int strlen(const char* s);
int strcmp(const char* s1, const char* s2);
char* strcpy(char* dest, const char* src);
char* strncpy(char* dest, const char* src, int n);
void* memset(void* s, int c, unsigned long n);
void* memcpy(void* dest, const void* src, unsigned long n);

// ============================================================
// Print functions
// ============================================================

void print_string(const char* s);
void print_newline(void);
void print_char(char c);
void print_dec(unsigned long num);
void print_hex(unsigned long num);

// ============================================================
// Input functions
// ============================================================

int read_char(char* c);
int read_line(char* buf, int max_len);

#endif
