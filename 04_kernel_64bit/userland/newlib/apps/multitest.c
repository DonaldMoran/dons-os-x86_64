/*
 * multitest.c — external port of the shell's old option 6.
 *
 * Creates three files, writes distinct content to each, reopens and
 * verifies, unlinks all three, then confirms they are gone.
 *
 * Exits 0 on full pass, 1 on any failure.
 */

#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>

static const char *NAMES[3] = {
    "0:/A.TXT",
    "0:/B.TXT",
    "0:/C.TXT",
};
static const char *CONTENTS[3] = {
    "alpha",
    "bravo",
    "charlie",
};

int main(void) {
    setvbuf(stdout, NULL, _IONBF, 0);

    printf("[multitest] create 3 files, verify, delete, verify deletion\n");

    int created = 0;
    for (int i = 0; i < 3; i++) {
        int fd = open(NAMES[i], O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (fd < 0) {
            printf("  create %s: FAILED (fd=%d)\n", NAMES[i], fd);
            continue;
        }
        int len = (int)strlen(CONTENTS[i]);
        int n = write(fd, CONTENTS[i], len);
        close(fd);
        if (n == len) {
            printf("  create %s: OK (%d bytes)\n", NAMES[i], n);
            created++;
        } else {
            printf("  create %s: short write (%d/%d)\n", NAMES[i], n, len);
        }
    }
    printf("  created %d/3 files\n", created);

    int verified = 0;
    for (int i = 0; i < 3; i++) {
        int fd = open(NAMES[i], O_RDONLY, 0);
        if (fd < 0) {
            printf("  reopen %s: FAILED\n", NAMES[i]);
            continue;
        }
        char buf[32];
        memset(buf, 0, sizeof(buf));
        int rn = read(fd, buf, sizeof(buf) - 1);
        close(fd);
        int expected = (int)strlen(CONTENTS[i]);
        if (rn == expected && memcmp(buf, CONTENTS[i], expected) == 0) {
            verified++;
        } else {
            printf("  verify %s: MISMATCH (got %d bytes)\n", NAMES[i], rn);
        }
    }
    printf("  verified %d/3 files\n", verified);

    printf("  deleting %d file(s)...\n", created);
    int deleted = 0;
    for (int i = 0; i < 3; i++) {
        if (unlink(NAMES[i]) == 0) {
            deleted++;
        } else {
            printf("  unlink %s: FAILED (errno=%d)\n", NAMES[i], errno);
        }
    }
    printf("  deleted %d/3 files\n", deleted);

    int still_present = 0;
    for (int i = 0; i < 3; i++) {
        int fd = open(NAMES[i], O_RDONLY, 0);
        if (fd >= 0) {
            still_present++;
            close(fd);
        }
    }
    printf("  after deletion, %d of 3 files still present\n", still_present);

    if (created == 3 && verified == 3 && deleted == 3 && still_present == 0) {
        printf("[multitest] PASS (create + write + read + delete, 3 files)\n");
        return 0;
    }
    if (created == 3 && verified == 3) {
        printf("[multitest] FAIL: delete step (create/write/read OK)\n");
        return 1;
    }
    printf("[multitest] FAIL\n");
    return 1;
}
