# SYS_EXEC — Design Plan

**Status:** planned, not yet implemented
**Target version:** v0.5.3
**Depends on:** v0.5.2 (heap, LFN, SYS_UNLINK, cross-GCC for FatFs)

---

## 1. What this feature is

`SYS_EXEC` lets a user program run another user program from the
filesystem. Today, every user program is compiled into the kernel
image as a C array via `xxd -i`. `SYS_EXEC` reads an ELF64 executable
from the FAT volume at runtime, creates a new process, loads the ELF
into it, and queues it.

This is the last piece needed before a real shell with external
commands is possible. After `SYS_EXEC`, `HELLO.ELF` can live on the
FAT partition and be run by name.

---

## 2. Design choice: spawn, not execve

The syscall creates a **new** process and returns its pid to the
caller. The caller keeps running. This is spawn semantics, not
POSIX execve.

**Why not execve:**

- The kernel has no `fork`. An execve-style syscall would replace the
  calling process's image, so the shell would be gone after the first
  program it ran.
- The kernel has no `waitpid`. There is no parent-child relationship
  in the PCB today.
- The kernel has no per-process file descriptor inheritance.
- execve composes with fork and waitpid; without those, it has
  nothing to compose with.

**Why spawn:**

- It matches what the kernel already does at boot. The kernel shell's
  `usershell` command, the user shell's `elfload` path in the kernel
  shell, and `runproc` all do `process_create` + `elf_load_into_process`
  + `scheduler_ready_queue_add`. `SYS_EXEC` is the same operation,
  exposed as a syscall.
- It doesn't require the parent to give up anything.
- It's the primitive a shell actually needs.

---

## 3. Design choice: block the caller until the child exits

Spawn alone is enough to prove the loader path works, but a shell that
spawns a program and then halts the CPU when the program exits is not
useful. The parent needs to know when the child is done.

Two options for the first version:

**Option A — spawn-and-halt.** The child runs, the shell resumes
concurrently, and when the child exits it halts the CPU. Proves the
loader works but leaves the machine dead.

**Option B — spawn-and-wait.** After spawn, the parent calls
`waitpid(pid)`, which blocks until the child exits and then returns the
child's exit status. The parent resumes normally.

**Decision: Option B.** The additional work is small (see §7) and it
produces a shell that can actually run external programs and return to
the prompt.

---

## 4. Design choice: load the ELF into a kmalloc'd buffer

The current `elf_load_into_process(pcb_t* pcb, const void* elf_data)`
expects the whole ELF in memory. Two options:

**Option A — kmalloc the whole file.** `f_open`, `f_size`, `kmalloc`,
`f_read` the whole file into the buffer, call `elf_load_into_process`,
`kfree`. Simple, reuses everything, works for files up to the size of
the heap.

**Option B — stream the file through a new loader API.** Add
`elf_stream_begin` / `elf_stream_write` / `elf_stream_finish` to
`elf.c`. Read the file in 4 KB chunks and feed them to the loader.
More correct for large files, no large contiguous allocation needed.

**Decision: Option A for v0.5.3.** `HELLO.ELF` is 10–20 KB. The kernel
heap is 4 MB after the stress test grew it. `kmalloc(20 KB)` is fine.
Option B is a follow-up if a program larger than ~1 MB ever needs to be
loaded; the streaming API is on the roadmap but not blocking.

---

## 5. Syscall interface

### Kernel side

```c
#define SYS_EXEC 8

long sys_exec(const char* user_path);
```

`user_path` is a user-space pointer to a NUL-terminated path string
like `"0:/HELLO.ELF"`. The kernel copies it into a kernel buffer with
`safe_copy_from_user`.

**Return values:**

- `> 0` — the new process's pid.
- `-1` — failure. The serial log carries a diagnostic line naming the
  reason (file not found, not an ELF, bad header, out of memory, no
  PCB slot, no kernel stack slot).

### Userland side

```c
int spawn(const char* path);
```

Declared in a project header (`apps/include/donsdos.h`), implemented
in `arc2/syscalls.c` as a thin wrapper around `syscall3(SYS_EXEC, ...)`.

### Wait

```c
#define SYS_WAITPID 9

long sys_waitpid(long pid, int* user_status, int options);
```

`pid > 0` — wait for that specific child.
`pid == -1` — wait for any child.
`options & WNOHANG` — return 0 immediately if no child has exited.

Returns the reaped pid, or `-1` on error, or `0` if `WNOHANG` and no
child has exited.

Userland:

```c
int waitpid(int pid, int* status, int options);
```

Declared in the same project header.

---

