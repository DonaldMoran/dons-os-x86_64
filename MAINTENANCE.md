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
`runproc` / `testyield` / `selftest` cycles in one boot
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

**Interaction with selftest (5a-ii).** Each `selftest` run spawns three
faulttest children, each of which calls `process_create` and therefore
`vmm_clone_page_table`. `process_reclaim` does not free the cloned
tables. So each `selftest` run leaks three page-table sets (a PML4,
a PDPT, a PD, and a PT per clone, since `vmm_clone_page_table` appears
to allocate a fresh tree). Repeated `selftest` runs in one boot will
show the PMM's free count dropping by a few pages per run.

This is acceptable for now — the leak is small and bounded by the 32-slot
PCB pool — but the "run selftest twice and compare" idempotency check
that a reader might reach for will *fail* on the free-page count until
this item is closed. The check that works today is: the `pmm` test's own
`Free Pages` line is stable across runs, because `pmm` runs before any
of the exception tests.

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

### 3f. Self-test fault triggers must be kernel-mode functions

**Status:** documented constraint; holds today
**Effort:** n/a now; ~1 hour if a user-mode fault test is ever wanted
**Introduced by:** 5a-i (tag `20260919K`)

`fault_kill_current` (in `interrupts.c`) terminates the current process
by calling `process_exit`. `process_exit` uses `exiting->entry_point >=
KERNEL_BASE` to decide between "resume the kernel shell" (kernel-mode
diagnostic) and "halt" (user-mode process). The self-test's three fault
triggers (`fault_de_trigger`, `fault_pf_trigger`, `fault_gp_trigger` in
`kmain.c`) are kernel text, so they take the resume-the-shell path, and
`selftest` gets control back after each fault.

**The constraint:** if a fault trigger is ever made a user-mode
function (e.g. to test a user-mode `#PF` from the self-test), the child
will *halt* on exit instead of resuming the shell, and the self-test
will never get control back. The result is a hang, not a FAIL, because
`g_fault_observed` is set but never read.

**Why this is fine today:** `selftest` is a kernel-shell command, and
all three triggers are kernel-mode by construction. The comment block
above the triggers in `kmain.c` says so.

**What a future refactor would need:** a per-PCB `expected_fault` field
that `process_exit` checks *before* the `entry_point >= KERNEL_BASE`
policy. That decouples "the process terminated by design" from "the
process terminated because it was a kernel diagnostic," which are
different concepts that the current code conflates. Not urgent — no
user-mode fault test exists or is planned.

### 3g. ~~`EFER.NXE` is not enabled~~ ✅

**Status:** ✅ **DONE (tag `20260919N`).** `enable_nx()` in `kmain.c` now
sets EFER.NXE (bit 11 of MSR `0xC0000080`) before `sti`, guarded by a
CPUID.80000001H:EDX.NX check.  `test_vmm` now asserts on NXE rather
than printing it as an observation.  Verified end-to-end on single-drive
under KVM: `NX Active: Yes`, `15 passed, 0 failed`, and the user shell's
malloc test (option 3) still works with NXE on.

Text below kept for history: it describes the state before the fix.

