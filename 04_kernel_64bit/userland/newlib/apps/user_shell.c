#include <stdint.h>
#include <stddef.h>
#include <stdarg.h>

static inline void sys_reboot(void) {
    // Pass the registered index number 25 straight to the RAX register
    __asm__ volatile("mov $25, %%rax\n\tsyscall\n\t" ::: "rax");
}

// Direct, verified kernel system call wrappers
static inline long sys_write(int fd, const void *buf, size_t count) {
    long ret;
    __asm__ volatile (
        "syscall"
        : "=a"(ret)
        : "a"(1), "D"(fd), "S"(buf), "d"(count)
        : "rcx", "r11", "memory"
    );
    return ret;
}

static inline long sys_read(int fd, void *buf, size_t count) {
    long ret;
    __asm__ volatile (
        "syscall"
        : "=a"(ret)
        : "a"(3), "D"(fd), "S"(buf), "d"(count)
        : "rcx", "r11", "memory"
    );
    return ret;
}

// NEW USER LAND VECTOR WRAPPER: Requests the kernel process list securely via vector 20
static inline void sys_proclist(void) {
    __asm__ volatile (
        "syscall"
        :
        : "a"(20)  // Maps directly to SYS_PROCLIST
        : "rcx", "r11", "memory"
    );
}

// Local User-Space Memory Pool Bump Allocator
#define LOCAL_HEAP_SIZE 65536
static uint8_t user_heap_pool[LOCAL_HEAP_SIZE];
static size_t user_heap_idx = 0;

void* malloc(size_t size) {
    size = (size + 7) & ~7; // 8-byte alignment
    if (user_heap_idx + size > LOCAL_HEAP_SIZE) return NULL;
    void* allocated_ptr = &user_heap_pool[user_heap_idx];
    user_heap_idx += size;
    return allocated_ptr;
}

static void printf_string(const char* s) {
    size_t len = 0;
    while (s[len]) len++;
    sys_write(1, s, len);
}

static void printf_int(int64_t val, int base) {
    char buf[64]; int i = 0; uint64_t uval = val;
    if (val < 0 && base == 10) { printf_string("-"); uval = -val; }
    if (uval == 0) { printf_string("0"); return; }
    while (uval > 0) {
        int rem = uval % base;
        buf[i++] = (rem < 10) ? (rem + '0') : (rem - 10 + 'a');
        uval /= base;
    }
    char reverse[64];
    for (int j = 0; j < i; j++) reverse[j] = buf[i - 1 - j];
    reverse[i] = 0; printf_string(reverse);
}

int printf(const char* format, ...) {
    va_list args; va_start(args, format);
    while (*format) {
        if (*format == '%') {
            format++;
            if (*format == 's') printf_string(va_arg(args, char*));
            else if (*format == 'd') printf_int(va_arg(args, int), 10);
            else if (*format == 'x') printf_int(va_arg(args, unsigned int), 16);
            else if (*format == 'p') { printf_string("0x"); printf_int((uintptr_t)va_arg(args, void*), 16); }
            else if (*format == '%') printf_string("%");
        } else {
            char c[2] = {*format, 0}; sys_write(1, c, 1);
        }
        format++;
    }
    va_end(args); return 0;
}

void readline(char* buf, size_t max_len) {
    size_t idx = 0;
    while (idx < max_len - 1) {
        char c; long n = sys_read(0, &c, 1);
        if (n <= 0) continue;
        if (c == '\n' || c == '\r') { printf("\n"); break; }
        else if (c == '\b' || c == 0x7F) { if (idx > 0) { idx--; printf("\b \b"); } }
        else { buf[idx++] = c; char echo[2] = {c, 0}; sys_write(1, echo, 1); }
    }
    buf[idx] = 0;
}

int main(void) {
    char input_buffer[16];
    printf("\n===================================\n");
    printf("   Don's OS USER_SHELL (IMMORTAL)\n");
    printf("===================================\n");

    while (1) {
        printf("\nMenu Options:\n");
        printf(" 1. Test Dynamic Memory (malloc)\n");
        printf(" 2. Trigger Scheduler Workload Profile\n");
        printf(" 3. View System Process Queue (proclist)\n");
        printf(" 4. Reboot\n");
        printf("Enter selection: ");

        readline(input_buffer, sizeof(input_buffer));

        if (input_buffer[0] == '1') {
            printf("[HEAP] Allocating target string object buffer...\n");
            char* ptr = (char*)malloc(32);
            if (ptr) printf("[HEAP] Allocation verified at address: %p\n", ptr);
            else printf("[ERR] Dynamic allocation failed.\n");
        } else if (input_buffer[0] == '2') {
            printf("[SCHED] Running active task loop profile...\n");
            for (volatile int i = 0; i < 5000000; i++);
            printf("[SCHED] Multi-pass computing batch complete.\n");
        } else if (input_buffer[0] == '3') {
            printf("[SYSCALL] Requesting kernel process list snapshot...\n");
            // Invoke our secure system call vector
            sys_proclist();
        } else if (input_buffer[0] == '4') {
            printf("\nIssuing hardware reset request...\n");
            sys_reboot();
        } else if (input_buffer[0] == '\0') {
            continue;
        } else {
            printf("[ERR] Unknown selection sequence.\n");
        }
    }
    return 0;
}