## 6. Kernel-side flow of `sys_exec`

```
sys_exec(user_path):
  1. copy_user_string(path, sizeof(path), user_path)
     -> return -1 on fault

  2. f_open(&file, path, FA_READ | FA_OPEN_EXISTING)
     -> return -1 on failure

  3. Read the ELF header (64 bytes) into a local Elf64_Ehdr.
     Validate:
       - magic \x7fELF
       - e_ident[EI_CLASS] == ELFCLASS64 (2)
       - e_ident[EI_DATA] == ELFDATA2LSB (1)
       - e_type == ET_EXEC (2)
       - e_machine == EM_X86_64 (62)
       - e_phentsize == sizeof(Elf64_Phdr)
       - e_phnum in [1, 16]
     -> close file, return -1 on failure

  4. Read the program headers (e_phnum * 64 bytes)
     into a stack array Elf64_Phdr phdrs[16].
     -> close file, return -1 on failure

  5. Read the file into a kmalloc'd buffer.
     size_t file_size = f_size(&file);
     uint8_t* buf = kmalloc(file_size);
     if (!buf) { f_close; return -1; }
     f_lseek(&file, 0);
     f_read(&file, buf, file_size, &br);   /* loop for short reads */
     f_close(&file);

  6. Derive a process name from the path's basename.
     e.g. "HELLO.ELF" from "0:/HELLO.ELF".
     Truncated to PROC_NAME_LEN - 1.

  7. pcb_t* child = process_create(name, USER_CODE_BASE, 0);
     if (!child) { kfree(buf); return -1; }

     process_create places the new PCB on the ready queue.  We remove
     it immediately, because the ELF is not yet loaded and a PIT tick
     between create and load would schedule a process whose entry page
     is not yet mapped.  This is the same fix that was applied to the
     kernel shell's elfload and usershell paths in v0.4.9.

     scheduler_ready_queue_remove(child);

  8. Switch CR3 to child->cr3, call elf_load_into_process(child, buf),
     switch back.
     -- OR --
     elf_load_into_process internally handles the CR3 switch.  Need to
     check the current implementation; if it writes through HHDM using
     pcb->cr3 directly, no switch is needed.  The kernel shell's
     elfload path does switch, so we follow that pattern.

  9. if (entry == 0) { process_destroy(child); kfree(buf); return -1; }
     child->entry_point = entry;
     child->rip = entry;

     Also patch the resume frame's RIP slot at [child->rsp + 15*8],
     because process_create built the frame with the placeholder entry.
     The frame layout matches context_switch.asm's, which has RIP at
     offset 0x78 from rsp (the 16th qword from the top).

 10. child->parent_pid = current->pid;
     kfree(buf);

 11. keyboard_buffer_flush();
     scheduler_ready_queue_add(child);

 12. return (long)child->pid;
```

---

## 7. Kernel-side flow of `sys_waitpid`

```
sys_waitpid(pid, user_status, options):
  1. pcb_t* self = process_get_current();
     if (!self) return -1;

  2. Find a child:
       - if pid > 0, look for the PCB with pid == pid and
         parent_pid == self->pid
       - if pid <= 0 (including -1), find any child of self->pid

     Cases:
       a. Child exists and is PROC_STATE_ZOMBIE:
          - write child->exit_status to user_status (if non-NULL,
            through safe_copy_to_user)
          - reap: process_reclaim(child)
          - return child->pid  (cache pid before reclaim zeroes it)

       b. Child exists and is still running:
          - if (options & WNOHANG): return 0
          - else: self->state = PROC_STATE_BLOCKED;
                  self->block_kind = BLOCK_WAITPID;
                  self->wait_pid = pid;    /* -1 means "any child" */
                  process_yield();
                  /* when resumed, re-run the search from step 2 */

       c. No such child:
          - return -1
```

Blocking and re-running the search has one subtlety: after
`process_yield()`, the process resumes from a saved kernel frame, and
the C function continues from right after the `yield` call. So
`sys_waitpid` must be a loop:

```c
long sys_waitpid(long pid, int* user_status, int options) {
    for (;;) {
        pcb_t* zombie = find_zombie_child(current, pid);
        if (zombie) {
            long reaped = (long)zombie->pid;
            int status = zombie->exit_status;
            process_reclaim(zombie);
            if (user_status) safe_copy_to_user(user_status, &status, sizeof status);
            return reaped;
        }
        if (options & WNOHANG) return 0;
        pcb_t* child = find_live_child(current, pid);
        if (!child) return -1;
        current->state = PROC_STATE_BLOCKED;
        current->block_kind = BLOCK_WAITPID;
        current->wait_pid = pid;
        process_yield();
        /* loop back and re-check */
    }
}
```

