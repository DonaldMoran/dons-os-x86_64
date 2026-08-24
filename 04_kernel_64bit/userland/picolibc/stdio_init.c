// stdio_init.c - Define stdout/stderr for Picolibc
#include <stdio.h>

// Declare the syscall stubs (defined in syscalls.c)
extern int _write(int file, char *ptr, int len);
extern int _read(int file, char *ptr, int len);

// Output for stdout
static int stdout_putc(char c, FILE *file) {
    (void) file;
    _write(1, &c, 1);
    return c;
}

// Output for stderr (same as stdout for now)
static int stderr_putc(char c, FILE *file) {
    (void) file;
    _write(2, &c, 1);
    return c;
}

// Input (will use your keyboard input via SYS_READ)
static int stdin_getc(FILE *file) {
    (void) file;
    char c;
    if (_read(0, &c, 1) > 0) {
        return (unsigned char)c;
    }
    return EOF;
}

// Define stdout, stdin, stderr
static FILE __stdout = FDEV_SETUP_STREAM(stdout_putc, NULL, NULL, _FDEV_SETUP_WRITE);
static FILE __stdin  = FDEV_SETUP_STREAM(NULL, stdin_getc, NULL, _FDEV_SETUP_READ);
static FILE __stderr = FDEV_SETUP_STREAM(stderr_putc, NULL, NULL, _FDEV_SETUP_WRITE);

FILE *const stdout = &__stdout;
FILE *const stdin = &__stdin;
FILE *const stderr = &__stderr;
