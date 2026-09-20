#ifndef DONSDOS_H
#define DONSDOS_H

/*
 * dons-os userland extension API.
 *
 * These are not POSIX.  They are the primitives this kernel actually
 * provides, declared under names that match their semantics so a
 * reader is not misled into thinking they behave like the POSIX
 * function of the same (or similar) name.
 */

#ifdef __cplusplus
extern "C" {
#endif

/*
 * spawn(path) — create a new process from an ELF on the FAT volume.
 *
 * The caller keeps running.  Returns the new process's pid (> 0) on
 * success, or -1 on failure.  Use waitpid() to wait for the child.
 *
 * `path` uses the same convention as open(): "0:/NAME.EXT".
 *
 * This is NOT execve().  execve would replace the calling process's
 * image; spawn leaves the caller alone.
 */
int spawn(const char *path);

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
 *
 * This follows the POSIX three-argument form.  newlib's <sys/wait.h>
 * may already declare waitpid with the same signature; if so, the
 * declaration here is redundant but harmless.
 */
int waitpid(int pid, int *status, int options);

#ifndef WNOHANG
#define WNOHANG 1
#endif

/* Reboot the machine.  Does not return. */
extern void sys_reboot(void);

#ifdef __cplusplus
}
#endif

#endif /* DONSDOS_H */