//~ #include <stdint.h>
//~ #include <stddef.h>
//~ #include <stdarg.h>

//~ // Direct, verified kernel system call wrappers
//~ static inline long sys_write(int fd, const void *buf, size_t count) {
    //~ long ret;
    //~ __asm__ volatile (
        //~ "syscall"
        //~ : "=a"(ret)
        //~ : "a"(1), "D"(fd), "S"(buf), "d"(count)
        //~ : "rcx", "r11", "memory"
    //~ );
    //~ return ret;
//~ }

//~ static inline long sys_read(int fd, void *buf, size_t count) {
    //~ long ret;
    //~ __asm__ volatile (
        //~ "syscall"
        //~ : "=a"(ret)
        //~ : "a"(3), "D"(fd), "S"(buf), "d"(count)
        //~ : "rcx", "r11", "memory"
    //~ );
    //~ return ret;
//~ }

//~ static inline void* sys_brk(void* addr) {
    //~ void* ret;
    //~ __asm__ volatile (
        //~ "syscall"
        //~ : "=a"(ret)
        //~ : "a"(10), "D"(addr)
        //~ : "rcx", "r11", "memory"
    //~ );
    //~ return ret;
//~ }

//~ // Standalone Free Heap (Bump Allocator)
//~ static uint8_t* heap_brk = NULL;
//~ void* malloc(size_t size) {
    //~ if (heap_brk == NULL) {
        //~ heap_brk = (uint8_t*)sys_brk(NULL);
    //~ }
    //~ size = (size + 7) & ~7; // Align block requirements to 8 bytes
    //~ uint8_t* current = heap_brk;
    //~ void* next = sys_brk(current + size);
    //~ if (next == (void*)-1) return NULL;
    //~ heap_brk = current + size;
    //~ return current;
//~ }

//~ // Standalone formatting loop: supports %s, %d, %x, %p
//~ static void printf_string(const char* s) {
    //~ size_t len = 0;
    //~ while (s[len]) len++;
    //~ sys_write(1, s, len);
//~ }

//~ static void printf_int(int64_t val, int base) {
    //~ char buf[64];
    //~ int i = 0;
    //~ uint64_t uval = val;
    //~ if (val < 0 && base == 10) {
        //~ printf_string("-");
        //~ uval = -val;
    //~ }
    //~ if (uval == 0) {
        //~ printf_string("0");
        //~ return;
    //~ }
    //~ while (uval > 0) {
        //~ int rem = uval % base;
        //~ buf[i++] = (rem < 10) ? (rem + '0') : (rem - 10 + 'a');
        //~ uval /= base;
    //~ }
    //~ char reverse[64];
    //~ for (int j = 0; j < i; j++) {
        //~ reverse[j] = buf[i - 1 - j];
    //~ }
    //~ reverse[i] = 0;
    //~ printf_string(reverse);
//~ }

//~ int printf(const char* format, ...) {
    //~ va_list args;
    //~ va_start(args, format);
    //~ while (*format) {
        //~ if (*format == '%') {
            //~ format++;
            //~ if (*format == 's') {
                //~ printf_string(va_arg(args, char*));
            //~ } else if (*format == 'd') {
                //~ printf_int(va_arg(args, int), 10);
            //~ } else if (*format == 'x') {
                //~ printf_int(va_arg(args, unsigned int), 16);
            //~ } else if (*format == 'p') {
                //~ printf_string("0x");
                //~ printf_int((uintptr_t)va_arg(args, void*), 16);
            //~ } else if (*format == '%') {
                //~ printf_string("%");
            //~ }
        //~ } else {
            //~ char c[2] = {*format, 0};
            //~ sys_write(1, c, 1);
        //~ }
        //~ format++;
    //~ }
    //~ va_end(args);
    //~ return 0;
//~ }

//~ // Custom User Space Text Parser
//~ void readline(char* buf, size_t max_len) {
    //~ size_t idx = 0;
    //~ while (idx < max_len - 1) {
        //~ char c;
        //~ long n = sys_read(0, &c, 1);
        //~ if (n <= 0) continue;
        //~ if (c == '\n' || c == '\r') {
            //~ printf("\n");
            //~ break;
        //~ } else if (c == '\b' || c == 0x7F) { // Backspace handling
            //~ if (idx > 0) {
                //~ idx--;
                //~ printf("\b \b");
            //~ }
        //~ } else {
            //~ buf[idx++] = c;
            //~ char echo[2] = {c, 0};
            //~ sys_write(1, echo, 1);
        //~ }
    //~ }
    //~ buf[idx] = 0;