---

## 8. Scheduler changes

### New PCB fields

```c
/* In pcb_t, AFTER block_kind, to avoid shifting the offsets that
 * context_switch.asm reads.  The _Static_assert block in process.c
 * pins every offset the asm depends on. */
uint64_t parent_pid;      /* 0 = no parent (idle, kernel shell) */
int      exit_status;     /* set by sys_exit, read by waitpid */
uint64_t wait_pid;        /* pid the process is blocked waiting for, or -1 */
```

### New block kind

```c
#define BLOCK_WAITPID 1
```

### `process_exit` changes

```c
void process_exit(void) {
    cli;

    pcb_t* exiting = current_process;

    /* Wake the parent if it is blocked in waitpid on us. */
    if (exiting->parent_pid != 0) {
        pcb_t* parent = process_find_by_pid(exiting->parent_pid);
        if (parent && parent->state == PROC_STATE_BLOCKED &&
            parent->block_kind == BLOCK_WAITPID) {
            if (parent->wait_pid == -1 ||
                parent->wait_pid == exiting->pid) {
                parent->state = PROC_STATE_READY;
                parent->block_kind = 0;
                scheduler_ready_queue_add(parent);
            }
        }
    }

    /* If the exiting process has a parent, become a zombie and wait
     * for the parent to reap us.  Do NOT reclaim now.
     *
     * If it has no parent, reclaim immediately. */
    if (exiting->parent_pid != 0) {
        exiting->state = PROC_STATE_ZOMBIE;
        scheduler_ready_queue_remove(exiting);
        /* fall through to the switch to next */
    } else {
        exiting->state = PROC_STATE_TERMINATED;
        scheduler_ready_queue_remove(exiting);
        process_reclaim(exiting);
    }

    pcb_t* next = scheduler_ready_queue_next();
    ...
}
```

### New state

```c
PROC_STATE_ZOMBIE  /* exited, waiting for parent to reap */
```

The existing `PROC_STATE_TERMINATED` stays for processes with no
parent (idle, kernel diagnostics) that are reclaimed immediately.

### Reaping

`sys_waitpid` calls `process_reclaim(zombie)` after reading
`exit_status`.  `process_reclaim` currently sets `state = UNUSED` and
`pid = 0` and decrements `process_count`.  That is correct for a
zombie.

### What happens if a process has a parent that never waits

The zombie sits in the PCB pool forever.  This is a real leak, but it
is bounded by the 32-slot PCB pool and it matches POSIX semantics: a
parent that does not reap its children accumulates zombies.  A future
`init` (or idle) could reap orphans whose parents have exited.  Not
in scope for v0.5.3.

---

## 9. Userland shim

`arc2/syscalls.c`:

```c
#define SYS_EXEC     8
#define SYS_WAITPID  9

int spawn(const char* path) {
    return (int)syscall3(SYS_EXEC, (uint64_t)path, 0, 0);
}

int waitpid(int pid, int* status, int options) {
    /* sys_waitpid is a 3-argument syscall. */
    int64_t ret;
    __asm__ volatile(
        "syscall"
        : "=a"(ret)
        : "a"((uint64_t)SYS_WAITPID),
          "D"((uint64_t)pid),
          "S"((uint64_t)status),
          "d"((uint64_t)options)
        : "rcx", "r11", "memory"
    );
    return (int)ret;
}
```

`apps/include/donsdos.h`:

```c
#ifndef DONSDOS_H
#define DONSDOS_H

#ifdef __cplusplus
extern "C" {
#endif

int spawn(const char* path);

/* POSIX-ish waitpid.  pid > 0 waits for a specific child;
 * pid == -1 waits for any child.  options & 1 (WNOHANG)
 * returns 0 if no child has exited. */
int waitpid(int pid, int* status, int options);

#define WNOHANG 1

extern void sys_reboot(void);

#ifdef __cplusplus
}
#endif

#endif /* DONSDOS_H */
```

Note: `waitpid` may already be declared by newlib's `sys/wait.h`.  If
a conflicting declaration shows up at build time, we either match
newlib's signature (which has the same 3-argument form on Linux and
BSD) or we name our function something else.  The 3-argument form
matches, so a conflict is unlikely, but it's worth a `grep waitpid
include/` before committing.

---

## 10. Build pipeline changes

### New user program: `apps/hello.c`

A minimal program:

```c
#include <stdio.h>

