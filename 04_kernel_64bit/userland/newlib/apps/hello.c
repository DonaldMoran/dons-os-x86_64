/*
 * hello.c — the first disk-loaded user program.
 *
 * Built into a standalone ELF by userland/newlib/Makefile, copied into
 * the FAT partition at image-build time, and run by the user shell's
 * "A" menu option via SYS_EXEC (spawn).  This is the first program in
 * the tree that is loaded from disk at runtime rather than embedded in
 * the kernel image.
 *
 * Exits via crt0's `call exit` on return from main, which issues
 * SYS_EXIT.  The kernel marks this process as a zombie and wakes the
 * parent (the user shell), which is blocked in waitpid.
 */

#include <stdio.h>

int main(void) {
    printf("\n");
    printf("====================================\n");
    printf("  Hello from a disk-loaded ELF!\n");
    printf("  I was loaded by SYS_EXEC from\n");
    printf("  0:/HELLO.ELF at runtime.\n");
    printf("====================================\n");
    printf("\n");
    return 0;
}
