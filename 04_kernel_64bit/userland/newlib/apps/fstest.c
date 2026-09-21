/*
 * fstest.c — external port of the shell's old option 4 (write) and
 * option 5 (persistence check).
 *
 * Default mode (no arguments, or any argv that is not "--verify"):
 *   Creates 0:/USER2.TXT, writes a known string, closes, reopens,
 *   reads back, and verifies byte-exact.  This is old option 4.
 *
 * --verify mode (argv[1] == "--verify"):
 *   Opens 0:/USER2.TXT and verifies its contents against the same
 *   known string.  This is old option 5: run fstest once, reboot,
 *   then run fstest --verify.
 *
 * Until Stage 4 (argv passing) lands, spawned programs always see
 * argc == 0, so this binary always runs the write test.  When argv
 * works, --verify becomes reachable with no change to this file.
 *
 * Exits 0 on pass, 1 on failure.
 */

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>

static const char *TEST_PATH = "0:/USER2.TXT";
static const char *TEST_MSG  = "Hello from ring 3!\r\n";

/* ============================================================
 * Write mode: create, write, close, reopen, read, verify.
 * Returns 0 on pass, 1 on failure.
 * ============================================================ */
static int do_write_test(void) {
    printf("[fstest] step 1: open for write/create/truncate\n");
    int fd = open(TEST_PATH, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    printf("  open() returned %d\n", fd);
    if (fd < 0) {
        printf("[fstest] FAIL: open for write (fd=%d)\n", fd);
        return 1;
    }

    printf("[fstest] step 2: write a known string\n");
    int msg_len = (int)strlen(TEST_MSG);
    int n = write(fd, TEST_MSG, msg_len);
    printf("  write() returned %d (expected %d)\n", n, msg_len);
    if (n != msg_len) {
        printf("[fstest] FAIL: write (short or error)\n");
        close(fd);
        return 1;
    }

    printf("[fstest] step 3: close\n");
    int cr = close(fd);
    printf("  close() returned %d\n", cr);
    if (cr != 0) {
        printf("[fstest] FAIL: close\n");
        return 1;
    }

    printf("[fstest] step 4: reopen for read\n");
    int rfd = open(TEST_PATH, O_RDONLY, 0);
    printf("  open(O_RDONLY) returned %d\n", rfd);
    if (rfd < 0) {
        printf("[fstest] FAIL: reopen (fd=%d)\n", rfd);
        return 1;
    }

    printf("[fstest] step 5: read it back\n");
    char rbuf[64];
    memset(rbuf, 0, sizeof(rbuf));
    int rn = read(rfd, rbuf, sizeof(rbuf) - 1);
    printf("  read() returned %d\n", rn);
    printf("  content: \"%s\"\n", rbuf);
    close(rfd);

    if (rn == msg_len && memcmp(rbuf, TEST_MSG, msg_len) == 0) {
        printf("[fstest] PASS (round-trip byte-exact)\n");
        return 0;
    }
    if (rn == msg_len) {
        printf("[fstest] FAIL: same length, different bytes\n");
    } else {
        printf("[fstest] FAIL: length mismatch\n");
    }
    return 1;
}

/* ============================================================
 * Verify mode: open, read, compare.  Does not create the file.
 * Returns 0 on pass, 1 on failure.
 * ============================================================ */
static int do_verify_test(void) {
    printf("[fstest] persistence check (--verify)\n");

    int fd = open(TEST_PATH, O_RDONLY, 0);
    if (fd < 0) {
        printf("  %s not found (fd=%d)\n", TEST_PATH, fd);
        printf("  Run fstest, reboot, then run fstest --verify.\n");
        printf("[fstest] FAIL: no prior write\n");
        return 1;
    }

    char rbuf[64];
    memset(rbuf, 0, sizeof(rbuf));
    int rn = read(fd, rbuf, sizeof(rbuf) - 1);
    close(fd);

    int expected_len = (int)strlen(TEST_MSG);
    printf("  read %d bytes: \"%s\"\n", rn, rbuf);

    if (rn == expected_len && memcmp(rbuf, TEST_MSG, expected_len) == 0) {
        printf("[fstest] PASS (file survived reboot, byte-exact)\n");
        return 0;
    }
    printf("[fstest] FAIL: length or content mismatch\n");
    return 1;
}

int main(int argc, char **argv) {
    setvbuf(stdout, NULL, _IONBF, 0);

    if (argc > 1 && strcmp(argv[1], "--verify") == 0) {
        return do_verify_test();
    }
    return do_write_test();
}
