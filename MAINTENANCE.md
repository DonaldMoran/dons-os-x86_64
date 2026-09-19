# MAINTENANCE
### dons-os (x86_64) — Known Debt, Latent Bugs, and Maintenance Work

This document lists everything that is *known to be unfinished, fragile,
or debt-laden* in the current codebase, but which is not a feature. It is
the counterpart to `ROADMAP.md` (which lists features) and
`OSDev_Checklist.md` (which tracks capability). If a thing is wrong, or
fragile, or will bite later, it belongs here.

Items are ordered by priority: do the ones at the top first. Effort is a
rough estimate, not a commitment.

Completed items are struck through (~~like this~~) and marked with ✅.
They are kept in place for history; do not delete them.

---

## 1. Kernel size ceiling ~~(176 KB staging limit)~~ ✅

**Status:** ✅ **DONE (v0.5.0 + commit 09a6f79).** The staging ceiling was
raised from 176 KB to 448 KB by adding PASS 4–7 and extending the
long-mode copy. The text below is kept for history: it describes the
176 KB state and the reasoning for raising it.

~~**Effort:** 30 minutes (Option 3), 2–3 hours (Option 2), a few days (Option 6)~~
~~**Risk if ignored:** the kernel grows past the ceiling and the Makefile's
tripwire fails the build. That is the *good* outcome. The bad outcome is
that someone removes the tripwire and the kernel silently truncates at
boot, producing a machine that boots to garbage.~~

### What the limits were

~~Two ceilings apply to `kernel.bin`:~~

~~1. **Staging ceiling: 176 KB.** `stage2.asm` reads the kernel from disk in
   three passes of 128 sectors each, staging it into low memory at
   `0x80000`, `0x90000`, and `0x20000`, then copies it to `0x100000` in
   long mode. That staging layout cannot exceed 176 KB (128 KB from the
   first two passes, plus 48 KB from the third). The tripwire in
   `04_kernel_64bit/Makefile` (`KERNEL_STAGING_LIMIT := 180224`) fails
   the build loudly if `kernel.bin` exceeds it.~~

~~2. **Disk-layout ceiling: 983 KB.** The kernel starts at LBA 128. The
   FAT partition starts at LBA 2048. So the kernel region is
   `(2048 - 128) × 512 = 983040` bytes. This ceiling is enforced by
   `KERNEL_DISK_LIMIT := 983040` in the same Makefile. It will not bind
   until the staging ceiling has been raised well past 176 KB.~~

~~The staging ceiling is the binding one today.~~

### How it was raised

~~**Option 3 — use the unused low memory (30 min, ceiling → 448 KB).**
`0x30000–0x7FFFF` (320 KB) is plain RAM, unused by the boot chain. Add
PASS 4–7 to `stage2.asm` reading into segments `0x3000` through `0x7000`,
and add a `rep movsq` in long mode to copy the new bytes to `0x12C000`
onward. Bump `KERNEL_TOTAL_BYTES` in `stage2.asm` to `448 * 1024` and
`KERNEL_STAGING_LIMIT` in the Makefile to `458752`.~~

~~Bonus: this also moves PASS 3 off `0x20000`, which is a robustness win
on real hardware. `0x20000–0x2FFFF` is conventional-free on QEMU/SeaBIOS
but not guaranteed by spec on bare metal. `0x30000–0x7FFFF` is safer.~~

### Remaining options if 448 KB is not enough

**Option 2 — stream reads (2–3 hours, ceiling → RAM limit).**
Read 128 KB into a single staging window, `rep movsq` it to its
destination, advance the destination pointer, read the next 128 KB into
the same window, repeat. Reuses one staging window indefinitely. Requires
the Makefile to write `kernel.bin`'s size somewhere stage2 can read (a
field in the boot sector, or a small header the Makefile emits). Stage2
then loops until that many bytes have been copied.

