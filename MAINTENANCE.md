# MAINTENANCE
### dons-os (x86_64) — Known Debt, Latent Bugs, and Maintenance Work

This document lists everything that is *known to be unfinished, fragile,
or debt-laden* in the current codebase, but which is not a feature. It is
the counterpart to `ROADMAP.md` (which lists features) and
`OSDev_Checklist.md` (which tracks capability). If a thing is wrong, or
fragile, or will bite later, it belongs here.

Items are ordered by priority: do the ones at the top first. Effort is a
rough estimate, not a commitment.

---

## 1. Kernel size ceiling (176 KB staging limit)

**Status:** binding today; ~29 KB of headroom at v0.5.0
**Effort:** 30 minutes (Option 3), 2–3 hours (Option 2), a few days (Option 6)
**Risk if ignored:** the kernel grows past the ceiling and the Makefile's
tripwire fails the build. That is the *good* outcome. The bad outcome is
that someone removes the tripwire and the kernel silently truncates at
boot, producing a machine that boots to garbage.

### What the limits are

Two ceilings apply to `kernel.bin`:

1. **Staging ceiling: 176 KB.** `stage2.asm` reads the kernel from disk in
   three passes of 128 sectors each, staging it into low memory at
   `0x80000`, `0x90000`, and `0x20000`, then copies it to `0x100000` in
   long mode. That staging layout cannot exceed 176 KB (128 KB from the
   first two passes, plus 48 KB from the third). The tripwire in
   `04_kernel_64bit/Makefile` (`KERNEL_STAGING_LIMIT := 180224`) fails
   the build loudly if `kernel.bin` exceeds it.

2. **Disk-layout ceiling: 983 KB.** The kernel starts at LBA 128. The
   FAT partition starts at LBA 2048. So the kernel region is
   `(2048 - 128) × 512 = 983040` bytes. This ceiling is enforced by
   `KERNEL_DISK_LIMIT := 983040` in the same Makefile. It will not bind
   until the staging ceiling has been raised well past 176 KB.

The staging ceiling is the binding one today.

### How to raise it

**Option 3 — use the unused low memory (30 min, ceiling → 448 KB).**
`0x30000–0x7FFFF` (320 KB) is plain RAM, unused by the boot chain. Add
PASS 4–7 to `stage2.asm` reading into segments `0x3000` through `0x7000`,
and add a `rep movsq` in long mode to copy the new bytes to `0x12C000`
onward. Bump `KERNEL_TOTAL_BYTES` in `stage2.asm` to `448 * 1024` and
`KERNEL_STAGING_LIMIT` in the Makefile to `458752`.

Bonus: this also moves PASS 3 off `0x20000`, which is a robustness win
on real hardware. `0x20000–0x2FFFF` is conventional-free on QEMU/SeaBIOS
but not guaranteed by spec on bare metal. `0x30000–0x7FFFF` is safer.

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

### When to do this

Do Option 3 when the tripwire fires or when you want to stop thinking
about the ceiling. Do Option 2 only when 448 KB is not enough. Do
Option 6 when you decide to modernize the boot chain — the right time
is before adding USB or a network stack, because those are the features
that benefit most from Limine.

---

## 2. Documentation gaps

**Status:** current README is accurate but incomplete in three places
**Effort:** ~2 hours total

### 2a. Kernel size limits are not documented in the README

See item 1 above. A reader has no idea the tripwire exists until it
fires. Add a short "Kernel size limits" section to the README under
Known Limitations, or fold it into the Build System notes.

### 2b. The `run` script is documented in one line

