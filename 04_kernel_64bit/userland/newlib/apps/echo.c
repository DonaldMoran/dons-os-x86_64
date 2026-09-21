/*
 * echo.c — print argv[1..] separated by single spaces, followed by
 * a newline.
 *
 * Simplest argv consumer.  Exercises argc and argv end-to-end.
 */

#include <stdio.h>
#include <unistd.h>

int main(int argc, char **argv) {
    setvbuf(stdout, NULL, _IONBF, 0);

    for (int i = 1; i < argc; i++) {
        if (i > 1) write(1, " ", 1);
        const char *s = argv[i];
        size_t len = 0;
        while (s[len]) len++;
        write(1, s, len);
    }
    write(1, "\n", 1);
    return 0;
}
