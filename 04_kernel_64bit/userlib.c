#include "include/userlib.h"

// ============================================================
// System calls
// ============================================================

long sys_write(int fd, const void* buf, unsigned long count) {
    long ret;
    __asm__ volatile (
        "syscall"
        : "=a"(ret)
        : "a"(1), "D"(fd), "S"(buf), "d"(count)
        : "rcx", "r11", "memory"
    );
    return ret;
}

long sys_read(int fd, void* buf, unsigned long count) {
    long ret;
    __asm__ volatile (
        "syscall"
        : "=a"(ret)
        : "a"(3), "D"(fd), "S"(buf), "d"(count)
        : "rcx", "r11", "memory"
    );
    return ret;
}

void sys_exit(int status) {
    __asm__ volatile (
        "syscall"
        :
        : "a"(2), "D"(status)
        : "rcx", "r11", "memory"
    );
    __builtin_unreachable();
}

// ============================================================
// String functions
// ============================================================

int strlen(const char* s) {
    int len = 0;
    while (s[len]) len++;
    return len;
}

int strcmp(const char* s1, const char* s2) {
    while (*s1 && (*s1 == *s2)) {
        s1++;
        s2++;
    }
    return *(const unsigned char*)s1 - *(const unsigned char*)s2;
}

char* strcpy(char* dest, const char* src) {
    char* d = dest;
    while (*src) {
        *d++ = *src++;
    }
    *d = '\0';
    return dest;
}

char* strncpy(char* dest, const char* src, int n) {
    char* d = dest;
    int i;
    for (i = 0; i < n && src[i]; i++) {
        d[i] = src[i];
    }
    for (; i < n; i++) {
        d[i] = '\0';
    }
    return dest;
}

void* memset(void* s, int c, unsigned long n) {
    unsigned char* p = (unsigned char*)s;
    for (unsigned long i = 0; i < n; i++) {
        p[i] = (unsigned char)c;
    }
    return s;
}

void* memcpy(void* dest, const void* src, unsigned long n) {
    unsigned char* d = (unsigned char*)dest;
    const unsigned char* s = (const unsigned char*)src;
    for (unsigned long i = 0; i < n; i++) {
        d[i] = s[i];
    }
    return dest;
}

// ============================================================
// Print functions
// ============================================================

void print_string(const char* s) {
    sys_write(1, s, strlen(s));
}

void print_newline(void) {
    sys_write(1, "\n", 1);
}

void print_char(char c) {
    sys_write(1, &c, 1);
}

void print_dec(unsigned long num) {
    char buf[32];
    int pos = 0;
    
    if (num == 0) {
        print_char('0');
        return;
    }
    
    while (num > 0) {
        buf[pos++] = '0' + (num % 10);
        num /= 10;
    }
    
    while (pos > 0) {
        print_char(buf[--pos]);
    }
}

void print_hex(unsigned long num) {
    const char hex[] = "0123456789ABCDEF";
    char buf[16];
    int pos = 0;
    
    if (num == 0) {
        print_string("0x0");
        return;
    }
    
    print_string("0x");
    
    while (num > 0) {
        buf[pos++] = hex[num & 0xF];
        num >>= 4;
    }
    
    while (pos > 0) {
        print_char(buf[--pos]);
    }
}

// ============================================================
// Input functions
// ============================================================

int read_char(char* c) {
    return sys_read(0, c, 1) == 1;
}

int read_line(char* buf, int max_len) {
    int pos = 0;
    char c;
    
    while (pos < max_len - 1) {
        if (!read_char(&c)) {
            continue;
        }
        
        if (c == '\n') {
            buf[pos] = '\0';
            // sys_read already echoes newline, so we don't need print_newline()
            return pos;
        } else if (c == '\b' || c == 127) {
            if (pos > 0) {
                pos--;
                sys_write(1, "\b \b", 3);
            }
        } else if (c >= 32 && c <= 126) {
            buf[pos] = c;
            pos++;
            // sys_read already echoes printable characters
        }
    }
    
    buf[max_len - 1] = '\0';
    print_newline();
    return pos;
}
