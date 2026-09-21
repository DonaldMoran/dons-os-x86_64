/*
 * bigtest.c — external port of the shell's old option 7.
 *
 * Writes a 4 KB pattern to 0:/BIG.BIN, reads it back, verifies
 * byte-exact.  Pattern is (i * 7 + 3) & 0xFF, same as the old shell.
 *
 * Exits 0 on pass, 1 on failure.
 */

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>

#define BIG_N 4096

int main(void) {
    setvbuf(stdout, NULL, _IONBF, 0);

    printf("[bigtest] 4096-byte round-trip through FatFs\n");

    size_t N = BIG_N;
    unsigned char *wbuf = (unsigned char *)malloc(N);
    if (!wbuf) {
        printf("  malloc(%lu) failed\n", (unsigned long)N);
        printf("[bigtest] FAIL: write buffer alloc\n");
        return 1;
    }
    for (size_t i = 0; i < N; i++) {
        wbuf[i] = (unsigned char)((i * 7 + 3) & 0xFF);
    }

    int fd = open("0:/BIG.BIN", O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) {
        printf("  open: FAILED (fd=%d)\n", fd);
        free(wbuf);
        printf("[bigtest] FAIL: open for write\n");
        return 1;
    }
    int n = write(fd, wbuf, (int)N);
    close(fd);
    printf("  wrote %d / %lu bytes\n", n, (unsigned long)N);
    if (n != (int)N) {
        printf("[bigtest] FAIL: short write\n");
        free(wbuf);
        return 1;
    }

    unsigned char *rbuf = (unsigned char *)malloc(N);
    if (!rbuf) {
        printf("  second malloc failed\n");
        free(wbuf);
        printf("[bigtest] FAIL: read buffer alloc\n");
        return 1;
    }

    fd = open("0:/BIG.BIN", O_RDONLY, 0);
    if (fd < 0) {
        printf("  reopen: FAILED (fd=%d)\n", fd);
        free(wbuf);
        free(rbuf);
        printf("[bigtest] FAIL: reopen\n");
        return 1;
    }
    int rn = read(fd, rbuf, (int)N);
    close(fd);
    printf("  read %d / %lu bytes\n", rn, (unsigned long)N);

    int ok = (rn == (int)N) && (memcmp(wbuf, rbuf, N) == 0);
    free(wbuf);
    free(rbuf);

    if (ok) {
        printf("[bigtest] PASS (byte-exact 4 KB round-trip)\n");
        return 0;
    }
    printf("[bigtest] FAIL: length or content mismatch\n");
    return 1;
}