int main(void) {
    printf("Hello from a disk-loaded ELF!\n");
    return 0;
}
```

### `userland/newlib/Makefile` — add a `hello` target

The target must:

1. Compile `apps/hello.c`.
2. Link it with the same `crt0`, `syscalls`, `reent`, and `-lc` as
   `user_shell.elf`.
3. **Not** `xxd -i` the result.  The output goes to
   `apps/hello.elf` (a stable path the boot Makefile can `mcopy`).
4. Strip debug metadata to keep the file small.

Proposed additions to the Makefile:

```make
# ---- hello (standalone disk-loaded ELF) ----
build/apps/hello.o: apps/hello.c
	$(CC) $(CFLAGS) -c apps/hello.c -o build/apps/hello.o

apps/hello.elf: build/apps/hello.o build/arc2/crt0.o build/arc2/syscalls.o build/arc2/reent.o
	$(LD) $(LDFLAGS) -o apps/hello.elf build/arc2/crt0.o build/arc2/syscalls.o build/arc2/reent.o build/apps/hello.o -lc
	$(STRIP) --strip-debug apps/hello.elf -o apps/hello.elf.tmp
	mv apps/hello.elf.tmp apps/hello.elf
	@echo "hello.elf: $$(stat -c %s apps/hello.elf) bytes (disk-loaded)"

.PHONY: hello
hello: apps/hello.elf

all: prep build/user_shell.elf ../../user_shell_data.c apps/hello.elf

clean:
	rm -rf build ../../user_shell_data.c apps/hello.elf