**Option 6 — replace stage2 with Limine (a few days, ceiling → none).**
Limine handles long-mode entry, the memory map, and page tables, and
hands the kernel a standard structure. Removes the staging architecture
entirely, turns the kernel into an ELF file loaded by the bootloader,
and gets you boot modules (the path to "user programs from disk") and a
framebuffer for free. Throws away `boot.asm`, `stage2.asm`, and the
BootInfo parsing in `entry.asm`. See ROADMAP §Long-term.

---

## 2. Documentation gaps ✅

**Status:** ✅ **DONE.** Item 2a is now covered by this file and by the
README's pointer to it. Items 2b and 2c were applied in commit `9b62d46`.
Text below kept for history.

~~**Effort:** ~2 hours total~~

~~### 2a. Kernel size limits are not documented in the README
See item 1 above. A reader has no idea the tripwire exists until it
fires. Add a short "Kernel size limits" section to the README under
Known Limitations, or fold it into the Build System notes.~~

~~### 2b. The `run` script is documented in one line
The README mentions it in the repository structure and one sentence in
Building & Running. The menu design ("uncomment one line in `menu()`,
then run `./run`") deserves a short paragraph. Otherwise a reader sees a
shell script with a function that's never called and has no idea how
it's supposed to be used.~~

~~### 2c. The user shell table is out of date
Options 1–7 are listed. Option 8 (list known files) and option 9
(reboot) are not. If the reboot change is kept, update the table.~~

---

## 3. Latent bugs (things that will bite eventually)

### 3a. ~~Hardcoded boot stack in `process_exit`~~ ✅

**Status:** ✅ **DONE (commit e246db6).** The kernel shell is now a real
process with its own PCB and kernel stack from the pool. It is created
by `kmain` on the `k` branch, resumed by `process_exit`'s fallback via
`scheduler_switch_to`, and suspended by every command handler that
yields to a diagnostic. The hardcoded `0xFFFFFFFF8008FF00` is gone, as
is the `kernel_shell_stack_top()` helper. Text below kept for history.

~~**Effort:** 1–2 hours~~

~~`process_exit`'s fallback to `kmain_shell_loop` sets
`rsp = 0xFFFFFFFF8008FF00`. That address is in the low 1 MB region,
currently mapped because the bootloader identity-maps it. Nothing in the
kernel guarantees it stays mapped. If you ever change the identity map,
this becomes a crash.~~

~~**Fix:** allocate the kernel shell's stack from the kernel stack pool
(`kernel_stack_alloc()`), set the shell up on that stack, free it on
exit. The fallback path in `process_exit` then sets `rsp` to the pool
stack instead of a hardcoded address.~~

### 3b. ~~No IST for `#DF`~~ ✅

**Status:** ✅ **DONE (commit f4d7df3).** A dedicated 4 KB IST1 stack is
allocated in `.bss`, `TSS.IST1` is populated with its top, and the `#DF`
gate's IST field is set to 1 in `idt.c`. The CPU now switches to the IST
stack before pushing the `#DF` frame, so `isr8_handler` prints RIP, CS,
RSP, SS, and the error code even if the original kernel stack was the
thing that faulted. Text below kept for history.

~~**Effort:** ~1 hour~~

~~Double faults currently produce a triple fault, which resets the CPU
without printing anything. QEMU shows the reset; on real hardware you'd
get a silent reboot. A small IST stack and an IST entry on the `#DF` IDT
gate turns this into a printed diagnostic.~~

~~**Fix:** add an IST stack (4 KB is plenty) to the TSS, set the `#DF`
gate's IST field to the new slot in `idt.c`, and have `isr8_handler`
print RIP, CS, and the error code before halting. The setup is very
similar to the current TSS `rsp0` handling.~~

### 3c. Page tables leak on process exit

**Status:** latent; a few pages per process, accumulates over many
`runproc` / `testyield` cycles in one boot
**Effort:** 2–3 hours

`process_exit` reclaims the PCB slot, the ELF segment pages, and the
user stack pages. It does not free the process's page tables (`cr3`).

**Fix:** walk the PML4 → PDPT → PD → PT chain in `process_reclaim` and
free every page table that is not shared with the kernel or another
process. Careful: `stage2.asm` shares `pt_low` between `pd[0]` and
`pd_hhdm[0]`, and the kernel's own mappings must not be touched. The
safe approach is to walk the user-space portion of the tree (the PML4
entries that correspond to the user address space, roughly PML4[0])
and free only those tables.

Defer this until you have multiple user processes. It's not a problem
with one.

### 3d. `sys_brk`'s `heap_base` is a single constant

**Status:** code cleanliness, not functional
**Effort:** 15 minutes

Every process's heap starts at the same virtual address
(`0x8000200000`) and grows in its own address space (different `cr3`),
so there's no address conflict. But the shared constant is ugly. A
future refactor would move it into the PCB.

### 3e. `vmm_map_page_in_cr3` does not flush the TLB

**Status:** documented, callers handle it
**Effort:** none unless a new caller forgets

Callers must `invlpg` after mapping if the address may have a stale
translation. `sys_brk` does this. New callers should too. The fix would
be to make `vmm_map_page_in_cr3` optionally flush, but that has its own
tradeoffs. Leave as-is; it's documented.

---

## 4. Code hygiene

**Status:** ✅ **DONE for 4a–4d.** Items 4e, 4f, 4g are architectural
or documentation-only and deferred.

### 4a. ~~Dead declarations~~ ✅

**Status:** ✅ **DONE (commit 20260919F).** Removed the orphaned
`sys_reboot` and `sys_proclist` declarations from `include/syscall.h`.
`sys_reboot` is defined in `arc2/syscalls.c` (userland) and the
kernel-side handler is `kernel_do_reboot` (static in `user_syscall.c`);
nothing in the kernel called the declared prototype. `sys_proclist`
was not defined anywhere — the shell's `proclist` command calls
`process_dump_all` directly.

~~`include/syscall.h` declares `void sys_reboot(void);`. The kernel-side
handler is now `kernel_do_reboot` (static, in `user_syscall.c`). The
declaration in `syscall.h` is likely unused. Check and remove.~~

### 4b. ~~The double-build in `run`~~ ✅

**Status:** ✅ **DONE (commit 20260919F).** The single-drive menu line
now calls `make -C 05_boot_kernel64 run-single` directly instead of
`make runkernel64-single`, so the kernel is built once, not twice.

~~The single-drive menu line rebuilds the kernel twice: once explicitly
in the line, and once via `runkernel64-single`, which itself does
`make -C 04_kernel_64bit FAT_CONFIG=single`. Two-line fix: either
drop the explicit rebuild from the menu line, or drop it from the
target. The `run` script's echoed `run: <cmd>` makes this visible in
the log.~~

### 4c. ~~Stale comments~~ ✅

**Status:** ✅ **DONE (commit 20260919F).** Removed the two stale
`FIXED:` markers from `arc2/syscalls.c`. Deleted the commented-out
`file_exists` helper from `user_shell.c`. Tidied the header comment
block in `stage2.asm` (consolidated the low-memory layout and history
into one readable block, removed the mid-thought narrative about
`KERNEL_TOTAL_BYTES`).

~~- `arc2/syscalls.c` has several `FIXED:` comments from earlier sessions.
  They were fixed long ago; the labels are noise now. Remove them.
- `user_shell.c` has a `file_exists` helper that is currently unused.
  Delete it.
- `stage2.asm`'s comments about "was 64 sectors" are accurate but
  accumulate cruft every time the layout changes. Consider a single
  "history" section rather than inline archaeology.~~

### 4d. ~~Serial output is not atomic~~ ✅

**Status:** ✅ **DONE (commits 20260919G, 20260919H).** Two commits:

- `20260919G` wrapped multi-part boot messages in `serial_lock()` /
  `serial_unlock()` and gated the ATA read-path prints behind
  `#define ATA_DEBUG 0`.
- `20260919H` made the lock shared between the serial and VGA drivers,
  made `serial_print` / `serial_print_hex` / `serial_print_dec` /
  `serial_write` take the lock around their whole operation, and did
  the same for `vga_print` and friends. `serial_lock` now saves
  `RFLAGS` on the outermost acquisition and `serial_unlock` restores
  it, so the lock is safe inside interrupt context.

The result: `vga_print("DonsDOS v0.5.0\n> ")` is atomic against timer
preemption. The `DonsDOS v` truncation is fixed. `serial_print` is
atomic; `**RING** 3 syscalls active` no longer gets split by the
timer's boot trace.

~~`PRINT_BOTH(str)` does `vga_print(str); serial_print(str);` as two
separate calls. If a timer tick fires between them (or between two
adjacent `PRINT_BOTH` calls in a multi-part message), the timer's own
serial output interleaves. Observed as `Storage: single-drive, FAT@Å
Prompt: press 'k' ...` and `ATA: probe driveTIMER[2] ...` in the
boot log. Cosmetic, but a real exception dump interleaved with another
process's output is very confusing.~~

### 4e. Print lock is a pragmatic fix, not the console design

**Status:** works for boot messages and single-console operation; not
the right shape for a tty
**Effort:** 1–2 hours to replace with a ring buffer
**When:** before the console subsystem (tty, per-user output routing)
is built

The shared print lock (item 4d) is a `cli`/`sti` critical section with a
nesting counter and RFLAGS save/restore. It is simple, correct, and
composes properly with interrupt context. But it is a **global** lock,
held with interrupts disabled, and this has two real costs:

1. **Scheduling fairness.** While any kernel code is printing, the
   timer does not fire. A 40-char line at 115200 baud is ~3.5 ms with
   interrupts off; a round-robin quantum of 10 ms (100 Hz tick) is
   silently stretched by that much for whichever process was running
   when the print started. Fine during boot. Not fine if the kernel
   ever prints from a scheduled context during normal operation.

2. **SMP and multi-user scale.** `cli`/`sti` is meaningless across
   CPUs, and a single global lock serializes output from unrelated
   users. Once the kernel supports multiple users or cores, this
   needs to become a per-console lock, or a proper tty.

The right design is a **ring buffer with a console task**:

- `serial_print` / `vga_print` memcpy their bytes into a fixed-size
  FIFO under a very short lock (microseconds, not milliseconds).
- A drain routine — a low-priority kernel task, a serial TX-ready
  IRQ handler, or a periodic tick — pulls bytes out of the FIFO and
  writes them to the UART / VGA.
- Producers never wait on the hardware. Interrupts are off for the
  memcpy only.
- Ordering is preserved by the FIFO's single consumer.

Once per-user ttys exist, each tty gets its own ring and its own
drain path, and the output of user A's process cannot block or
interleave with user B's.

This is not urgent — nothing in the current code prints during
normal operation after boot, now that the ATA read-path prints are
gated off. But it is the design the console subsystem should be
built on, and it should be in place before per-user tty support is
added.

### 4f. Print functions must remain leaf functions

**Status:** invariant; not yet violated
**Effort:** n/a (documentation and code review discipline)

The shared print lock is safe because nothing inside the print path
takes another lock. If a future `vga_print` implementation decides to
allocate a line buffer with `kmalloc`, and `kmalloc` takes a heap lock,
and some other path takes the heap lock then calls `print`, you have a
lock-order inversion.

Keep the invariant:

> **Print functions (`vga_*` and `serial_*`) may not take any other
> lock.** If a print routine needs to allocate, it must do so before
> acquiring the print lock, or use a fixed-size stack buffer.

Violating this creates the possibility of a lock-order inversion with
the heap lock (or any other lock introduced later). Nothing in the
current code violates it; this entry exists to keep it that way.

### 4g. Diagnostic for print-lock hold duration

**Status:** future diagnostic, not needed yet
**Effort:** ~30 minutes when it's needed

If the print lock ever starts holding interrupts off long enough to
matter (see item 4e, cost 1), the way to know is a small counter:

- In `timer_preempt_handler`, on each tick, compute the drift between
  `g_ticks` and `expected_ticks`, and record the maximum.
- Expose it via a `printstat` shell command alongside `schstat`.

A boot log or `schstat` output that shows the max drift creeping above
1 tick is the signal that the print path is starting to matter and the
ring buffer (item 4e) is due.

This is not something to build now. It's the tool you'll reach for if
you ever wonder "is printing slow?"

---

## 5. Testing infrastructure

**Status:** ad-hoc; runs exist but are not automated
**Effort:** ~3 hours
**Payoff:** makes every future change cheaper to verify

### 5a. Kernel-shell self-test

A single command that runs all the existing test commands in sequence
(`pmmtest`, `vmmtest`, `heaptest`, `atatest`, `fatmount`, `fatls`,
`elfload`, `testyield`, `syscall`, etc.) and reports a pass/fail summary.
Currently each test must be typed by hand.

### 5b. Boot-time self-test mode

A build flag (`-DSELFTEST`) that makes the kernel run a fixed test
sequence at boot and print results to serial, then halt. Useful for
regression checks after a change.

### 5c. `make test` target

A Makefile target that boots QEMU headless, runs the self-test, captures
the serial output, and reports pass/fail based on the output. Automatable
with a serial-to-file and a grep. Would let you catch regressions without
watching the boot.

### When to do this

Nothing is blocking this. The remaining items on this list are either
deferred (3c, 3d) or architectural (4e), and none of them change the
kernel in ways that would break a self-test written today. It can be
built on the current code.

---

## 6. Priorities, one more time

| # | Item | Effort | Status |
|---|------|--------|--------|
| 1 | Kernel size Option 3 | 30 min | ✅ Done (09a6f79) |
| 2 | Documentation gaps | 2 hrs | ✅ Done (9b62d46) |
| 3a | Boot stack off hardcoded address | 1–2 hrs | ✅ Done (e246db6) |
| 3b | IST for `#DF` | 1 hr | ✅ Done (f4d7df3) |
| 3c | Page table teardown | 2–3 hrs | Defer until multiple processes |
| 3d | `heap_base` per-process | 15 min | Whenever |
| 3e | TLB flush in VMM | — | Leave as-is, documented |
| 4a | Dead declarations | 15 min | ✅ Done (20260919F) |
| 4b | Double-build in `run` | 15 min | ✅ Done (20260919F) |
| 4c | Stale comments | 30 min | ✅ Done (20260919F) |
| 4d | Serial/VGA output atomicity | 1 hr | ✅ Done (20260919G, 20260919H) |
| 4e | Print lock → ring buffer | 1–2 hrs | Before tty/per-user console |
| 4f | Print functions as leaf functions | — | Documented invariant |
| 4g | Print-lock hold diagnostic | 30 min | Build when needed |
| 5a | Kernel-shell self-test | 1 hr | Next available slot |
| 5b | Boot-time self-test | 1 hr | After 5a |
| 5c | `make test` target | 1 hr | After 5b |

Everything on this list is either done, deferred, or architectural.
Nothing urgent remains. The natural next steps, in order of value:

1. **Testing infrastructure (5a–5c)** — three sessions, and it pays off
   every time you change the kernel afterward. Do this before starting
   new features.
2. **The ring buffer (4e)** — the correct long-term design for the print
   path. Do this before the tty subsystem lands.
3. **`sys_exec`** — the next feature milestone (user programs from disk).
   Not on this list because it's a feature, not maintenance.

---

## Notes

This file is not a wish list. Everything in it is either:

- **Known debt:** the design has an acknowledged limitation (the size
  ceiling, the page table leak, the print lock).
- **Missing documentation:** the code is correct but the reader can't
  find out what it does.
- **Cosmetic:** it works, it's just ugly.

If you fix something, strike it through and mark it with a ✅ rather
than deleting it. The history of what was wrong is useful to a reader.

If you find a new problem, add it here with the same format. The point
is to keep the list of "things we know we're wrong about" honest and short.

Feature work goes in `ROADMAP.md`. Capability tracking goes in
`OSDev_Checklist.md`. Debt and maintenance go here.