//~ }

//~ int main(void) {
    //~ char input_buffer[16];
    //~ printf("\n===================================\n");
    //~ printf("   HERMES OS USER MODE SUB-SHELL\n");
    //~ printf("===================================\n");

    //~ while (1) {
        //~ printf("\nMenu Options:\n");
        //~ printf(" 1. Test Dynamic Memory (malloc)\n");
        //~ printf(" 2. Trigger Scheduler Workload Profile\n");
        //~ printf(" 3. Exit Shell\n");
        //~ printf("Enter selection: ");

        //~ readline(input_buffer, sizeof(input_buffer));

        //~ if (input_buffer[0] == '1') {
            //~ printf("[HEAP] Allocating target string object buffer...\n");
            //~ char* ptr = (char*)malloc(32);
            //~ if (ptr) {
                //~ printf("[HEAP] Allocation verified at address: %p\n", ptr);
            //~ } else {
                //~ printf("[ERR] Dynamic allocation failed.\n");
            //~ }
        //~ } else if (input_buffer[0] == '2') {
            //~ printf("[SCHED] Running active task loop profile to yield control elements...\n");
            //~ for (volatile int i = 0; i < 5000000; i++);
            //~ printf("[SCHED] Multi-pass computing batch complete.\n");
        //~ } else if (input_buffer[0] == '3') {
            //~ printf("Terminating Ring 3 Execution framework.\n");
            //~ break;
        //~ } else {
            //~ printf("[ERR] Unknown selection sequence.\n");
        //~ }
    //~ }
    //~ return 0;
//~ }



//~ #include <stddef.h>
//~ #include <stdint.h>

//~ static inline long sys_write(int fd, const void *buf, size_t count) {
    //~ long ret;
    //~ __asm__ volatile (
        //~ "syscall"
        //~ : "=a"(ret)
        //~ : "a"(1), "D"(fd), "S"(buf), "d"(count)
        //~ : "rcx", "r11", "memory"
    //~ );
    //~ return ret;
//~ }


//~ // Print a string using a single sys_write
//~ static void print_str(const char *s) {
    //~ size_t len = 0;
    //~ while (s[len]) len++;
    //~ if (len) sys_write(1, s, len);
//~ }

//~ int main(void) {
    //~ print_str("Hello from userland!\n");
    //~ print_str("This is a second line.\n");
    //~ print_str("And a third.\n");

    //~ while (1) __asm__ volatile("pause");
    //~ return 0;
//~ }


//~ #include <stddef.h>
//~ #include <stdint.h>
//~ #include <stdio.h>

//~ // External declarations
//~ extern struct _reent my_reent;
//~ extern void __sinit(struct _reent *);

//~ static inline long sys_write(int fd, const void *buf, size_t count) {
    //~ long ret;
    //~ __asm__ volatile (
        //~ "syscall"
        //~ : "=a"(ret)
        //~ : "a"(1), "D"(fd), "S"(buf), "d"(count)
        //~ : "rcx", "r11", "memory"
    //~ );
    //~ return ret;
//~ }

//~ int main(void) {
    //~ // Allocate heap for stdio buffers
    //~ extern char *sbrk(ptrdiff_t inc);
    //~ if (sbrk(4096) == (char*)-1) {
        //~ sys_write(1, "Heap allocation failed!\n", 24);
        //~ while(1) __asm__ volatile("pause");
    //~ }

    //~ // CRITICAL: initialise the standard streams for our reent structure
    //~ __sinit(&my_reent);

    //~ // Now printf and puts will use properly initialised FILE structures
    //~ printf("Hello from printf!\n");
    //~ puts("And puts works too.");

    //~ while(1) __asm__ volatile("pause");
    //~ return 0;
//~ }

//~ #include <unistd.h>

//~ int main(void) {
    //~ const char *msg = "Hello from userland!\n";
    //~ write(1, msg, 22);
    //~ while (1) __asm__ volatile("pause");
    //~ return 0;
//~ }

























//~ #include <stdio.h>
//~ #include <unistd.h>

//~ int main(void) {
    //~ // Test write first
    ///~ #include <stdio.h>
//~ #include <unistd.h>
//~ #include <string.h>

