// crt0.c - Startup code for Picolibc
extern int main(void);
extern void _exit(int status);

void _start(void) {
    // Call main and exit with the return value
    int status = main();
    _exit(status);
}
