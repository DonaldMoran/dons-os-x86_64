#include <stdio.h>
#include <unistd.h>

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;

    /* Unbuffered stdout so every printf immediately hits sys_write. */
    setvbuf(stdout, NULL, _IONBF, 0);

    printf("\n========================================\n");
    printf("   DonsDOS Native Newlib 4.x User Shell\n");
    printf("========================================\n\n");
    printf("Select option node:\n");
    printf("  1. Print a message via printf\n");
    printf("  2. Exit Runtime Environment\n\n");
    printf("> ");

    char input_char = 0;
    while (1) {
        if (read(0, &input_char, 1) > 0) {
            if (input_char == '1') {
                printf("\n[SUCCESS] printf works in ring 3!\n> ");
            } else if (input_char == '2') {
                printf("\nExiting user shell environment...\n");
                break;
            }
        }
    }

    return 0;
}








//~ #include <stdio.h>
//~ #include <unistd.h>

//~ /* Clean inline string length helper */
//~ static inline size_t user_strlen(const char* str) {
    //~ size_t len = 0;
    //~ while (str[len]) len++;
    //~ return len;
//~ }

//~ /* Register-level raw kernel write trap */
//~ static void direct_serial_write(const char* str) {
    //~ size_t len = user_strlen(str);
    //~ if (len == 0) return;

    //~ register long rax __asm__("rax") = 1;
    //~ register long rdi __asm__("rdi") = 1;
    //~ register const char* rsi __asm__("rsi") = str;
    //~ register size_t rdx __asm__("rdx") = len;

    //~ __asm__ volatile (
        //~ "syscall"
        //~ : "+r"(rax)
        //~ : "r"(rdi), "r"(rsi), "r"(rdx)
        //~ : "rcx", "r11", "memory"
    //~ );
//~ }

//~ /* Print a 64-bit value as 0x + 16 hex digits + newline */
//~ static void print_hex_label(const char *label, unsigned long val) {
    //~ char buf[40];
    //~ static const char hx[] = "0123456789abcdef";
    //~ int i = 0;
    //~ while (label[i] && i < 12) { buf[i] = label[i]; i++; }
    //~ buf[i++] = '=';
    //~ buf[i++] = '0';
    //~ buf[i++] = 'x';
    //~ for (int k = 15; k >= 0; k--) {
        //~ buf[i++] = hx[(val >> (k * 4)) & 0xF];
    //~ }
    //~ buf[i++] = '\n';
    //~ buf[i] = '\0';
    //~ direct_serial_write(buf);
//~ }

//~ int main(int argc, char** argv) {
    //~ (void)argc;
    //~ (void)argv;

    //~ extern struct _reent *_impure_ptr;
    //~ extern struct _reent _impure_data;

    //~ direct_serial_write("M1\n");
    //~ direct_serial_write("M2\n");
    //~ direct_serial_write("M3\n");
    //~ direct_serial_write("P1\n");

    //~ /* ---- EXPERIMENT: fixed-value call to print_hex_label ---- */
    //~ direct_serial_write("Q1\n");
    //~ print_hex_label("test", 0x1122334455667788UL);
    //~ direct_serial_write("Q2\n");
    //~ /* ------------------------------------------------------- */

    //~ print_hex_label("imp_ptr",  (unsigned long)_impure_ptr);
    //~ direct_serial_write("P2\n");
    //~ print_hex_label("imp_data", (unsigned long)&_impure_data);
    //~ direct_serial_write("P3\n");
    //~ print_hex_label("sf0",      (unsigned long)&__sf[0]);
    //~ direct_serial_write("P4\n");
    //~ print_hex_label("sf1",      (unsigned long)&__sf[1]);
    //~ direct_serial_write("P5\n");
    //~ direct_serial_write("MAIN_DONE\n");
    //~ return 0;
//~ }




//~ int main(int argc, char** argv) {
    //~ (void)argc;
    //~ (void)argv;

    //~ extern struct _reent *_impure_ptr;
    //~ extern struct _reent _impure_data;

    //~ /* ============================================================
     //~ * MAIN ISOLATION TEST
     //~ * Each line is a distinct, short write so we can see exactly
     //~ * which one lands and which does not.
     //~ * ============================================================ */

    //~ direct_serial_write("M1\n");
    //~ direct_serial_write("M2\n");
    //~ direct_serial_write("M3\n");

    //~ /* Now the pointer diagnostics, in a stable order. */
    //~ print_hex_label("imp_ptr",  (unsigned long)_impure_ptr);
    //~ print_hex_label("imp_data", (unsigned long)&_impure_data);
    //~ print_hex_label("sf0",      (unsigned long)&__sf[0]);
    //~ print_hex_label("sf1",      (unsigned long)&__sf[1]);

    //~ direct_serial_write("MAIN_DONE\n");

    //~ /* Do not loop; exit cleanly so the kernel logs its exit path. */
    //~ return 0;
//~ }






//~ #include <stdio.h>
//~ #include <unistd.h>

