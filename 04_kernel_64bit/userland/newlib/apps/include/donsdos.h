#ifndef DONSDOS_H
#define DONSDOS_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * spawn(path, argc, argv) — create a new process from an ELF on the
 * FAT volume.
 *
 * The caller keeps running.  Returns the new process's pid (> 0) on
 * success, or -1 on failure.  Use waitpid() to wait for the child.
 *
 * `path` uses the same convention as open(): "0:/NAME.EXT".
 *
 * `argc`/`argv` are the arguments to hand the child's main().
 * Pass argc=0, argv=NULL for no arguments.  argv strings are copied
 * by the kernel into the child's address space; the caller's strings
 * and array need not outlive the call.
 *
 * Cap: argc <= 16, each string <= 256 bytes including NUL.
 *
 * This is NOT execve().  execve would replace the calling process's
 * image; spawn leaves the caller alone.
 */
int spawn(const char *path, int argc, char **argv);

/*
 * waitpid(pid, status, options) — wait for a child to exit.
 *
 * pid > 0   — wait for that specific child.
 * pid == -1 — wait for any child.
 * options & WNOHANG — return 0 immediately if no child has exited.
 *
 * Returns the reaped child's pid, 0 if WNOHANG and no child has
 * exited, or -1 on error.  If status is non-NULL, the child's exit
 * status is written to it.
 */
int waitpid(int pid, int *status, int options);

#ifndef WNOHANG
#define WNOHANG 1
#endif

/* Reboot the machine.  Does not return. */
extern void sys_reboot(void);

/*
 * Directory iteration (v0.6.x, Stage 3).
 *
 * Mirrors struct dons_dirent_t in 04_kernel_64bit/user_syscall.c.
 * Keep both in sync.  name[256] matches FF_LFN_BUF+1 with LFN
 * enabled and FF_MAX_LFN=255.
 */
struct dons_dirent {
    char     name[256];
    uint64_t size;
    uint8_t  attrib;
    uint8_t  _pad[7];
};

#define AM_RDO  0x01
#define AM_HID  0x02
#define AM_SYS  0x04
#define AM_DIR  0x10
#define AM_ARC  0x20

int opendir(const char *path);
int readdir(int dirfd, struct dons_dirent *out);
int closedir(int dirfd);

#ifdef __cplusplus
}
#endif

#endif /* DONSDOS_H */
