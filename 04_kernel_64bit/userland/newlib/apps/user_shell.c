#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>

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
    printf("  3. Test malloc/free via newlib\n");
    printf("  4. Test FatFs (create/write/read/close)\n\n");
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
            } else if (input_char == '4') {

/* ===== USERLAND FS TEST BEGIN ===== */
                /* Run each step and report the result, so we can see
                 * exactly where it breaks if it does. All strings use
                 * short 8.3 names since FF_USE_LFN == 0. */

                printf("\n[FS TEST] step 1: open for write/create/truncate\n");

                int fd = open("0:/USER.TXT",
                              O_WRONLY | O_CREAT | O_TRUNC,
                              0644);

                printf("  open() returned %d\n", fd);
                if (fd < 0) {
                    printf("[FS TEST] FAILED at open\n] ");
                    continue;
                }

                printf("\n[FS TEST] step 2: write a known string\n");

                const char* msg = "Hello from ring 3!\r\n";
                int msg_len = 19;   /* strlen(msg) */
                int n = write(fd, msg, msg_len);

                printf("  write() returned %d (expected %d)\n", n, msg_len);
                if (n != msg_len) {
                    printf("[FS TEST] FAILED at write (short or error)\n] ");
                    close(fd);
                    continue;
                }

                printf("\n[FS TEST] step 3: close\n");

                int cr = close(fd);
                printf("  close() returned %d\n", cr);
                if (cr != 0) {
                    printf("[FS TEST] FAILED at close\n] ");
                    continue;
                }

                printf("\n[FS TEST] step 4: reopen for read\n");

                int rfd = open("0:/USER.TXT", O_RDONLY, 0);
                printf("  open(O_RDONLY) returned %d\n", rfd);
                if (rfd < 0) {
                    printf("[FS TEST] FAILED at reopen\n] ");
                    continue;
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
/* ===== USERLAND FS TEST END ===== */

            }
        }
    }

    return 0;
}
