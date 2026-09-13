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
    printf("] ");

    char input_char = 0;
    while (1) {
        if (read(0, &input_char, 1) > 0) {
            if (input_char == '1') {
                printf("\n[SUCCESS] printf works in ring 3!\n] ");
            } else if (input_char == '2') {
                printf("\nExiting user shell environment...\n");
                break;
            }
        }
    }

    return 0;
}
