/*
 * ls.c — list the contents of 0:/ (the FAT volume root).
 *
 * First real userland consumer of SYS_OPENDIR / SYS_READDIR /
 * SYS_CLOSEDIR (Stage 3).  Matches the kernel shell's fatls output
 * shape: "<DIR>  name" for directories, "FILE  name  (size bytes)"
 * for files.
 *
 * No arguments yet — argv is Stage 4.  Hardcoded to "0:/".
 *
 * Exits 0 on success, 1 on opendir failure, 2 on readdir error.
 */

#include <stdio.h>
#include <string.h>
#include "donsdos.h"

int main(void) {
    setvbuf(stdout, NULL, _IONBF, 0);

    int dfd = opendir("0:/");
    if (dfd < 0) {
        printf("[ls] FAIL: opendir(\"0:/\") = %d\n", dfd);
        return 1;
    }

    int files = 0;
    int dirs  = 0;

    for (;;) {
        struct dons_dirent ent;
        memset(&ent, 0, sizeof(ent));
        int r = readdir(dfd, &ent);
        if (r == 0) break;
        if (r < 0) {
            printf("[ls] FAIL: readdir error\n");
            closedir(dfd);
            return 2;
        }
        if (ent.attrib & AM_DIR) {
            printf("<DIR>  %s\n", ent.name);
            dirs++;
        } else {
            printf("FILE   %s  (%lu bytes)\n",
                   ent.name, (unsigned long)ent.size);
            files++;
        }
    }

    closedir(dfd);

    printf("\n%lu file(s), %lu directory(ies)\n",
           (unsigned long)files, (unsigned long)dirs);
    return 0;
}
