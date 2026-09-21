/*
 * cat.c — print one file's contents to stdout.
 *
 * First userland program that consumes argv.  Usage:
 *     cat FILE
 * FILE is opened as "0:/FILE" (case-insensitive via FatFs).
 *
 * Exits 0 on success, 1 on usage error, 2 on open failure,
 * 3 on read error.
 */

#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>

int main(int argc, char **argv) {
    setvbuf(stdout, NULL, _IONBF, 0);

    if (argc < 2) {
        printf("usage: cat FILE\n");
        return 1;
    }

    char path[64];
    int n = snprintf(path, sizeof(path), "0:/%s", argv[1]);
    if (n < 0 || n >= (int)sizeof(path)) {
        printf("cat: path too long\n");
        return 1;
    }

    int fd = open(path, O_RDONLY, 0);
    if (fd < 0) {
        printf("cat: %s: cannot open\n", path);
        return 2;
    }

    char buf[512];
    for (;;) {
        int r = read(fd, buf, sizeof(buf));
        if (r < 0) {
            printf("\ncat: read error\n");
            close(fd);
            return 3;
        }
        if (r == 0) break;
        if (write(1, buf, r) != r) {
            close(fd);
            return 3;
        }
    }

    close(fd);
    return 0;
}
