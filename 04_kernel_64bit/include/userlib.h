#ifndef USERLIB_H
#define USERLIB_H

#include <stddef.h>   // for size_t

// Syscall wrappers (userspace → kernel)
long sys_write(int fd, const void* buf, size_t count);
long sys_read(int fd, void* buf, size_t count);
void sys_exit(int code);

// User I/O helpers
void print_string(const char* s);
void print_char(char c);
int  read_char(char* c);
int  read_line(char* buf, size_t max_len);

#endif