The README mentions it in the repository structure and one sentence in
Building & Running. The menu design ("uncomment one line in `menu()`,
then run `./run`") deserves a short paragraph. Otherwise a reader sees a
shell script with a function that's never called and has no idea how
it's supposed to be used.

### 2c. The user shell table is out of date

Options 1–7 are listed. Option 8 (list known files) and option 9
(reboot) are not. If the reboot change is kept, update the table.

---

## 3. Latent bugs (things that will bite eventually)

### 3a. Hardcoded boot stack in `process_exit`

**Status:** latent; works today, breaks if the identity map changes
**Effort:** 1–2 hours

`process_exit`'s fallback to `kmain_shell_loop` sets
`rsp = 0xFFFFFFFF8008FF00`. That address is in the low 1 MB region,
currently mapped because the bootloader identity-maps it. Nothing in the
kernel guarantees it stays mapped. If you ever change the identity map,
this becomes a crash.

**Fix:** allocate the kernel shell's stack from the kernel stack pool
(`kernel_stack_alloc()`), set the shell up on that stack, free it on
exit. The fallback path in `process_exit` then sets `rsp` to the pool
stack instead of a hardcoded address.

### 3b. No IST for `#DF`

**Status:** latent; a real double fault triple-faults with no diagnostic
**Effort:** ~1 hour

Double faults currently produce a triple fault, which resets the CPU
without printing anything. QEMU shows the reset; on real hardware you'd
get a silent reboot. A small IST stack and an IST entry on the `#DF` IDT
gate turns this into a printed diagnostic.

**Fix:** add an IST stack (4 KB is plenty) to the TSS, set the `#DF`
gate's IST field to the new slot in `idt.c`, and have `isr8_handler`
print RIP, CS, and the error code before halting. The setup is very
similar to the current TSS `rsp0` handling.

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

**Status:** cosmetic, not urgent
**Effort:** ~1 hour total

### 4a. Dead declarations

`include/syscall.h` declares `void sys_reboot(void);`. The kernel-side
handler is now `kernel_do_reboot` (static, in `user_syscall.c`). The
declaration in `syscall.h` is likely unused. Check and remove.

### 4b. The double-build in `run`

The single-drive menu line rebuilds the kernel twice: once explicitly
in the line, and once via `runkernel64-single`, which itself does
`make -C 04_kernel_64bit FAT_CONFIG=single`. Two-line fix: either
drop the explicit rebuild from the menu line, or drop it from the
target. The `run` script's echoed `run: <cmd>` makes this visible in
the log.

### 4c. Stale comments

- `arc2/syscalls.c` has several `FIXED:` comments from earlier sessions.
  They were fixed long ago; the labels are noise now. Remove them.
- `user_shell.c` has a `file_exists` helper that is currently unused.
  Delete it.
- `stage2.asm`'s comments about "was 64 sectors" are accurate but
  accumulate cruft every time the layout changes. Consider a single
  "history" section rather than inline archaeology.

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

After items 1–3. The self-test only matters when there's something worth
testing, and items 1–3 change the kernel in ways that would break any
test you wrote beforehand.

---

## 6. Priorities, one more time

| # | Item | Effort | When |
|---|------|--------|------|
| 1 | Kernel size Option 3 | 30 min | Do first |
| 2 | Documentation gaps | 2 hrs | Do second |
| 3a | Boot stack off hardcoded address | 1–2 hrs | Do third |
| 3b | IST for `#DF` | 1 hr | Same session as 3a |
| 3c | Page table teardown | 2–3 hrs | Defer until multiple processes |
| 3d | `heap_base` per-process | 15 min | Whenever |
| 3e | TLB flush in VMM | — | Leave as-is, documented |
| 4a | Dead declarations | 15 min | Batch with 4b, 4c |
| 4b | Double-build in `run` | 15 min | Batch with 4a, 4c |
| 4c | Stale comments | 30 min | Batch with 4a, 4b |
| 5a | Kernel-shell self-test | 1 hr | After 3a/3b |
| 5b | Boot-time self-test | 1 hr | After 5a |
| 5c | `make test` target | 1 hr | After 5b |

Items 1 and 2 are the natural next two sessions. Items 3a and 3b are a
good third session — they're related and both small. Everything after
that is optional.

---

## Notes

This file is not a wish list. Everything in it is either:

- **Known debt:** the design has an acknowledged limitation (the size
  ceiling, the hardcoded boot stack, the page table leak).
- **Missing documentation:** the code is correct but the reader can't
  find out what it does.
- **Cosmetic:** it works, it's just ugly.

If you fix something, remove it from this file. If you find a new
problem, add it here with the same format. The point is to keep the
list of "things we know are wrong" honest and short.

Feature work goes in `ROADMAP.md`. Capability tracking goes in
`OSDev_Checklist.md`. Debt and maintenance go here.
