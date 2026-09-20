# Full OSDev Milestone Checklist
## dons‑os (x86_64) — Project Progress

For known debt and cleanup work, see [`MAINTENANCE.md`](MAINTENANCE.md).

### Legend
- ✅ **Complete** — Feature implemented and stable
- 🚧 **In Progress** — Partially implemented or in testing
- ☐ **Not Started** — Planned for future development
- ⚠️ **Stable** — Working with known limitations
- ❌ **Not Needed** — Feature not required

---

## 1. Boot & System Initialization (5/5 Complete)

| # | Milestone | Status | Notes |
|---|-----------|--------|-------|
| 1 | **Boot Sector** | ✅ Complete | 16‑bit real‑mode bootloader, BIOS interrupts, disk loading |
| 2 | **Protected Mode Entry** | ✅ Complete | A20 line, GDT, CR0.PE, 32‑bit flat mode |
| 3 | **Basic VGA Console** | ✅ Complete | 80×25 text mode, print routines, cursor control |
| 4 | **Long Mode Entry** | ✅ Complete | PAE paging, PML4/PDPT/PD/PT, IA32_EFER.LME, 64‑bit jump |
| 5 | **64‑bit Kernel Start** | ✅ Complete | `_start`, stack setup, `kmain` entry point |

---

## 2. Core Kernel Features (44/44 Complete)

