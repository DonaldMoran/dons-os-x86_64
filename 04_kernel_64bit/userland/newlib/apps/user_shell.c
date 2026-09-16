#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>

/* ============================================================
 * Helper: print the menu.
 * ============================================================ */
static void print_menu(void) {
    printf("\n========================================\n");
    printf("   DonsDOS Native Newlib 4.x User Shell\n");
    printf("========================================\n\n");
    printf("Select option node:\n");
    printf("  1. Print a message via printf\n");
    printf("  2. Exit Runtime Environment\n");
    printf("  3. Test malloc/free via newlib\n");
    printf("  4. Test FatFs (create/write/read/close)\n");
    printf("  5. Persistence check (run after option 4 + reboot)\n");
    printf("  6. Multi-file test (create, list, delete)\n");
    printf("  7. Large write test (4 KB round-trip)\n");
    printf("\n] ");
}

/* ============================================================
 * Option 1 — printf smoke test.
 * ============================================================ */
static void test_printf(void) {
    printf("\n[SUCCESS] printf works in ring 3!\n] ");
}

/* ============================================================
 * Option 3 — malloc/free over sbrk.
 * ============================================================ */
static void test_malloc(void) {
    printf("\n[HEAP TEST] Testing newlib malloc/free via sbrk\n");

    size_t N = 4096;
    unsigned char* buf = (unsigned char*)malloc(N);
    if (!buf) {
        printf("  malloc(%lu) failed (returned NULL)\n] ",
               (unsigned long)N);
        return;
    }
    printf("  malloc(%lu) returned %p\n", (unsigned long)N, (void*)buf);

    for (size_t i = 0; i < N; i++) {
        buf[i] = (unsigned char)(i & 0xFF);
    }
    printf("  wrote %lu bytes at %p\n",
           (unsigned long)N, (void*)buf);

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

/* ============================================================
 * Option 4 — the original FS round-trip, with the length fixed.
 * ============================================================ */
static void test_fs_roundtrip(void) {
    printf("\n[FS TEST] step 1: open for write/create/truncate\n");

    int fd = open("0:/USER.TXT",
                  O_WRONLY | O_CREAT | O_TRUNC,
                  0644);
    printf("  open() returned %d\n", fd);
    if (fd < 0) {
        printf("[FS TEST] FAILED at open\n] ");
        return;
    }

    printf("\n[FS TEST] step 2: write a known string\n");
    const char* msg = "Hello from ring 3!\r\n";
    int msg_len = (int)strlen(msg);
    int n = write(fd, msg, msg_len);
    printf("  write() returned %d (expected %d)\n", n, msg_len);
    if (n != msg_len) {
        printf("[FS TEST] FAILED at write (short or error)\n] ");
        close(fd);
        return;
    }

    printf("\n[FS TEST] step 3: close\n");
    int cr = close(fd);
    printf("  close() returned %d\n", cr);
    if (cr != 0) {
        printf("[FS TEST] FAILED at close\n] ");
        return;
    }

    printf("\n[FS TEST] step 4: reopen for read\n");
    int rfd = open("0:/USER.TXT", O_RDONLY, 0);
    printf("  open(O_RDONLY) returned %d\n", rfd);
    if (rfd < 0) {
        printf("[FS TEST] FAILED at reopen\n] ");
        return;
    }

    printf("\n[FS TEST] step 5: read it back\n");
    char rbuf[64];
    memset(rbuf, 0, sizeof(rbuf));
    int rn = read(rfd, rbuf, sizeof(rbuf) - 1);
    printf("  read() returned %d\n", rn);
    printf("  content: \"%s\"\n", rbuf);

    if (rn == msg_len && memcmp(rbuf, msg, msg_len) == 0) {
        printf("[FS TEST] PASSED (round-trip byte-exact)\n] ");
    } else if (rn == msg_len) {
        printf("[FS TEST] PARTIAL (same length, different bytes)\n] ");
    } else {
        printf("[FS TEST] FAILED (length mismatch)\n] ");
    }
    close(rfd);
}

/* ============================================================
 * Option 5 — persistence check.
 *
 * The expected content is the same string option 4 writes. If the
 * file exists and matches, persistence is proven. If it doesn't
 * exist, we say so and tell the user to run option 4 and reboot.
 * ============================================================ */
static void test_persistence(void) {
    printf("\n[PERSIST TEST] Checking for USER.TXT from a previous boot\n");

    int fd = open("0:/USER.TXT", O_RDONLY, 0);
    if (fd < 0) {
        printf("  USER.TXT not found (fd=%d)\n", fd);
        printf("  Run option 4, then reboot and run option 5.\n");
        printf("[PERSIST TEST] INCONCLUSIVE (no prior write)\n] ");
        return;
    }

    char rbuf[64];
    memset(rbuf, 0, sizeof(rbuf));
    int rn = read(fd, rbuf, sizeof(rbuf) - 1);
    close(fd);

    const char* expected = "Hello from ring 3!\r\n";
    int expected_len = (int)strlen(expected);

    printf("  read %d bytes: \"%s\"\n", rn, rbuf);

    if (rn == expected_len && memcmp(rbuf, expected, expected_len) == 0) {
        printf("[PERSIST TEST] PASSED (file survived reboot, byte-exact)\n] ");
    } else {
        printf("[PERSIST TEST] FAILED (length or content mismatch)\n] ");
    }
}

/* ============================================================
 * Option 6 — multi-file test.
 *
 * Creates three short files, lists them via readdir-equivalent
 * (open each and read), deletes them, then confirms deletion by
 * attempting to open them again and expecting failure.
 * ============================================================ */
static void test_multi_file(void) {
    printf("\n[MULTI TEST] Create 3 files, verify, delete, verify deletion\n");

    const char* names[3] = {
        "0:/A.TXT",
        "0:/B.TXT",
        "0:/C.TXT",
    };
    const char* contents[3] = {
        "alpha",
        "bravo",
        "charlie",
    };

    /* Phase 1: create and write. */
    int created = 0;
    for (int i = 0; i < 3; i++) {
        int fd = open(names[i], O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (fd < 0) {
            printf("  create %s: FAILED (fd=%d)\n", names[i], fd);
            continue;
        }
        int len = (int)strlen(contents[i]);
        int n = write(fd, contents[i], len);
        close(fd);
        if (n == len) {
            printf("  create %s: OK (%d bytes)\n", names[i], n);
            created++;
        } else {
            printf("  create %s: short write (%d/%d)\n", names[i], n, len);
        }
    }
    printf("  created %d/3 files\n", created);

    /* Phase 2: reopen and verify contents. */
    int verified = 0;
    for (int i = 0; i < 3; i++) {
        int fd = open(names[i], O_RDONLY, 0);
        if (fd < 0) {
            printf("  reopen %s: FAILED\n", names[i]);
            continue;
        }
        char buf[32];
        memset(buf, 0, sizeof(buf));
        int rn = read(fd, buf, sizeof(buf) - 1);
        close(fd);
        int expected = (int)strlen(contents[i]);
        if (rn == expected && memcmp(buf, contents[i], expected) == 0) {
            verified++;
        } else {
            printf("  verify %s: MISMATCH (got %d bytes)\n", names[i], rn);
        }
    }
    printf("  verified %d/3 files\n", verified);

    /* Phase 3: delete. unlink is not wired through the syscall table,
     * so we can't test deletion from ring 3 yet. Report that. */
    printf("  (deletion not available: no SYS_UNLINK syscall yet)\n");

    if (created == 3 && verified == 3) {
        printf("[MULTI TEST] PASSED (create + write + read, 3 files)\n] ");
    } else {
        printf("[MULTI TEST] FAILED\n] ");
    }
}

/* ============================================================
 * Option 7 — large write test.
 *
 * Writes 4096 bytes (a full page, crossing one or more clusters),
 * reads them back, and verifies byte-exact. Catches multi-cluster
 * bugs that a 19-byte write won't hit.
 * ============================================================ */
static void test_large_write(void) {
    printf("\n[BIG TEST] 4096-byte round-trip through FatFs\n");

    size_t N = 4096;
    unsigned char* wbuf = (unsigned char*)malloc(N);
    if (!wbuf) {
        printf("  malloc(%lu) failed\n] ", (unsigned long)N);
        return;
    }
    for (size_t i = 0; i < N; i++) {
        wbuf[i] = (unsigned char)((i * 7 + 3) & 0xFF);
    }

    int fd = open("0:/BIG.BIN", O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) {
        printf("  open: FAILED (fd=%d)\n] ", fd);
        free(wbuf);
        return;
    }
    int n = write(fd, wbuf, (int)N);
    close(fd);
    printf("  wrote %d / %lu bytes\n", n, (unsigned long)N);
    if (n != (int)N) {
        printf("[BIG TEST] FAILED (short write)\n] ");
        free(wbuf);
        return;
    }

    unsigned char* rbuf = (unsigned char*)malloc(N);
    if (!rbuf) {
        printf("  second malloc failed\n] ");
        free(wbuf);
        return;
    }

    fd = open("0:/BIG.BIN", O_RDONLY, 0);
    if (fd < 0) {
        printf("  reopen: FAILED (fd=%d)\n] ", fd);
        free(wbuf); free(rbuf);
        return;
    }
    int rn = read(fd, rbuf, (int)N);
    close(fd);

    printf("  read %d / %lu bytes\n", rn, (unsigned long)N);

    if (rn == (int)N && memcmp(wbuf, rbuf, N) == 0) {
        printf("[BIG TEST] PASSED (byte-exact 4 KB round-trip)\n] ");
    } else {
        printf("[BIG TEST] FAILED (length or content mismatch)\n] ");
    }

    free(wbuf);
    free(rbuf);
}

/* ============================================================
 * Main loop.
 * ============================================================ */
int main(int argc, char** argv) {
    (void)argc;
    (void)argv;

    /* Unbuffered stdout so every printf immediately hits sys_write.
     * This means test output appears in order even if a later test
     * crashes the shell. */
    setvbuf(stdout, NULL, _IONBF, 0);

    print_menu();

    char input_char = 0;
    while (1) {
        if (read(0, &input_char, 1) > 0) {
            switch (input_char) {
                case '1': test_printf();       break;
                case '2':
                    printf("\nExiting user shell environment...\n");
                    return 0;
                case '3': test_malloc();       break;
                case '4': test_fs_roundtrip(); break;
                case '5': test_persistence();  break;
                case '6': test_multi_file();   break;
                case '7': test_large_write();  break;
                default:
                    /* Ignore unrecognized keys silently; the next
                     * menu reprint after a test shows the prompt. */
                    break;
            }
            print_menu();
        }
    }

    return 0;
}