```

### `05_boot_kernel64/Makefile` — mcopy `HELLO.ELF` onto the FAT partition

The `hdd-single.img` rule already loops over `../test-files/*` and
`mcopy`s each one.  Add a step after that loop that:

1. Runs `$(MAKE) -C ../04_kernel_64bit/userland/newlib hello` to build
   `apps/hello.elf`.
2. `mcopy`s it onto the FAT partition as `HELLO.ELF`.

Proposed additions:

```make
HELLO_ELF := ../04_kernel_64bit/userland/newlib/apps/hello.elf

.PHONY: hello-elf
hello-elf:
	$(MAKE) -C ../04_kernel_64bit/userland/newlib hello

hdd-single.img: boot.bin stage2.bin ../04_kernel_64bit/kernel.bin hello-elf
	... (existing recipe) ...

	@if [ -f "$(HELLO_ELF)" ]; then \
	    echo "mcopy $(HELLO_ELF) -> ::/HELLO.ELF"; \
	    mcopy -i hdd-single.img@@$$(( $(SINGLE_DRIVE_LBA) * 512 )) \
	        "$(HELLO_ELF)" ::/HELLO.ELF ; \
	else \
	    echo "WARN: $(HELLO_ELF) not found; HELLO.ELF missing from image"; \
	fi
	@echo "--- FAT partition contents ---"
	@mdir -i hdd-single.img@@$$(( $(SINGLE_DRIVE_LBA) * 512 )) ::/ || true
```

The `mdir` at the end shows what actually landed on the partition, so
a missing file is visible immediately.

The dual-drive path (`hdd.img` and `fat.img`) has the same structure
but a different `mcopy` target — `fat.img` is a committed binary in
the tree.  For v0.5.3, only the single-drive path needs to support
`HELLO.ELF`.  The dual-drive path can be updated in a follow-up if
the user ever needs it.

---

## 11. Test path

User shell menu option `A`:

```c
static void test_spawn(void) {
    printf("\n[SPAWN TEST] spawn(\"0:/HELLO.ELF\")\n");
    int pid = spawn("0:/HELLO.ELF");
    if (pid < 0) {
        printf("  spawn failed (returned %d)\n", pid);
        return;
    }
    printf("  spawned pid=%d; waiting...\n", pid);
    int status = 0;
    int reaped = waitpid(pid, &status, 0);
    printf("  child %d exited with status %d (waitpid returned %d)\n",
           pid, status, reaped);
}
```

Expected output:

```
[SPAWN TEST] spawn("0:/HELLO.ELF")
  spawned pid=3; waiting...
Hello from a disk-loaded ELF!
  child 3 exited with status 0 (waitpid returned 3)
```

The interleaving of the child's output between "waiting..." and
"child 3 exited" is expected.  The parent blocked in waitpid, the
scheduler ran the child, the child printed, the child exited, the
parent was woken.

---

## 12. What is not in scope for v0.5.3

- **Loading programs larger than the heap.**  kmalloc-based loading is
  fine up to a few MB.  Streaming loader is a follow-up.
- **argv / envp.**  The child gets `argc = 0`, `argv = NULL`, because
  `crt0.S` calls `main(0, NULL)`.  Passing arguments requires building
  an argument vector on the child's user stack at load time.
- **Per-process working directory.**  Paths are absolute
  (`"0:/HELLO.ELF"`).
- **`SYS_UNLINK` on the child while it is running.**  FatFs refuses to
  delete an open file; the parent should close the file before
  spawning, which sys_exec does (the f_close happens before
  process_create).
- **Reaping orphans.**  A child whose parent has already exited
  becomes a zombie that nobody reaps.  Bounded by the PCB pool.
- **Signals, process groups, session leaders.**  Not in this kernel.
- **Execve-style replace.**  Not applicable without fork.

---

## 13. Risk register

| Risk | Mitigation |
|------|------------|
| `elf_load_into_process` assumes CR3 switch is handled by caller.  Must switch to `child->cr3` before calling, switch back after.  Same pattern as `kmain`'s `elfload` and `usershell` paths. | Copy the existing pattern exactly. |
| PCB field additions shift `context_switch.asm` offsets. | Add new fields **after** `block_kind`.  The `_Static_assert` block in `process.c` will catch any accidental shift at build time. |
| Zombie state confuses the scheduler. | `PROC_STATE_ZOMBIE` is distinct from `PROC_STATE_TERMINATED` and `PROC_STATE_BLOCKED`.  The scheduler skips zombies (they are not on the ready queue).  `process_reclaim` sets `state = UNUSED` when reaped. |
| Waitpid blocks forever if the child never exits. | There is no timeout.  A runaway child keeps its parent blocked.  This matches POSIX waitpid without alarm handlers.  A follow-up could add a scheduler-level watchdog. |
| Newlib's `sys/wait.h` declares `waitpid` with a different signature. | Match newlib's signature.  If it differs, name our function `donsdos_waitpid` and keep a POSIX-named wrapper. |
| The child writes to stdout, and the parent is blocked.  Output ordering between them could interleave. | Expected.  The console has no per-process isolation yet.  See MAINTENANCE.md §4e. |
| Kernel stack overflow in `sys_waitpid` if it loops many times. | Each loop iteration pushes one frame, but the loop is not tail-recursive and `process_yield` returns.  Stack usage is bounded by one `sys_waitpid` frame at a time.  Not a risk. |

---

## 14. Version numbering

This is **v0.5.3**.  After implementation and verification, tag it and
merge `dev` → `main` following the same procedure used for v0.5.2.

---

## 15. Implementation order

1. **`pcb_t` fields** (parent_pid, exit_status, wait_pid, block_kind
   already exists, add new state).  Verify with `_Static_assert`.
2. **`process_exit` changes** (wake parent, zombie state).  This is
   the most delicate change and needs the most testing.
3. **`sys_waitpid`** in `user_syscall.c`.  Dispatcher case 9.
4. **`sys_exec`** in `user_syscall.c`.  Dispatcher case 8.
5. **`arc2/syscalls.c`** — `spawn()` and `waitpid()` shims.
6. **`apps/include/donsdos.h`** — declarations.
7. **`apps/hello.c`** — the test program.
8. **`userland/newlib/Makefile`** — the `hello` target.
9. **`05_boot_kernel64/Makefile`** — `mcopy` the ELF onto the image.
10. **`user_shell.c`** — menu option `A`.
11. **End-to-end test** — boot, press `k` (to get a shell that can
    return), `fatls` (confirm HELLO.ELF is there), `usershell`
    (launch the user shell), press `A`.
12. **Commit, tag v0.5.3, merge to main.**

Steps 1–3 are the risky ones.  Steps 4–10 are mostly plumbing.

---

## 16. What to watch for during testing

- **Double-free or leak of the ELF buffer.** `kmalloc` on success
  before `process_create`, `kfree` on all failure paths and after a
  successful load.
- **Zombie left in the PCB pool.** `process_dump_all` after a spawn
  and wait should show only idle and the shell.
- **Child PID vs parent PID confusion.** `proclist` after a spawn
  should show the parent still running and the child either running
  or gone.
- **Frame corruption.**  The `_Static_assert` block in `process.c`
  checks the `pcb_t` layout offsets that `context_switch.asm` reads.
  If the build passes, the frame layout is right.
- **Kernel stack usage.**  `sys_waitpid` blocking inside a syscall
  means the syscall returns through the saved kernel frame after the
  timer resumes the process.  The frame was built by
  `user_syscall_entry.asm`; it must survive the yield.
- **The `sys_read` blocking path is the closest existing example.**
  It sets `BLOCKED`, yields, and is woken by `irq1`.  `sys_waitpid`
  does the same thing, woken by `process_exit` instead.