//~ int main(int argc, char** argv) {
    //~ // Explicitly cast unused parameters to suppress warnings
    //~ (void)argc;
    //~ (void)argv;

    //~ /* 1. Disable all internal Newlib stream buffering entirely for stdout.
       //~ This forces every single printf statement to immediately trigger 
       //~ your fixed sys_write kernel call instead of caching characters! */
    //~ setvbuf(stdout, NULL, _IONBF, 0);

    //~ printf("\n========================================\n");
    //~ printf("   DonsDOS Native Newlib 4.x User Shell   \n");
    //~ printf("========================================\n\n");
    
    //~ printf("Select option node:\n");
    //~ printf("  1. Evaluate Dynamic Malloc Heap Allocations\n");
    //~ printf("  2. Exit Runtime Environment\n\n");
    //~ printf("> ");

    //~ /* 2. Interactive Loop via blocking system calls.
       //~ By using read(), the process waits inside the kernel's safe kbd_buffer_get loop
       //~ instead of spinning in a tight userland loop that triggers page faults! */
    //~ char input_char = 0;
    //~ while (1) {
        //~ // Read 1 byte from standard input (File Descriptor 0)
        //~ if (read(0, &input_char, 1) > 0) {
            
            //~ if (input_char == '1') {
                //~ printf("\n[SUCCESS] Running Option 1: Allocating user memory loop...\n> ");
            //~ } 
            //~ else if (input_char == '2') {
                //~ printf("\nExiting user shell environment...\n");
                //~ break; // Gracefully breaks the loop to hit return 0 and exit cleanly
            //~ }
        //~ }
    //~ }

    //~ return 0;
//~ }











//~ #include <stdio.h>
//~ #include <unistd.h>

//~ int main(int argc, char** argv) {
    //~ /* Disable all internal Newlib stream buffering entirely for stdout.
       //~ This forces every single printf statement to immediately trigger 
       //~ a sys_write kernel call back to your supervisor pipeline! */
    //~ setvbuf(stdout, NULL, _IONBF, 0);

    //~ printf("\n========================================\n");
    //~ printf("   DonsDOS Native Newlib 4.x User Shell   \n");
    //~ printf("========================================\n\n");
    
    //~ printf("Select option node:\n");
    //~ printf("  1. Evaluate Dynamic Malloc Heap Allocations\n");
    //~ printf("  2. Exit Runtime Environment\n\n");
    //~ printf("> ");

    //~ /* Defensive synchronization fallback */
    //~ fflush(stdout);

    //~ return 0;
//~ }










//~ #include <stdio.h>
//~ #include <stdlib.h>
//~ #include <string.h>

//~ // Forward declare your system reboot wrapper from syscalls.c
//~ void sys_reboot(void);

//~ int main(void) {
    //~ char input_buffer[32];

    //~ // CRITICAL: Disable stdio line-buffering so printf calls immediately 
    //~ // fall through to your sys_write handler without waiting for newline traits.
    //~ setvbuf(stdout, NULL, _IONBF, 0);

    //~ printf("\n=============================================\n");
    //~ printf("  DonsDOS Fully Custom Newlib 4.x User Shell \n");
    //~ printf("=============================================\n");

    //~ while (1) {
        //~ printf("\nMenu Selection:\n");
        //~ printf(" 1. Test Newlib Dynamic Heap Allocation (malloc/free)\n");
        //~ printf(" 2. Run Busy Work Execution Loop\n");
        //~ printf(" 3. Perform Clean Motherboard Reboot\n");
        //~ printf("Enter option number: ");

        //~ // Retrieve input cleanly via Newlib's standard input processing stream
        //~ if (fgets(input_buffer, sizeof(input_buffer), stdin) == NULL) {
            //~ continue;
        //~ }

        //~ // Clean out trailing carriage returns or line feeds
        //~ input_buffer[strcspn(input_buffer, "\r\n")] = 0;

        //~ if (strcmp(input_buffer, "1") == 0) {
            //~ printf("[HEAP] Requesting memory allocation slot from Newlib...\n");
            //~ char *dynamic_string = (char *)malloc(128);
            
            //~ if (dynamic_string) {
                //~ // Testing Newlib's dynamic string formatting engine
                //~ snprintf(dynamic_string, 128, "Dynamic heap cell allocated successfully at address: %p\n", (void*)dynamic_string);
                //~ printf("[SUCCESS] %s", dynamic_string);
                //~ free(dynamic_string);
            //~ } else {
                //~ printf("[FAILURE] Newlib heap allocation returned NULL!\n");
            //~ }
        //~ } 
        //~ else if (strcmp(input_buffer, "2") == 0) {
            //~ printf("[SCHED] Spinning execution loops to prompt preemptive slices...\n");
            //~ for (volatile int i = 0; i < 5000000; i++);
            //~ printf("[SCHED] Computing task batch successfully completed.\n");
        //~ } 
        //~ else if (strcmp(input_buffer, "3") == 0) {
            //~ printf("\n[SYSCALL] Dispatching Ring 3 triple-fault hard reset directive...\n");
            //~ sys_reboot();
        //~ } 
        //~ else if (strlen(input_buffer) == 0) {
            //~ continue;
        //~ } 
        //~ else {
            //~ printf("[ERROR] Unrecognized choice sequence: '%s'\n", input_buffer);
        //~ }
    //~ }

    //~ return 0;
//~ }