//~ int main(void) {
    //~ // Test direct write (uses Newlib's write syscall wrapper)
    //~ const char *msg = "Testing printf:\n";
    //~ write(1, msg, strlen(msg));
    
    //~ // Test Newlib printf with various formats
    //~ printf("Hello from Newlib printf!\n");
    //~ printf("Number: %d, Hex: 0x%X, String: '%s'\n", 42, 42, "This is a test");
    //~ printf("Float: %.2f\n", 3.14159);
    //~ printf("Pointer: %p\n", (void*)0x8000000000);
    
    //~ // Flush stdout to ensure output is sent
    //~ fflush(stdout);
    
    //~ while (1) __asm__ volatile("pause");
    //~ return 0;
//~ }/~ write(1, "Testing printf:\n", 16);
    
    //~ // Test Newlib printf
    //~ printf("Hello from Newlib printf!\n");
    //~ printf("Number: %d, Hex: 0x%X, String: '%s'\n", 42, 42, "This is a test");
    
    //~ // Flush stdout
    //~ fflush(stdout);
    
    //~ while (1) __asm__ volatile("pause");
    //~ return 0;
//~ }






//~ #include <unistd.h>

//~ // Declare _write explicitly
//~ extern ssize_t _write(int fd, const void *buf, size_t count);

//~ int main(void) {
    //~ const char *msg = "Hello from userland!\n";
    //~ _write(1, msg, 22);  // Direct syscall, no strlen
    //~ while (1) __asm__ volatile("pause");
    //~ return 0;
//~ }

//~ #include <stdio.h>
//~ #include <unistd.h>
//~ #include <string.h>

//~ int main(void) {
    //~ // Test direct write (uses Newlib's write syscall wrapper)
    //~ const char *msg = "Testing printf:\n";
    //~ write(1, msg, strlen(msg));
    
    //~ // Test Newlib printf with various formats
    //~ printf("Hello from Newlib printf!\n");
    //~ printf("Number: %d, Hex: 0x%X, String: '%s'\n", 42, 42, "This is a test");
    //~ printf("Float: %.2f\n", 3.14159);
    //~ printf("Pointer: %p\n", (void*)0x8000000000);
    
    //~ // Flush stdout to ensure output is sent
    //~ fflush(stdout);
    
    //~ while (1) __asm__ volatile("pause");
    //~ return 0;
//~ }



//~ #include <stddef.h>
//~ #include <unistd.h>   // for ssize_t
//~ #include <stdio.h>

//~ ssize_t _write(int fd, const void *buf, size_t count);

//~ static void myputchar(char c) {
    //~ _write(1, &c, 1);
//~ }

//~ static void print_str(const char *s) {
    //~ while (*s) myputchar(*s++);
//~ }

//~ static void print_dec(unsigned int n) {
    //~ char buf[16];
    //~ int i = 0;
    //~ if (n == 0) { myputchar('0'); return; }
    //~ while (n > 0) {
        //~ buf[i++] = '0' + (n % 10);
        //~ n /= 10;
    //~ }
    //~ while (i > 0) myputchar(buf[--i]);
//~ }

//~ static void print_hex(unsigned int n) {
    //~ char buf[16];
    //~ int i = 0;
    //~ if (n == 0) { myputchar('0'); return; }
    //~ while (n > 0) {
        //~ int d = n & 0xF;
        //~ buf[i++] = (d < 10) ? ('0' + d) : ('A' + d - 10);
        //~ n >>= 4;
    //~ }
    //~ while (i > 0) myputchar(buf[--i]);
//~ }

//~ int myprintf(const char *fmt, ...) {
    //~ // Simple version without va_list to avoid alignment issues.
    //~ // For now, we just print static strings and a few fixed values.
    //~ const char *p = fmt;
    //~ while (*p) {
        //~ if (*p == '%') {
            //~ p++;
            //~ switch (*p) {
                //~ case 'd': {
                    //~ print_dec(42);   // fixed example
                    //~ break;
                //~ }
                //~ case 'x': {
                    //~ print_hex(0xABCD);
                    //~ break;
                //~ }
                //~ case 's': {
                    //~ print_str("(string)");
                    //~ break;
                //~ }
                //~ case 'c': {
                    //~ myputchar('X');
                    //~ break;
                //~ }
                //~ case '%': {
                    //~ myputchar('%');
                    //~ break;
                //~ }
                //~ default:
                    //~ myputchar('%');
                    //~ myputchar(*p);
                    //~ break;
            //~ }
        //~ } else {
            //~ myputchar(*p);
        //~ }
        //~ p++;
    //~ }
    //~ return 0;
//~ }

//~ int main(void) {
    //~ const char *msg1 = "Hello from custom myprintf!\n";
    //~ _write(1, msg1, __builtin_strlen(msg1));

    //~ //printf("Hello from Newlib printf!\n");

    //~ myprintf("Number: %d, Hex: %x, String: %s, Char: %c\n");

    //~ while (1) __asm__ volatile("pause");
    //~ return 0;
//~ }