| # | Milestone | Status | Notes |
|---|-----------|--------|-------|
| 6 | **IDT + ISR Stubs** | ✅ Complete | Exception handlers, interrupt gates, error‑code support. All stubs that call C handlers preserve caller-saved registers. |
| 7 | **PIC Remap** | ✅ Complete | IRQ0–IRQ15 mapped to 0x20–0x2F |
| 8 | **PIT Timer** | ✅ Complete | IRQ0 tick counter, scheduling foundation |
| 9 | **Keyboard Driver** | ✅ Complete | IRQ1, scancode set 1, shift/caps, input buffer |
| 10 | **VGA Console Upgrade** | ✅ Complete | Scrolling, cursor control, shell‑ready console |
| 11 | **Kernel Shell** | ✅ Complete | Diagnostic console reached via `k` at boot. Commands: help, clear, info, mem, version, reboot, pmmtest, test, vmmtest, serialtest, heapstat, maptest, testrec, heaptest, heapcheck, heapstress, nxtest, syscall, elfload, proclist, proccreate, vmmclone, runproc, schstat, testyield, usershell, gdtdump, tssdump, selftest, atatest, fatmount, fatls, fatcat |
| 12 | **E820 Memory Map** | ✅ Complete | Memory detection, BootInfo struct passed to kernel |
| 13 | **Physical Memory Manager** | ✅ Complete ⭐ v0.5.3 | Bitmap allocator, page alloc/free, reserved region marking. **Reentrancy fix (v0.5.3): `pmm_alloc_page`'s test-and-set is now atomic with respect to a timer IRQ; `pmm_free_page`, `pmm_reserve_page`, `pmm_unreserve_page` also wrapped in cli/sti critical sections.** |
| 14 | **Virtual Memory Manager** | ✅ Complete ⭐ v0.5.3 | Recursive paging at PML4[510]. HHDM mapping at PML4[256]. Dynamic page table allocation. User-space page mapping with PT_USER. **NX bit support via PT_NX, with EFER.NXE enabled so the CPU enforces it (v0.5.1).** **Dynamic HHDM mapping via `ensure_hhdm_mapped()`.** **Deep page table cloning via `vmm_clone_page_table()` (v0.5.3).** |
| 15 | **Serial Debug Output** | ✅ Complete | COM1 serial output for kernel debugging alongside VGA |
| 16 | **Kernel Heap Allocator** | ✅ Complete ⭐ v0.5.2 | `kmalloc()`/`kfree()` with free list, `heapstat`/`heaptest`. 1 MB initial heap with automatic expansion up to 24 MB. Rewritten in v0.5.2: `heap_extend` places new blocks at the start of the newly mapped region (the old version lost up to 4 KB per extension). Header padded to 48 bytes so payloads are 16-aligned. `heap_validate()` checks every block-list invariant; `heap_stress()` exercises the extension path (grows the heap to ~4.15 MB, zero leak). `heapcheck` and `heapstress` shell commands. |
| 17 | **User Mode (Ring 3)** | ✅ Complete | GDT with user segments (0x33 code, 0x2B data). TSS configured for stack switching. `iretq` transition. CPL=3 with page protection. |
| 18 | **NX (No Execute) Bit Support** | ✅ Complete | PT_NX flag in `vmm.h` (bit 63). NX handling in `vmm_map_page()`. `nxtest` and `vmmtest` verify. |
| 19 | **System Calls** | ✅ Complete ⭐ v0.5.3 | SYSCALL/SYSRET via MSRs. SYS_WRITE (#1), SYS_EXIT (#2), SYS_READ (#3), SYS_OPEN (#4), SYS_CLOSE (#6), SYS_UNLINK (#7), **SYS_EXEC (#8)**, **SYS_WAITPID (#9)**, SYS_BRK (#10), SYS_GETPID (#20), SYS_REBOOT (#25). `syscall` test command. **Safe user-space access via `safe_copy_from_user()`/`safe_copy_to_user()`.** |
| 20 | **ELF Loader** | ✅ Complete ⭐ FINALIZED | Parses ELF64, maps LOAD segments with permissions, allocates user stack, transitions to Ring 3 via IRETQ, `elfload` command. Works on first boot. |
| 21 | **Process Foundation** | ✅ Complete | PCB structure, `process_create`, `proclist`, `vmmclone`. Ready-queue infrastructure. |
| 22 | **BootInfo Fix** | ✅ Complete | Fixed BootInfo alignment between bootloader and kernel. Magic number and version validation. |
| 23 | **Process Stack Setup** | ✅ Complete | Static kernel stack pool. Dedicated user/kernel stacks. `process_destroy()`. `runproc` command. |
| 24 | **Cooperative Scheduler** | ✅ Complete | Ready queue, round-robin. `process_yield()`, `process_exit()`. `context_switch.asm`. `testyield`, `schstat`. |
| 25 | **Preemptive Scheduler** | ✅ Complete | PIT timer (100 Hz) preempts user and kernel mode. Quantum-based slicing. Timer saves/restores per-process kernel frames. |
| 26 | **Segment-Shifting Bootloader** | ✅ Complete | `stage2.asm` reads in 128-sector chunks, advancing segment offsets. Kernel size ceiling lifted to 448 KB (v0.5.1). |
| 27 | **Userland Syscall Reboot** | ✅ Complete | `SYS_REBOOT` (syscall #25) maps Ring 3 shell option 9 to a Ring 0 triple-fault reboot. |
| 28 | **Kernel-Stack-on-Syscall-Entry** | ✅ Complete ⭐ v0.4.7 | Syscall path runs on a per-process kernel stack. `g_syscall_stack_top` maintained by scheduler in lockstep with `TSS.RSP0`. |
| 29 | **newlib 4.x Userland C Library** | ✅ Complete ⭐ v0.4.7 | `libc.a`/`libm.a` statically linked. `printf`, `malloc`/`free`, `memcpy`, `str*`, `setvbuf`, `errno`. Reentrancy initialized via `_impure_ptr = &_impure_data`. |
| 30 | **Blocking `sys_read`** | ✅ Complete ⭐ v0.4.7 | Reads no longer spin the CPU. Process marks BLOCKED, yields via timer, woken by `irq1`. Timer skips blocked processes. |
| 31 | **Boot-Time Shell Choice** | ✅ Complete ⭐ v0.4.7 | 2-second window at boot. `k` → kernel shell, else → user shell. Kernel shell not reachable after boot. |
| 32 | **Userland Heap via `sys_brk`** | ✅ Complete ⭐ v0.4.7 | newlib `malloc`/`free` over `sys_brk` → page mapping. Exercised by user shell menu option 3. |
| 33 | **`gdtdump` / `tssdump`** | ✅ Complete ⭐ v0.4.7 | On-demand kernel shell commands to inspect the GDT and TSS. |
| 34 | **Scheduler / TSS / Interrupt ABI Stability Pass** | ✅ Complete ⭐ v0.4.9 | Seven bugs fixed: ELF-load race, TSS lockstep, fallback TSS restoration, kernel stack slot aliasing, `context_switch.asm` offsets, `scheduler_switch_to` preemption window, `irq1_stub` ABI violation. Plus frame validation in `context_switch.asm` and `_Static_assert` layout guards in `process.c`. |
| 35 | **ATA PIO Block Device Driver** | ✅ Complete ⭐ v0.4.10 | Primary-channel ATA in PIO (polled) mode, LBA28. Per-drive read/write entry points. FLUSH CACHE for durability. Write-protect floor on master. Three bring-up bugs found and fixed: PIC mask restoration, exception frame offsets, LBA mode bit. `atatest` diagnostic. |
| 36 | **FatFs Integration** | ✅ Complete ⭐ v0.5.2 | FatFs R0.16 vendored with `diskio.c` shim over the ATA driver. `FF_FS_READONLY = 0`. Dual-drive storage: kernel on master, FAT16 volume on slave. Kernel shell commands `fatmount`, `fatls`, `fatcat`. Long filename support added in v0.5.2. |
| 37 | **Userland File I/O over FatFs** | ✅ Complete ⭐ v0.4.10 | Per-process file table in the PCB. `SYS_OPEN` (4), `SYS_CLOSE` (6), `SYS_GETPID` (20), fd branch in `SYS_READ` (3). Newlib `open`/`close`/`read`/`write` work from Ring 3. Verified create/write/close/reopen/read round-trip byte-exact. |
| 38 | **Single-Drive Layout** | ✅ Complete ⭐ v0.5.0 | `FAT_CONFIG=dual|single` selects the layout. FAT16 partition at LBA 2048. `diskio.c` applies `FAT_PARTITION_OFFSET`. `FF_MULTI_PARTITION = 0` means FatFs is not partition-aware, so the offset lives in `diskio.c`, not the BPB. `hdd-single.img` built by `mkfs.vfat --offset=2048 -h 2048` and populated by `mcopy`. |
| 39 | **Persistence Across Reboot** | ✅ Complete ⭐ v0.5.0 | Verified end-to-end: user shell option 5 writes in one boot, verifies byte-exact after reboot on the same image. Proves writes are durable, not just buffered. |
| 40 | **Config Diagnostic at Boot** | ✅ Complete ⭐ v0.5.0 | `kmain.c` prints a config-specific storage line to VGA and serial (`"Storage: single-drive, FAT@LBA 2048"` vs `"Storage: dual-drive, FAT@LBA 0 on slave"`). Mount failures now appear on VGA, not just serial. |
| 41 | **Self-Test Infrastructure** | ✅ Complete ⭐ v0.5.1 | `selftest` kernel shell command runs 17 tests and prints a pass/fail summary. Coverage: GDT descriptor contents, TSS fields, PMM allocation/free, VMM control registers, page mapping, recursive paging, heap integrity, NX bit in final PTE, syscall entry point, ATA reads, FatFs mount/directory listing, and the three exception handlers (#DE, #PF, #GP). Expected-fault protocol lets the exception tests recover cleanly. |
| 42 | **`EFER.NXE` Enabled (NX on Hardware)** | ✅ Complete ⭐ v0.5.1 | Previously the kernel wrote NX bits into PTEs via `PT_NX` but the CPU ignored them because `EFER.NXE` was clear. `enable_nx()` in `kmain.c` now sets the bit, guarded by a CPUID check. `test_vmm` asserts on it. |
| 43 | **Kernel-Owned GDT (Higher-Half)** | ✅ Complete ⭐ v0.5.1 | Previously the GDT lived in low memory (base `0x101DC`, inside `stage2.asm`'s loaded image). `gdt_init` now builds `kernel_gdt[16]` in `.bss` at a higher-half address, `lgdt`s it, and reloads the segment registers. `gdt_set_tss` writes the TSS descriptor directly. `gdt_fix_user_segments` became a no-op. |
| 44 | **Print Atomicity (Shared Serial/VGA Lock)** | ✅ Complete ⭐ v0.5.1 | Serial and VGA drivers share a single print lock (cli/sti critical section with nesting counter and RFLAGS save/restore). Multi-part boot messages wrap in `serial_lock`/`serial_unlock`. The DonsDOS banner truncation and interleaved boot trace are gone. |
| 45 | **`#DF` through IST1** | ✅ Complete ⭐ v0.5.1 | A dedicated 4 KB stack and an IST entry on the `#DF` gate turn a double fault into a printed diagnostic instead of a silent triple-fault reset. |
| 46 | **Kernel Shell on a Pool-Allocated Stack** | ✅ Complete ⭐ v0.5.1 | The kernel shell is now a real process (`kshell`) with a stack from the kernel stack pool, not a hardcoded address. `0xFFFFFFFF8008FF00` is gone. |
| 47 | **Heap Rewrite and Validator** | ✅ Complete ⭐ v0.5.2 | `heap_extend` places new blocks at the start of the new region. Header padded to 48 bytes. `heap_validate()` checks every invariant. `heap_stress()` grows the heap from 1 MB to ~4.15 MB with zero leak. `heapcheck` and `heapstress` commands; both in selftest. |
| 48 | **Long Filename Support** | ✅ Complete ⭐ v0.5.2 | `FF_USE_LFN = 2`, `FF_LFN_UNICODE = 2`, `FF_CODE_PAGE = 437`. `fatfs/ffunicode.o` linked. `fatls` shows `HELLO-WORLD.TXT`; `fatcat HELLO-WORLD.TXT` reads it. `sys_open`'s path buffer enlarged to 300 bytes; `copy_user_string` helper respects the buffer size. Kernel 158 KB → 163 KB. |
| 49 | **`SYS_UNLINK` (syscall #7)** | ✅ Complete ⭐ v0.5.2 | Wraps FatFs `f_unlink`. Kernel handler `sys_unlink` in `user_syscall.c`; userland `unlink()` shim in `arc2/syscalls.c`. The user shell's multi-file test (option 6) now deletes the files it creates and confirms they are gone. |
| 50 | **Makefile Header Dependency Tracking** | ✅ Complete ⭐ v0.5.2 | `-MMD -MP` in CFLAGS; `-include $(OBJS:.o=.d)`. Header changes now trigger the right rebuilds automatically. This was the root cause of several stale-object debugging sessions. |
| 51 | **Cross-GCC for FatFs (`ff.o`, `ffunicode.o`)** | ✅ Complete ⭐ v0.5.2 | `fatfs/ff.o` and `fatfs/ffunicode.o` compiled with `/opt/cross/bin/x86_64-elf-gcc` at `-O2`. Clang 22.1.8 miscompiles `ff.c` at every level: `-O0` breaks the FILINFO read path, `-O1` hangs in `f_unlink`, `-O2` produces LLD-truncated instructions. GCC produces correct code and links cleanly with the Clang-built kernel. See `LLD_BUG_REPORT.md`. |
| 52 | **`SYS_EXEC` — Disk-Loaded ELF User Programs** | ✅ Complete ⭐ v0.5.3 | Opens an ELF file on the FAT volume, loads it into a new process, returns the child's pid. `spawn()` shim in `arc2/syscalls.c`. First feature that runs user programs loaded from disk rather than embedded in the kernel image. |
| 53 | **`SYS_WAITPID` — Parent/Child Synchronization** | ✅ Complete ⭐ v0.5.3 | Blocks the calling process until a matching child exits, then reaps it and returns its exit status. `WNOHANG` supported. `waitpid()` shim in `arc2/syscalls.c`. |
| 54 | **Process Parent/Child Tracking** | ✅ Complete ⭐ v0.5.3 | New `pcb_t` fields `parent_pid`, `exit_status`, `wait_pid`; new `PROC_STATE_ZOMBIE`. A process with a parent becomes a zombie on exit and is reaped by `sys_waitpid`; a process with no parent reclaims itself immediately. `process_wake_parent_if_waiting` in `process.c`. |
| 55 | **PMM Allocator Reentrancy Fix** | ✅ Complete ⭐ v0.5.3 | `pmm_alloc_page`'s test-and-set on the shared bitmap was non-atomic; a timer IRQ between the read and write could hand the same page out twice. Fixed with cli/sti critical sections using `pushfq`/`pop` to preserve the caller's IF. Applied to `pmm_alloc_page`, `pmm_free_page`, `pmm_reserve_page`, `pmm_unreserve_page`. |
| 56 | **Page-Table Aliasing Fix (Deep Clone)** | ✅ Complete ⭐ v0.5.3 | `vmm_clone_page_table` was a shallow copy: parent and child shared every PDPT, PD, and PT. Any `vmm_map_page_in_cr3` on the child overwrote the parent's PTEs. Fixed with a deep copy of the low-half hierarchy (PML4[0..255]); the high half (HHDM and kernel) remains shared by design. |
| 57 | **`context_switch` Resume-by-Frame Fix** | ✅ Complete ⭐ v0.5.3 | The resume side chose user vs kernel by comparing `next->entry_point` against `KERNEL_BASE`, which is wrong for a process preempted or blocked in kernel mode. Fixed by making the resume side trust the saved frame verbatim and choose the validation rule from the frame's own CS. |

---

## 3. Memory Management (8/8 Complete)

| # | Milestone | Status | Notes |
|---|-----------|--------|-------|
| 58 | **Higher‑Half Kernel** | ✅ Complete | Kernel mapped to `0xFFFFFFFF80100000`, identity map preserved |
| 59 | **Virtual Memory Manager** | ✅ Complete ⭐ v0.5.3 | Recursive paging, HHDM, dynamic page tables, NX, dynamic HHDM mapping, deep page table cloning |
| 60 | **Serial Debug Output** | ✅ Complete | COM1 serial output, integrated with QEMU |
| 61 | **Kernel Heap** | ✅ Complete ⭐ v0.5.2 | `kmalloc()`/`kfree()` with free list. Memory reuse verified. Rewritten in v0.5.2 with `heap_validate()` and `heap_stress()`. |
| 62 | **User Memory Mapping** | ✅ Complete | Pages mapped with PT_USER for user/kernel isolation |
| 63 | **NX (No Execute) Bit** | ✅ Complete | PT_NX flag, `nxtest`, NX status in `vmmtest`. Since v0.5.1, `EFER.NXE` is enabled so the CPU actually enforces it. |
| 64 | **HHDM Dynamic Mapping** | ✅ Complete | `ensure_hhdm_mapped()` for on-demand physical memory access. Used by ELF loader and page table cloning. |
| 65 | **`sys_brk` Heap Growth** | ✅ Complete ⭐ v0.4.7 | Per-process heap state in `current->brk_virt`. Pages mapped with `invlpg` after map. |

---

## 4. Storage & File Systems (7/7 Complete)

| # | Milestone | Status | Notes |
|---|-----------|--------|-------|
| 66 | **ATA PIO Driver** | ✅ Complete ⭐ v0.4.10 | Primary channel, LBA28, polled. Per-drive entry points, FLUSH CACHE. Write-protect floor on master. `atatest` diagnostic. |
| 67 | **FatFs Integration** | ✅ Complete ⭐ v0.5.2 | FatFs R0.16, read and write, long filename support. Kernel shell and userland access. `ff.o` and `ffunicode.o` compiled with the cross-GCC. |
| 68 | **Single-Drive Layout** | ✅ Complete ⭐ v0.5.0 | Boot chain + kernel + FAT16 partition on one `hdd.img`. FAT at LBA 2048. |
| 69 | **Persistence Across Reboot** | ✅ Complete ⭐ v0.5.0 | User-shell option 5 proves writes survive reboot. |
| 70 | **Dual-Drive Layout (Retained)** | ✅ Complete | Kernel on master, FAT16 on slave. Remains the default; useful for debugging. |
| 71 | **`SYS_UNLINK`** | ✅ Complete ⭐ v0.5.2 | Syscall #7, wraps FatFs `f_unlink`. Kernel handler `sys_unlink` in `user_syscall.c`; userland `unlink()` shim in `arc2/syscalls.c`. The user shell's multi-file test (option 6) now deletes the files it creates and confirms they are gone. |
| 72 | **User Programs from Disk (`SYS_EXEC`)** | ✅ Complete ⭐ v0.5.3 | `sys_exec`-style syscall, loads ELF files from the FAT volume. `HELLO.ELF` is copied into the single-drive FAT partition at build time; the user shell's option A loads it and runs it as an independent Ring 3 process. |

---

## 5. User Space & Advanced Features (15/15 Complete)

| # | Milestone | Status | Notes |
|---|-----------|--------|-------|
| 73 | **System Calls** | ✅ Complete ⭐ v0.5.3 | SYS_WRITE, SYS_EXIT, SYS_READ, SYS_OPEN, SYS_CLOSE, SYS_UNLINK, SYS_EXEC, SYS_WAITPID, SYS_BRK, SYS_GETPID, SYS_REBOOT. Safe user-space access. |
| 74 | **ELF Loader** | ✅ Complete ⭐ FINALIZED | ELF64 parsing, user-mode transition, `elfload`. Works on first boot. |
| 75 | **Process Foundation** | ✅ Complete | PCB, `process_create`, `proclist`, `vmmclone` |
| 76 | **Process Stack Setup** | ✅ Complete | Static kernel stack pool, user/kernel stacks, `runproc`, `process_destroy` |
| 77 | **Cooperative Scheduler** | ✅ Complete | Ready queue, `process_yield()`, `process_exit()`, `testyield`, `schstat` |
| 78 | **Preemptive Scheduler** | ✅ Complete | PIT timer preemption, quantum slicing, timer-driven kernel-mode preemption |
| 79 | **newlib Userland C Library** | ✅ Complete ⭐ v0.4.7 | Full newlib 4.x linked into user programs. Standard C available in Ring 3. |
| 80 | **Blocking I/O** | ✅ Complete ⭐ v0.4.7 | `sys_read` on fd 0 blocks via BLOCKED + `hlt`, woken by `irq1`. |
| 81 | **File I/O from Ring 3** | ✅ Complete ⭐ v0.5.2 | `open`/`close`/`read`/`write`/`unlink` on FAT files from userland, with long filename support. |
| 82 | **User-Mode Process Spawning** | ✅ Complete ⭐ v0.5.3 | `spawn()` + `waitpid()` from the user shell. First user-visible demonstration of running a disk-loaded program. |
| 83 | **Process Cleanup on Exit** | ✅ Complete ⭐ v0.4.8 | `process_reclaim` frees ELF pages and user stack pages, marks PCB UNUSED, resets pid, decrements count. `process_exit` runs with interrupts disabled. Page-table teardown deferred. |
| 84 | **Expanded User-Shell Regression Harness** | ✅ Complete ⭐ v0.5.3 | Options 4 (FS round-trip), 5 (persistence across reboot), 6 (multi-file with delete phase), 7 (4 KB round-trip), A (spawn + wait for disk-loaded ELF). All pass in dual-drive and single-drive. |
| 85 | **Slab Allocator** | ❌ Not Needed | Free list already provides memory reuse for kmalloc/kfree |
| 86 | **Per-Process tty / Console Focus** | ☐ Not Started | Prerequisite for multiple concurrent shells. Not urgent until there's more than one shell. |

---

## Summary

| Phase | Completed | Total | Progress |
|-------|-----------|-------|----------|
| Boot & System Init | 5 | 5 | **100%** ✅ |
| Core Kernel | 44 | 44 | **100%** ✅ |
| Memory Management | 8 | 8 | **100%** ✅ |
| Storage & File Systems | 7 | 7 | **100%** ✅ |
| User Space | 14 | 15 | **93%** 🚧 |
| **Overall** | **78** | **79** | **99%** |

All eight checklist categories that were tracked as "capability" are now at 100% except User Space, where per-process tty / console focus remains the last item. Everything else on the roadmap is either a follow-up refinement (ELF loader `PT_NX`), an alternative console path (serial console debug access), a new subsystem (framebuffer, VFS), or a testing-infrastructure improvement (boot-time self-test mode, `make test`).

The Core Kernel count rose from 41 to 44 with the v0.5.3 additions (items 52–57).

---

## Recent Milestone Achievements (Chronological Order — Newest First)

### v0.5.3 — `SYS_EXEC` Complete + Three Memory-Safety Fixes ⭐ NEW
- **`SYS_EXEC` (syscall #8).** Opens an ELF file on the FAT volume, loads it into a new process, and returns its pid. `spawn("0:/HELLO.ELF")` from the user shell loads `HELLO.ELF` from disk at runtime; the child runs as an independent Ring 3 process and prints its banner.
- **`SYS_WAITPID` (syscall #9).** Blocks the calling process until a matching child exits, then reaps it and returns its exit status. `WNOHANG` supported.
- **`spawn()` and `waitpid()` shims** in `arc2/syscalls.c`, and `apps/include/donsdos.h`. Declared for user programs.
- **Process parent/child tracking.** New `pcb_t` fields `parent_pid`, `exit_status`, `wait_pid`, and a `PROC_STATE_ZOMBIE` state. A process with a parent becomes a zombie on exit and is reaped by `sys_waitpid`; a process with no parent reclaims itself immediately.
- **Three latent memory-safety bugs found and fixed during bring-up.** Each was independently sufficient to make `SYS_EXEC` flaky, and each is documented in `MAINTENANCE.md` §3j with the specific symptom it caused:
  1. **PMM allocator reentrancy.** `pmm_alloc_page` did a non-atomic read-modify-write on the bitmap; a timer IRQ between `bitmap_test` and `bitmap_set` could hand the same page out twice. Fixed with cli/sti critical sections.
  2. **Page-table aliasing in `vmm_clone_page_table`.** The clone was a shallow copy of the PML4: parent and child shared every PDPT, PD, and PT. Fixed with a deep copy of the low-half hierarchy.
  3. **`context_switch` resumed processes by `entry_point`.** The resume side chose user vs kernel by comparing `next->entry_point` against `KERNEL_BASE`, which is wrong for a process preempted or blocked in kernel mode. Fixed by making the resume side trust the saved frame verbatim.
- **`HELLO.ELF` built and copied into the FAT partition** at image-build time by `05_boot_kernel64/Makefile`.
- All prior features remain functional. `selftest` still passes 17/17; user shell options 1–9 still work; option A now spawns a disk-loaded ELF and waits for it.

### v0.5.2 — Heap Rewrite, LFN, SYS_UNLINK, Cross-GCC for FatFs, Makefile Dependency Tracking
- **heap.c rewritten.** `heap_extend` now returns the start of the newly mapped region. Header padded to 48 bytes.
- **`heap_validate()` and `heap_stress()`.** `heap_stress` grows the heap from 1 MB to ~4.15 MB with zero leak. `heapcheck` and `heapstress` shell commands; both in selftest. Test count 17/17.
- **Long filename support.** `FF_USE_LFN = 2`, `FF_LFN_UNICODE = 2`, `FF_CODE_PAGE = 437`. `fatls` shows `HELLO-WORLD.TXT`.
- **`SYS_UNLINK` (syscall #7).** User shell's multi-file test (option 6) now deletes the files it creates and verifies they are gone.
- **Cross-GCC for FatFs.** `fatfs/ff.o` and `fatfs/ffunicode.o` compiled with `/opt/cross/bin/x86_64-elf-gcc`. See `LLD_BUG_REPORT.md`.
- **Makefile header dependency tracking.** `-MMD -MP` in CFLAGS; `-include $(OBJS:.o=.d)`.

### v0.5.1 — Maintenance Batch: Self-Test, NX on Hardware, Kernel-Owned GDT
- **Self-test infrastructure (`selftest` command).** 15 tests covering GDT, TSS, PMM, VMM, heap, NX, syscalls, ATA, FatFs, and the three exception handlers. Expected-fault protocol.
- **`EFER.NXE` enabled.** `enable_nx()` in `kmain.c` sets the bit, guarded by a CPUID check.
- **Kernel-owned GDT.** `gdt_init` builds `kernel_gdt[16]` in `.bss` at a higher-half address.
- **Print atomicity.** Shared serial/VGA lock.
- **#DF through IST1.** Dedicated 4 KB stack; double faults print diagnostics.
- **Kernel shell is process-backed.** Runs as a real process (`kshell`) with a pool-allocated stack.
- **Staging ceiling raised** from 176 KB to 448 KB.
- **Reboot flushes file handles.**

### v0.5.0 — Storage Layer Complete
- ATA PIO block device driver on the primary channel.
- FatFs R0.16 vendored. Read and write, kernel-side and userland.
- Dual-drive and single-drive layouts. `FAT_CONFIG=dual|single` selects.
- Userland file I/O: `SYS_OPEN` (4), `SYS_CLOSE` (6), fd branch in `SYS_READ` (3).
- Persistence across reboot verified end-to-end by user shell option 5.

### v0.4.9 — Scheduler, TSS, and Interrupt ABI Stability Pass
- ELF-load race at boot fixed at all three call sites.
- TSS.RSP0 / `g_syscall_stack_top` lockstep.
- Kernel stack slot aliasing fixed.
- `context_switch.asm` offsets updated; `_Static_assert` guards added.
- `scheduler_switch_to` preemption window closed.
- `irq1_stub` ABI violation fixed.
- Frame validation in `context_switch.asm`.

### v0.4.8 — Process Cleanup on Exit
- PCB reclaim.
- `process_exit` timer race closed.
- Page-table teardown deferred.

### v0.4.7 — newlib, Blocking I/O, Boot Choice
- newlib 4.x in userland.
- Kernel-stack-on-syscall-entry.
- Blocking `sys_read`.
- Boot-time shell choice.
- User shell is the terminal console.
- Userland heap test: user shell menu option 3.
- `gdtdump` / `tssdump` kernel shell commands.
- `testyield` fix.
- TSS.RSP0 / `g_syscall_stack_top` in lockstep from boot.

### v0.4.6 — Preemptive & Unlocked Core Milestone
- Preemptive Scheduler 100% complete.
- Multi-pass segment register incrementing in `stage2.asm`.
- `SYS_REBOOT` (syscall #25) links Ring 3 user shell option 9 to a safe Ring 0 triple-fault restart.

### v0.4.5 — Cooperative Scheduler
- Ready queue with round-robin scheduling.
- `process_yield()` for voluntary context switching.
- `process_exit()` for clean process termination.
- Assembly-level context switching (`context_switch.asm`).
- `testyield` command, `schstat` command.

### v0.4.4 — Process Stack Setup
- Static kernel stack pool for processes.
- Process creation with dedicated user and kernel stacks.
- Process cleanup with `process_destroy()`.
- `runproc` command.

### v0.4.3 — ELF Loader Stabilized + Process Foundation
- ELF loader works on **first boot**.
- Bootloader identity-mapping conflict resolved.
- Safe HHDM-based user-space memory access.
- Process Foundation: PCB, `process_create()`, `proclist`, `proccreate`, `vmmclone`.
- Dynamic HHDM mapping.

### v0.4.2 — STAR MSR Fix
- Fixed IA32_STAR MSR configuration for SYSCALL/SYSRET.

### v0.4.1 — Syscall Stack Stability
- Unified kernel stack model.
- Correct SYSRET return path.

### v0.4.0 — ELF Loader
- Fully functional ELF64 loader.
- `elfload` command.
- "Hello from Userland!" verified.

### v0.3.2 — System Calls
- SYSCALL/SYSRET MSR setup.
- SYS_WRITE + SYS_EXIT.

### v0.3.1 — NX Bit Support
- PT_NX flag.
- `nxtest` command.

### v0.3.0 — User Mode (Ring 3)
- GDT user segments.
- TSS stack switching.
- IRETQ transition.

---

## Next Steps (Recommended Order)

1. **Boot-time self-test mode and `make test`** — the last two pieces of the self-test work. 5b runs the same 17 tests at boot and halts; 5c wraps 5b in a headless QEMU invocation and greps the serial log for the summary line. See [`MAINTENANCE.md`](MAINTENANCE.md) §5b–5c.
2. **Spawn regression test** — a kernel-mode self-test child that opens `0:/HELLO.ELF`, spawns it, waits for it, and asserts exit status 0. This makes the three v0.5.3 fixes regression-detectable. See [`MAINTENANCE.md`](MAINTENANCE.md) §5d.
3. **A real shell with external commands** — the user shell's menu is a fixed list; `SYS_EXEC` and `SYS_WAITPID` give it the primitives to run arbitrary programs by name. This is the natural next feature now that the loader works end to end.
4. **ELF Loader `PT_NX` follow-up** — with `EFER.NXE` now enabled, `elf_load_into_process` can mark non-executable segments (data, BSS, user stack) with `PT_NX`. About an hour of work; closes the follow-up noted in `MAINTENANCE.md` §3g.
5. **Page-table teardown on process exit** — walk the process's page tables and free the user-space portion. The v0.5.3 deep clone makes each process own more page-table pages than before, so this is now more valuable. See [`MAINTENANCE.md`](MAINTENANCE.md) §3c.
6. **Serial Console Debug Access** — kernel shell over COM1 (the right shape for runtime kernel-shell access; the magic-key-combo approach was tried and abandoned).
7. **Framebuffer Graphics** — move from VGA text mode to graphics.
8. **File System (VFS)** — VFS layer above FatFs.

---

*Last Updated: September 2026 (v0.5.3)*
