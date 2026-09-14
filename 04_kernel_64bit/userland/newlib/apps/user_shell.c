#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;

    /* Unbuffered stdout so every printf immediately hits sys_write. */
    //setvbuf(stdout, NULL, _IONBF, 0);

    printf("\n========================================\n");
    printf("   DonsDOS Native Newlib 4.x User Shell\n");
    printf("========================================\n\n");
    printf("Select option node:\n");
    printf("  1. Print a message via printf\n");
    printf("  2. Exit Runtime Environment\n");
    printf("  3. Test malloc/free via newlib\n\n");
    printf("] ");

    char input_char = 0;
    while (1) {
        if (read(0, &input_char, 1) > 0) {
            if (input_char == '1') {
                printf("\n[SUCCESS] printf works in ring 3!\n] ");
            } else if (input_char == '2') {
                printf("\nExiting user shell environment...\n");
                break;
            } else if (input_char == '3') {
                printf("\n[HEAP TEST] Testing newlib malloc/free via sbrk\n");

                size_t N = 4096;
                unsigned char* buf = (unsigned char*)malloc(N);
                if (!buf) {
                    printf("  malloc(%lu) failed (returned NULL)\n] ",
                           (unsigned long)N);
                    continue;
                }
                printf("  malloc(%lu) returned %p\n", (unsigned long)N, (void*)buf);

                /* Write a pattern across the whole buffer. This is the
                   critical test: it touches every byte of the newly
                   mapped user page. If sys_brk didn't map the page, or
                   didn't invlpg, this write faults. */
                for (size_t i = 0; i < N; i++) {
                    buf[i] = (unsigned char)(i & 0xFF);
                }
                printf("  wrote %lu bytes at %p\n",
                       (unsigned long)N, (void*)buf);

                /* Read back and verify. */
                int ok = 1;
                for (size_t i = 0; i < N; i++) {
                    if (buf[i] != (unsigned char)(i & 0xFF)) {
                        printf("  MISMATCH at offset %lu: got 0x%02x, expected 0x%02x\n",
                               (unsigned long)i, buf[i],
                               (unsigned char)(i & 0xFF));
                        ok = 0;
                        break;
                    }
                }
                printf("  read back: %s\n", ok ? "OK" : "FAIL");

                free(buf);
                printf("  free() returned\n");

                /* Optional: a second allocation to see whether newlib
                   reuses the freed block or grows the heap again. */
                size_t M = 8192;
                unsigned char* buf2 = (unsigned char*)malloc(M);
                if (buf2) {
                    printf("  second malloc(%lu) returned %p\n",
                           (unsigned long)M, (void*)buf2);
                    for (size_t i = 0; i < M; i++) buf2[i] = (unsigned char)(0xAA);
                    printf("  wrote %lu bytes at second buffer\n",
                           (unsigned long)M);
                    free(buf2);
                } else {
                    printf("  second malloc(%lu) failed\n",
                           (unsigned long)M);
                }

                printf("[HEAP TEST] done\n] ");
            }
        }
    }

    return 0;
}











//~ #include <stdio.h>
//~ #include <unistd.h>

//~ int main(int argc, char** argv) {
    //~ (void)argc;
    //~ (void)argv;

    //~ /* Unbuffered stdout so every printf immediately hits sys_write. */
    //~ setvbuf(stdout, NULL, _IONBF, 0);

    //~ printf("\n========================================\n");
    //~ printf("   DonsDOS Native Newlib 4.x User Shell\n");
    //~ printf("========================================\n\n");
    //~ printf("Select option node:\n");
    //~ printf("  1. Print a message via printf\n");
    //~ printf("  2. Exit Runtime Environment\n\n");
    //~ printf("] ");

    //~ char input_char = 0;
    //~ while (1) {
        //~ if (read(0, &input_char, 1) > 0) {
            //~ if (input_char == '1') {
                //~ printf("\n[SUCCESS] printf works in ring 3!\n] ");
            //~ } else if (input_char == '2') {
                //~ printf("\nExiting user shell environment...\n");
                //~ break;
            //~ }
        //~ }
    //~ }

    //~ return 0;
//~ }