~~**Effort:** ~15 minutes to set the bit, ~1 hour to verify safely~~
~~**Found by:** 5a-ii (`test_vmm`'s NXE assertion, tag `20260919L`)~~

~~`vmm_map_page` and `vmm_map_page_in_cr3` write bit 63 of a PTE when
called with the `PT_NX` flag, and `test_nx` confirms the bit lands in
the PTE.  But `EFER.NXE` (bit 11 of MSR `0xC0000080`) is clear:~~

```
=== VMM Dashboard ===
  CR3 Root : 0x00000000002FB000
  WP Active: No
  NX Active: No
```

~~`user_syscall_init` sets `EFER.SCE` (bit 0) but not `EFER.NXE`.  Nothing
else in the boot path enables it.  So every page the kernel marks
non-executable is still executable.~~

~~**What this actually meant, mode by mode.**~~

~~- **Under KVM.**  Worked, by accident.  KVM's shadow page-table
  implementation enables NX on the host side regardless of the guest's
  `EFER.NXE` value.  The host MMU honored bit 63 in the guest's PTEs as
  if it were valid, so NX was genuinely enforced at runtime.  This was
  not a property of the guest kernel; it was a property of KVM's shadow
  MMU being more forgiving than the architecture requires.~~

~~- **Under TCG.**  QEMU's software MMU may or may not have emulated the
  reserved-bit rule faithfully.  The observed behavior (no fault on
  `test_nx`'s PTE) suggested TCG was lenient in the QEMU version used.
  Not guaranteed across QEMU releases.~~

~~- **On bare metal.**  The SDM is explicit: "If IA32_EFER.NXE = 0 and
  the P flag of a PDE or a PTE is 1, the XD flag (bit 63) is
  reserved," and a reference using such an entry causes a page-fault
  exception.  A real CPU would `#PF` the moment the kernel touched an
  NX-marked page.~~

~~**The fix (now applied).**  In `kmain`, before `sti`:~~

```c
/* EFER.NXE requires CPUID.80000001H:EDX.NX (bit 20). */
uint32_t eax, ebx, ecx, edx;
__asm__ volatile("cpuid"
                 : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx)
                 : "a"(0x80000001));
if (edx & (1u << 20)) {
    uint64_t efer = rdmsr(0xC0000080);
    wrmsr(0xC0000080, efer | (1ULL << 11));
    PRINT_BOTH("CPU: EFER.NXE enabled\n");
} else {
    PRINT_BOTH("WARN: CPU lacks NX; PT_NX mappings will #PF\n");
}
```

~~The CPUID guard is required, not optional: the SDM says processors
that do not support CPUID leaf `0x80000001` do not allow
`IA32_EFER.NXE` to be set, and `wrmsr` on such a CPU would `#GP`.~~

~~**Why the fix was safe for the kernel.**  No page mapped by `pmm`,
`vmm`, the heap, the kernel stacks, the page tables, or the ELF loader
was mapped with `PT_NX` at the time of the fix.  `elf_load_into_process`
maps every segment with `0x1F`, which is a separate bug (data segments
should be NX), but it meant no currently-mapped page had bit 63 set.
The only pages that ever got `PT_NX` were the ones `test_nx` and
`nxtest` map, and both unmap before returning.  Enabling NXE changed
nothing that ran; it made the bit honest.~~

**Follow-up once NXE is on.**  `elf_load_into_process` should map
non-executable segments (data, BSS, user stack) with `PT_NX`.  With
NXE currently off, that would have been a `#PF`; with it on, it
becomes correct.  This is a natural companion to the fix, not a
prerequisite — the fix is done and verified, the ELF-loader change
is a separate, optional enhancement.

**Claims in the other docs.**  `README.md`, `ROADMAP.md`, and
`OSDev_Checklist.md` describe NX as supported.  With `enable_nx()`
in place, that is now accurate at the hardware level, not just at
the software level.

### 3h. The GDT lives in low memory

**Status:** latent; works because the identity map covers it
**Effort:** 2–3 hours to rebuild the GDT at a higher-half address
**Found by:** 5a-ii (`test_gdt`'s base assertion, tag `20260919L`)

`sgdt` reports the GDT base as `0x101DC`, inside the first 64 KB of
physical memory:

```
GDT base=0x00000000000101DC limit=0x0000000000000047 entries=9
```

This is the bootloader's GDT. `gdt_init` in `gdt.c` is a no-op ("Using
the bootloader's GDT — nothing to build here"), and `entry.asm` never
relocates it into the higher half. The kernel reads the GDT through
the bootloader's identity map.

**Why this is fragile:** the same reason item 3a was fragile before it
was fixed. The identity map is currently stable, but nothing guarantees
it. If it were ever narrowed — e.g. to reclaim low memory for the PMM —
`sgdt` would still return `0x101DC`, and the first `ltr`, `lldt`,
`lgdt`, or segment load that consulted the GDT would fault with a
non-obvious cause. The failure would appear at an unrelated site
(wherever the next descriptor load happens), not at the boot.

**Fix:** rebuild the GDT at a known higher-half virtual address during
boot. In `entry.asm` or `gdt.c`, copy the bootloader's GDT (or build a
fresh one matching the current layout — null, code32, data32, code64,
data64, user_data, user_code, TSS) to a static `.bss` array in the
kernel image, then `lgdt` with the higher-half base. `gdt_fix_user_segments`
and `gdt_set_tss` already write to `sgdt`'s result, so they'd pick up the
new location automatically once `lgdt` runs.

**The self-test's relaxed check.** `test_gdt` currently asserts only that
the base is non-zero, because asserting "base must be in the higher
half" would make `selftest` fail until this item is fixed. Once this is
fixed, change the assertion back to `if (gdt_ptr.base <
0xFFFFFFFF80000000ULL) return SELFTEST_FAIL;` and add a comment
pointing at this item.

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

**Observed symptom, pre-fix, that this design addresses:** in the
`20260919N` capture, the user shell's banner appears twice, with the
second copy truncated mid-word (`ib 4.x User Shell==`).  The kernel's
`PRINT_BOTH` output and the user shell's `printf` (via `sys_write`)
are both writing to the same console, and the kernel's prompt logic
races with the user shell's banner.  The print lock covers kernel-side
prints; it does not cover user-mode `sys_write` output.  This is the
class of interleaving the ring buffer design fixes once a tty layer
exists.  Not urgent.

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

**Status:** partial; fault-path and non-fault coverage exist, context-switch
coverage does not
**Effort:** ~2 hours remaining (5b, 5c)

### 5a. Kernel-shell self-test

**5a-i and 5a-ii are done.  5a-iii is declined.**

- **5a-i ✅ DONE (tag `20260919K`).**  Expected-fault protocol
  (`g_expect_fault` / `g_fault_observed` / `fault_kill_current`) and
  a `selftest` command that runs the three exception tests.  Each
  test spawns a kernel-mode child whose entry point is a small
  trigger function, sets the expected vector, yields to the child,
  and checks `g_fault_observed` when the shell resumes.  The three
  exception handlers (`isr0_handler`, `isr13_handler`,
  `isr14_handler`) gained a guard at the top: if `g_expect_fault`
  matches their vector, they call `fault_kill_current` instead of
  the diagnostic-and-halt path.  Verified end-to-end on
  single-drive: `3 passed, 0 failed`.

  This is the first time the exception handlers have been exercised
  by anything.  Prior to this commit, a broken `isr14_stub` frame
  offset or a mis-wired IDT gate would only have been visible by
  typing `test` and reading the dump by hand.

  Files touched: `interrupts.h`, `interrupts.c`, `kmain.c`.  No
  changes to `process.c`, `scheduler.c`, `context_switch.asm`,
  `isr.asm`, or `process.h`.  The `_Static_assert` offsets in
  `process.c` remain valid because `pcb_t` is untouched.

  **What is not covered by 5a-i:** the handlers' *diagnostic* path
  (`g_expect_fault == -1`) is untested — that's the path the `test`
  command takes.  User-mode faults are untested; see item 3f for the
  coupling that makes this non-trivial.

- **5a-ii ✅ DONE (tag `20260919L`).**  Every non-fault test that
  had an inline body in `handle_command` was refactored into a
  `static int test_xxx(void)` that returns `SELFTEST_PASS` or
  `SELFTEST_FAIL`.  The shell commands became one-line wrappers
  around those functions; `selftest` calls the same functions and
  reads the return value.  Verified end-to-end on single-drive:
  `15 passed, 0 failed`.

  Coverage added:
  - `test_gdt` decodes the GDT and asserts: null descriptor is zero;
    kernel code (`0x18`) present, DPL=0, code, L=1; kernel data
    (`0x20`) present, DPL=0, not code; user data (`0x28`) present,
    DPL=3, not code; user code (`0x30`) present, DPL=3, code, L=1;
    TSS (`0x38`) present, system, type 0x9 or 0xB.
  - `test_tss` asserts: TR == 0x38; RSP0 and IST1 are non-zero,
    16-byte aligned, in the higher half; `iopb_base == sizeof(tss_t)`;
    `g_syscall_stack_top == tss->rsp0` (the v0.4.9 lockstep invariant).
  - `test_pmm`, `test_map`, `test_nx` now free the physical page they
    allocate, so `selftest` is idempotent across runs (modulo item 3c).
  - `test_ata` asserts the 0xAA55 MBR signature in addition to the
    read succeeding.
  - `test_nx` walks the page tables via HHDM and asserts the NX bit
    (bit 63) landed in the final PTE.

  Findings filed during this commit:
  - **Item 3g.** `test_vmm`'s NXE check failed on first run: `EFER.NXE`
    is not set.  The test was relaxed to a printed observation; the
    underlying gap is filed as item 3g.
  - **Item 3h.** `test_gdt`'s base-address check failed on first run:
    the GDT lives at `0x101DC`, in low memory, read through the
    bootloader's identity map.  The test was relaxed to "base must be
    non-zero"; the underlying fragility is filed as item 3h.

  Files touched: `kmain.c` only.  `interrupts.c` and `interrupts.h`
  unchanged from 5a-i.

- **5a-iii — DECLINED.**  "Add `elfload` to `selftest`" was the
  original plan.  It is not being done, and the reason is a design
  decision, not a bug.

  `elfload` launches a *user-mode* process.  When a user process
  exits — by `SYS_EXIT` or by fault — `process_exit` halts the CPU.
  That is the intended behavior: the user shell is the terminal
  interactive console, and there is nothing behind it to return to.
  The kernel shell is a boot-time choice, not a persistent fallback.

  `selftest` is a two-way operation: it runs a test and reads back a
  result.  `elfload` is a one-way operation by design.  Putting it in
  `selftest` would mean the test never completes — not because the
  loader is broken, but because "launch a user process" and "return to
  the caller" are mutually exclusive in this kernel.

  `elfload` is already exercised indirectly: `usershell` uses the same
  `process_create` + `elf_load_into_process` + `scheduler_switch_to`
  path on every boot that drops into the user shell.  If the loader
  regressed, the user shell would fail to start — a much louder signal
  than a self-test line.

  The expected-fault tests in 5a-i work because their children are
  *kernel-mode* processes (`entry_point >= KERNEL_BASE`), which
  `process_exit` resumes the shell for.  That is a property of the
  test harness, not a general rule about user processes.  See item 3f.

  Nothing here needs fixing.  If direct loader coverage is ever
  wanted, the route is a kernel-mode ELF whose entry point is in
  kernel text — which tests a different scenario than `usershell`
  exercises, and is not worth building just for `selftest`.

Original estimate of "1 hr" for 5a was optimistic: the exception
handlers were halting, so a recoverable fault path had to be designed
and built before any of the fault tests could be sequenced.  5a-i
alone was ~2 hrs.  5a-ii was ~1.5 hrs including the two findings.

### 5b. Boot-time self-test mode

A build flag (`-DSELFTEST`) that makes the kernel run a fixed test
sequence at boot and print results to serial, then halt. Useful for
regression checks after a change.  Unlike the interactive `selftest`
command, 5b's whole purpose is "run and stop," so it *can* include a
final user-mode step (launch `usershell`, let it run, halt) — that is
a design question for 5b, not part of the interactive command.

### 5c. `make test` target

A Makefile target that boots QEMU headless, runs the self-test, captures
the serial output, and reports pass/fail based on the output. Automatable
with a serial-to-file and a grep. Would let you catch regressions without
watching the boot.

### When to do the rest

5b and 5c build on 5a-i and 5a-ii.  Neither is blocked by the remaining
items on this list.  They can be done in either order; 5c is more
valuable because it enables `make test` in a loop, but 5b is a
prerequisite for the headless part of 5c.

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
| 3f | Self-test fault-trigger constraint | — | Documented (20260919K) |
| 3g | `EFER.NXE` not enabled | 15 min + 1 hr verify | ✅ Done (20260919N) |
| 3h | GDT lives in low memory | 2–3 hrs | Next feature-sized fix |
| 4a | Dead declarations | 15 min | ✅ Done (20260919F) |
| 4b | Double-build in `run` | 15 min | ✅ Done (20260919F) |
| 4c | Stale comments | 30 min | ✅ Done (20260919F) |
| 4d | Serial/VGA output atomicity | 1 hr | ✅ Done (20260919G, 20260919H) |
| 4e | Print lock → ring buffer | 1–2 hrs | Before tty/per-user console |
| 4f | Print functions as leaf functions | — | Documented invariant |
| 4g | Print-lock hold diagnostic | 30 min | Build when needed |
| 5a-i | Self-test: exception path | 2 hrs | ✅ Done (20260919K) |
| 5a-ii | Self-test: non-fault tests | 1.5 hrs | ✅ Done (20260919L) |
| 5a-iii | `elfload` in selftest | — | Declined (see §5a) |
| 5b | Boot-time self-test mode | 1 hr | Next |
| 5c | `make test` target | 1 hr | After 5b |

Everything on this list is either done, deferred, declined, or
architectural.  Nothing urgent remains.  The natural next steps, in
order of value:

1. **Testing infrastructure (5b, 5c)** — the last two pieces of the
   self-test work.  5b runs the same 15 tests at boot and halts; 5c
   wraps 5b in a headless QEMU invocation and greps the serial log
   for the summary line.  Do these before starting new features.
2. **Item 3h (GDT in low memory)** — same class as the boot-stack
   issue that was fixed in `e246db6`.  Now the highest-priority
   open item.  Not urgent, but every boot log that shows
   `GDT base=0x101DC` is a reminder.
3. **The ring buffer (4e)** — the correct long-term design for the
   print path.  Do this before the tty subsystem lands.  The
   duplicated user-shell banner in the `20260919N` capture is a
   preview of the interleaving this fixes.
4. **`sys_exec`** — the next feature milestone (user programs from
   disk).  Not on this list because it's a feature, not maintenance.

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
