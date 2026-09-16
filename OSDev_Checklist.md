# Full OSDev Milestone Checklist
## dons‑os (x86_64) — Project Progress

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

## 2. Core Kernel Features (31/31 Complete)

| # | Milestone | Status | Notes |
|---|-----------|--------|-------|
| 6 | **IDT + ISR Stubs** | ✅ Complete | Exception handlers, interrupt gates, error‑code support. All stubs that call C handlers preserve caller-saved registers. |
| 7 | **PIC Remap** | ✅ Complete | IRQ0–IRQ15 mapped to 0x20–0x2F |
| 8 | **PIT Timer** | ✅ Complete | IRQ0 tick counter, scheduling foundation |
| 9 | **Keyboard Driver** | ✅ Complete | IRQ1, scancode set 1, shift/caps, input buffer |
| 10 | **VGA Console Upgrade** | ✅ Complete | Scrolling, cursor control, shell‑ready console |
| 11 | **Kernel Shell** | ✅ Complete | Diagnostic console reached via `k` at boot. Commands: help, clear, info, mem, version, reboot, pmmtest, test, vmmtest, serialtest, heapstat, maptest, testrec, heaptest, nxtest, syscall, elfload, proclist, proccreate, vmmclone, runproc, schstat, testyield, usershell, gdtdump, tssdump, atatest, fatmount, fatls, fatcat |
| 12 | **E820 Memory Map** | ✅ Complete | Memory detection, BootInfo struct passed to kernel |
| 13 | **Physical Memory Manager** | ✅ Complete | Bitmap allocator, page alloc/free, reserved region marking |
| 14 | **Virtual Memory Manager** | ✅ Complete | Recursive paging at PML4[510]. HHDM mapping at PML4[256]. Dynamic page table allocation. User-space page mapping with PT_USER. **NX bit support via PT_NX.** **Dynamic HHDM mapping via `ensure_hhdm_mapped()`.** **Page table cloning via `vmm_clone_page_table()`.** |
| 15 | **Serial Debug Output** | ✅ Complete | COM1 serial output for kernel debugging alongside VGA |
| 16 | **Kernel Heap Allocator** | ✅ Complete | `kmalloc()`/`kfree()` with free list, `heapstat`/`heaptest`. 64MB initial heap with automatic expansion. |
| 17 | **User Mode (Ring 3)** | ✅ Complete | GDT with user segments (0x33 code, 0x2B data). TSS configured for stack switching. `iretq` transition. CPL=3 with page protection. |
| 18 | **NX (No Execute) Bit Support** | ✅ Complete | PT_NX flag in `vmm.h` (bit 63). NX handling in `vmm_map_page()`. `nxtest` and `vmmtest` verify. |
| 19 | **System Calls** | ✅ Complete | SYSCALL/SYSRET via MSRs. SYS_WRITE (#1), SYS_EXIT (#2), SYS_READ (#3), SYS_OPEN (#4), SYS_CLOSE (#6), SYS_BRK (#10), SYS_GETPID (#20), SYS_REBOOT (#25). `syscall` test command. **Safe user-space access via `safe_copy_from_user()`/`safe_copy_to_user()`.** |
| 20 | **ELF Loader** | ✅ Complete ⭐ FINALIZED | Parses ELF64, maps LOAD segments with permissions, allocates user stack, transitions to Ring 3 via IRETQ, `elfload` command. Works on first boot. |
| 21 | **Process Foundation** | ✅ Complete | PCB structure, `process_create`, `proclist`, `vmmclone`. Ready-queue infrastructure. |
| 22 | **BootInfo Fix** | ✅ Complete | Fixed BootInfo alignment between bootloader and kernel. Magic number and version validation. |
| 23 | **Process Stack Setup** | ✅ Complete | Static kernel stack pool. Dedicated user/kernel stacks. `process_destroy()`. `runproc` command. |
| 24 | **Cooperative Scheduler** | ✅ Complete | Ready queue, round-robin. `process_yield()`, `process_exit()`. `context_switch.asm`. `testyield`, `schstat`. |
| 25 | **Preemptive Scheduler** | ✅ Complete | PIT timer (100 Hz) preempts user and kernel mode. Quantum-based slicing. Timer saves/restores per-process kernel frames. |
| 26 | **Segment-Shifting Bootloader** | ✅ Complete | `stage2.asm` reads in 128-sector chunks, advancing segment offsets. Kernel size ceiling lifted to 128 KB. |
| 27 | **Userland Syscall Reboot** | ✅ Complete | `SYS_REBOOT` (syscall #25) maps Ring 3 shell option 4 to a Ring 0 triple-fault reboot. |
| 28 | **Kernel-Stack-on-Syscall-Entry** | ✅ Complete ⭐ v0.4.7 | Syscall path runs on a per-process kernel stack. `g_syscall_stack_top` maintained by scheduler in lockstep with `TSS.RSP0`. |
| 29 | **newlib 4.x Userland C Library** | ✅ Complete ⭐ v0.4.7 | `libc.a`/`libm.a` statically linked. `printf`, `malloc`/`free`, `memcpy`, `str*`, `setvbuf`, `errno`. Reentrancy initialized via `_impure_ptr = &_impure_data`. |
| 30 | **Blocking `sys_read`** | ✅ Complete ⭐ v0.4.7 | Reads no longer spin the CPU. Process marks BLOCKED, yields via timer, woken by `irq1`. Timer skips blocked processes. |
| 31 | **Boot-Time Shell Choice** | ✅ Complete ⭐ v0.4.7 | 2-second window at boot. `k` → kernel shell, else → user shell. Kernel shell not reachable after boot. |
| 32 | **Userland Heap via `sys_brk`** | ✅ Complete ⭐ v0.4.7 | newlib `malloc`/`free` over `sys_brk` → page mapping. Exercised by user shell menu option 3. |
| 33 | **`gdtdump` / `tssdump`** | ✅ Complete ⭐ v0.4.7 | On-demand kernel shell commands to inspect the GDT and TSS. |
| 34 | **Scheduler / TSS / Interrupt ABI Stability Pass** | ✅ Complete ⭐ v0.4.9 | Seven bugs fixed: ELF-load race, TSS lockstep, fallback TSS restoration, kernel stack slot aliasing, `context_switch.asm` offsets, `scheduler_switch_to` preemption window, `irq1_stub` ABI violation. Plus frame validation in `context_switch.asm` and `_Static_assert` layout guards in `process.c`. |
| 35 | **ATA PIO Block Device Driver** | ✅ Complete ⭐ v0.4.10 | Primary-channel ATA in PIO (polled) mode, LBA28. Per-drive read/write entry points. FLUSH CACHE for durability. Write-protect floor on master. Three bring-up bugs found and fixed: PIC mask restoration, exception frame offsets, LBA mode bit. `atatest` diagnostic. |
| 36 | **FatFs Integration** | ✅ Complete ⭐ v0.4.10 | FatFs R0.15 vendored with `diskio.c` shim over the ATA driver. `FF_FS_READONLY = 0`. Dual-drive storage: kernel on master, FAT16 volume on slave. Kernel shell commands `fatmount`, `fatls`, `fatcat`. |
| 37 | **Userland File I/O over FatFs** | ✅ Complete ⭐ v0.4.10 | Per-process file table in the PCB. `SYS_OPEN` (4), `SYS_CLOSE` (6), `SYS_GETPID` (20), fd branch in `SYS_READ` (3). Newlib `open`/`close`/`read`/`write` work from Ring 3. Verified create/write/close/reopen/read round-trip byte-exact. |
| 38 | **Single-Drive Layout** | ✅ Complete ⭐ v0.5.0 | `FAT_CONFIG=dual|single` selects the layout. FAT16 partition at LBA 2048. `diskio.c` applies `FAT_PARTITION_OFFSET`. `FF_MULTI_PARTITION = 0` means FatFs is not partition-aware, so the offset lives in `diskio.c`, not the BPB. `hdd-single.img` built by `mkfs.vfat --offset=2048 -h 2048` and populated by `mcopy`. |
| 39 | **Persistence Across Reboot** | ✅ Complete ⭐ v0.5.0 | Verified end-to-end: user shell option 5 writes in one boot, verifies byte-exact after reboot on the same image. Proves writes are durable, not just buffered. |
| 40 | **Config Diagnostic at Boot** | ✅ Complete ⭐ v0.5.0 | `kmain.c` prints a config-specific storage line to VGA and serial (`"Storage: single-drive, FAT@LBA 2048"` vs `"Storage: dual-drive, FAT@LBA 0 on slave"`). Mount failures now appear on VGA, not just serial. |

---

## 3. Memory Management (8/8 Complete)

| # | Milestone | Status | Notes |
|---|-----------|--------|-------|
| 41 | **Higher‑Half Kernel** | ✅ Complete | Kernel mapped to `0xFFFFFFFF80100000`, identity map preserved |
| 42 | **Virtual Memory Manager** | ✅ Complete | Recursive paging, HHDM, dynamic page tables, NX, dynamic HHDM mapping, page table cloning |
| 43 | **Serial Debug Output** | ✅ Complete | COM1 serial output, integrated with QEMU |
| 44 | **Kernel Heap** | ✅ Complete | `kmalloc()`/`kfree()` with free list. Memory reuse verified. |
| 45 | **User Memory Mapping** | ✅ Complete | Pages mapped with PT_USER for user/kernel isolation |
| 46 | **NX (No Execute) Bit** | ✅ Complete | PT_NX flag, `nxtest`, NX status in `vmmtest` |
| 47 | **HHDM Dynamic Mapping** | ✅ Complete | `ensure_hhdm_mapped()` for on-demand physical memory access. Used by ELF loader and page table cloning. |
| 48 | **`sys_brk` Heap Growth** | ✅ Complete ⭐ v0.4.7 | Per-process heap state in `current->brk_virt`. Pages mapped with `invlpg` after map. |

---

## 4. Storage & File Systems (5/7 Complete)

| # | Milestone | Status | Notes |
|---|-----------|--------|-------|
| 49 | **ATA PIO Driver** | ✅ Complete ⭐ v0.4.10 | Primary channel, LBA28, polled. Per-drive entry points, FLUSH CACHE. Write-protect floor on master. `atatest` diagnostic. |
| 50 | **FatFs Integration** | ✅ Complete ⭐ v0.4.10 | FatFs R0.15, read and write. Kernel shell and userland access. |
| 51 | **Single-Drive Layout** | ✅ Complete ⭐ v0.5.0 | Boot chain + kernel + FAT16 partition on one `hdd.img`. FAT at LBA 2048. |
| 52 | **Persistence Across Reboot** | ✅ Complete ⭐ v0.5.0 | User-shell option 5 proves writes survive reboot. |
| 53 | **Dual-Drive Layout (Retained)** | ✅ Complete | Kernel on master, FAT16 on slave. Remains the default; useful for debugging. |
| 54 | **`SYS_UNLINK`** | ☐ Not Started | `f_unlink` behind a syscall. Completes the user-shell multi-file test. |
| 55 | **User Programs from Disk** | ☐ Not Started | `sys_exec`-style syscall, load ELF files from the FAT volume. Enables external commands in a real shell. |

---

## 5. User Space & Advanced Features (11/14 Complete)

| # | Milestone | Status | Notes |
|---|-----------|--------|-------|
| 56 | **System Calls** | ✅ Complete | SYS_WRITE, SYS_EXIT, SYS_READ, SYS_OPEN, SYS_CLOSE, SYS_BRK, SYS_GETPID, SYS_REBOOT. Safe user-space access. |
| 57 | **ELF Loader** | ✅ Complete ⭐ FINALIZED | ELF64 parsing, user-mode transition, `elfload`. Works on first boot. |
| 58 | **Process Foundation** | ✅ Complete | PCB, `process_create`, `proclist`, `vmmclone` |
| 59 | **Process Stack Setup** | ✅ Complete | Static kernel stack pool, user/kernel stacks, `runproc`, `process_destroy` |
| 60 | **Cooperative Scheduler** | ✅ Complete | Ready queue, `process_yield()`, `process_exit()`, `testyield`, `schstat` |
| 61 | **Preemptive Scheduler** | ✅ Complete | PIT timer preemption, quantum slicing, timer-driven kernel-mode preemption |
| 62 | **newlib Userland C Library** | ✅ Complete ⭐ v0.4.7 | Full newlib 4.x linked into user programs. Standard C available in Ring 3. |
| 63 | **Blocking I/O** | ✅ Complete ⭐ v0.4.7 | `sys_read` on fd 0 blocks via BLOCKED + `hlt`, woken by `irq1`. |
| 64 | **File I/O from Ring 3** | ✅ Complete ⭐ v0.4.10 | `open`/`close`/`read`/`write` on FAT files from userland. |
| 65 | **User-Mode Processes** | 🚧 In Progress | Kernel-mode processes work; per-process tty / focus is the remaining piece for multiple concurrent user shells. |
| 66 | **Process Cleanup on Exit** | ✅ Complete ⭐ v0.4.8 | `process_reclaim` frees ELF pages and user stack pages, marks PCB UNUSED, resets pid, decrements count. `process_exit` runs with interrupts disabled. Page-table teardown deferred. |
| 67 | **Expanded User-Shell Regression Harness** | ✅ Complete ⭐ v0.5.0 | Options 4 (FS round-trip), 5 (persistence across reboot), 6 (multi-file), 7 (4 KB round-trip). All pass in dual-drive and single-drive. |
| 68 | **Slab Allocator** | ❌ Not Needed | Free list already provides memory reuse for kmalloc/kfree |

---

## Summary

| Phase | Completed | Total | Progress |
|-------|-----------|-------|----------|
| Boot & System Init | 5 | 5 | **100%** ✅ |
| Core Kernel | 31 | 31 | **100%** ✅ |
| Memory Management | 8 | 8 | **100%** ✅ |
| Storage & File Systems | 5 | 7 | **71%** 🚧 |
| User Space | 11 | 13 | **85%** 🚧 |
| **Overall** | **60** | **64** | **94%** |

The overall number is lower than the earlier 98% because the checklist now counts the storage milestones as separate items. This is more honest: storage was not present in the earlier counts even though the roadmap listed it as "Next." The project is materially closer to complete on the previously-tracked items, and the roadmap is more accurate about what remains.

---

## Recent Milestone Achievements (Chronological Order — Newest First)

### v0.5.0 — Storage Layer Complete ⭐ NEW
- **ATA PIO block device driver** on the primary channel. `ata_init` probes master and slave via IDENTIFY, extracts model strings, and logs to serial. Per-drive entry points.
- **Three ATA bring-up bugs found and fixed:** PIC mask restoration, exception frame offsets, LBA mode bit.
- **FatFs R0.15 vendored** with a `diskio.c` shim. Read and write, kernel-side and userland.
- **Dual-drive storage:** kernel on master `hdd.img`, FAT16 volume on slave `fat.img`. Kernel shell `fatmount`, `fatls`, `fatcat`.
- **Userland file I/O:** per-process file table, `SYS_OPEN` (4), `SYS_CLOSE` (6), `SYS_GETPID` (20), fd branch in `SYS_READ` (3). Newlib `open`/`close`/`read`/`write` work from Ring 3. Verified byte-exact round-trip.
- **Single-drive layout:** `FAT_CONFIG=dual|single` selects. FAT16 partition at LBA 2048. `diskio.c` applies `FAT_PARTITION_OFFSET` (2048 in single, 0 in dual).
- **Key correction:** `FF_MULTI_PARTITION = 0` means FatFs is not partition-aware; the offset belongs in `diskio.c`, not the BPB.
- **Persistence across reboot** verified end-to-end by user shell option 5.
- **`[FS TEST]` fixed:** `msg_len` was hardcoded to 19 but the string is 20 bytes.
- **Kernel-size tripwire** now checks both the stage2 staging ceiling (176 KB) and the disk-layout ceiling (983 KB).
- **Config diagnostic** at boot prints the storage layout to VGA and serial.

### v0.4.11 — Single-Drive Layout
- See v0.5.0.

### v0.4.10 — ATA PIO + FatFs
- See v0.5.0.

### v0.4.9 — Scheduler, TSS, and Interrupt ABI Stability Pass
- **ELF-load race at boot** fixed at all three call sites (`kmain` boot path, `elfload`, `usershell`). Eliminated the intermittent user-mode `#PF` at `CR2 == RIP == 0x8000000000` that reproduced when a key was pressed during the boot-choice window.
- **`TSS.RSP0` / `g_syscall_stack_top` lockstep.** Gate on `entry_point < KERNEL_BASE` removed in `scheduler_switch_to`, `process_exit`, `timer_preempt_handler`.
- **`process_exit` fallback TSS restoration.** Now restores both fields to idle's kernel stack top.
- **Kernel stack slot aliasing.** Replaced `pid % MAX_PROCESSES` with an explicit `slot_owner[]` allocator and a `pcb_t::kernel_stack_slot` field. Survives 20+ `testyield` runs per boot with no fault.
- **`context_switch.asm` offsets.** Every offset at or after `user_stack_phys` updated by +8 to account for the new `kernel_stack_slot` field. `_Static_assert` block in `process.c` pins the offsets.
- **`scheduler_switch_to` preemption window.** `cli` added at the top of the function to close the race between `current_process = next` and `context_switch(prev, next)`.
- **`irq1_stub` ABI violation.** All stubs that call C handlers now preserve all 15 GPRs. `isr8_stub` also fixed a latent missing `add rsp, 8` for the CPU's error code. `process_exit`'s fallback now uses a direct `jmp kmain_shell_loop`.
- **Frame validation in `context_switch.asm`.** `.kernel_task` checks the resume frame's `rip` and `cs` before popping, and emits a serial marker byte (`R` or `C`) and halts on failure.
- **`process_dump_all` cosmetic fix.** Detached PCBs show `DETACHED` instead of `READY`.

### v0.4.8 — Process Cleanup on Exit
- **PCB reclaim.** `process_reclaim(pcb_t*)` in `process.c`, called from `process_exit`. Frees ELF segment pages and user stack pages, sets `PROC_STATE_UNUSED`, resets `pid = 0`, decrements `process_count`. Repeated `testyield` and `runproc` no longer exhaust the 32-slot pool.
- **`process_exit` timer race closed.** Interrupts are disabled for the entire critical section (state transition + queue removal + reclaim + context switch). The timer cannot re-add the exiting process or save the current stack frame into `next->rsp`. Interrupts re-enabled by `iretq`, or by explicit `sti` before the kernel-shell jump in the no-runnable fallback.
- **Page-table teardown deferred.** A few pages per process.
- Verified: 19 consecutive `testyield` runs and 5 `runproc` runs in one boot, only idle in the process table afterward, no fault.

### v0.4.7 — newlib, Blocking I/O, Boot Choice
- **newlib 4.x in userland**: `printf`, `malloc`/`free`, `memcpy`, `str*` work in Ring 3, statically linked against `libc.a`/`libm.a`. Reentrancy initialized via `_impure_ptr = &_impure_data`.
- **Kernel-stack-on-syscall-entry**: syscall path runs on a per-process kernel stack (from A2, tag `20260912I`).
- **Blocking `sys_read`**: reads no longer spin the CPU. Process marks BLOCKED, yields via timer, woken by `irq1`.
- **Boot-time shell choice**: 2-second window, `k` for kernel shell, any other key (or timeout) for user shell.
- **User shell is the terminal console**: kernel shell not reachable after boot.
- **Kernel diagnostics return to the kernel shell** on exit.
- **Userland heap test**: user shell menu option 3 exercises `malloc`/`free` over `sys_brk`.
- **`gdtdump` / `tssdump`** kernel shell commands.
- **`testyield` fix**: `process_yield` no longer corrupts the ready queue.
- **TSS.RSP0 / `g_syscall_stack_top` in lockstep** from boot.

### v0.4.6 — Preemptive & Unlocked Core Milestone
- Preemptive Scheduler 100% complete.
- Multi-pass segment register incrementing integrated into `stage2.asm` (256 sectors / 128 KB kernel size).
- `SYS_REBOOT` (syscall #25) links Ring 3 user shell option 4 to a safe Ring 0 triple-fault restart.
- String alignment layout anomalies inside `kmain.c` fixed for embedded `.userelf` sections.

### v0.4.5 — Cooperative Scheduler
- Ready queue with round-robin scheduling.
- `process_yield()` for voluntary context switching.
- `process_exit()` for clean process termination.
- Assembly-level context switching (`context_switch.asm`).
- **`testyield` command** to test cooperative scheduling.
- **`schstat` command** for scheduler statistics.
- `runproc` now uses the scheduler.
- Known limitation: All processes ran in kernel mode (Ring 0) at this stage.

### v0.4.4 — Process Stack Setup
- Static kernel stack pool for processes (eliminates PMM corruption).
- Process creation with dedicated user and kernel stacks.
- Process execution via direct function call (kernel mode).
- Process cleanup with `process_destroy()` (frees user stack, marks PCB unused).
- **`runproc` command** to create and execute a test process.

### v0.4.3 — ELF Loader Stabilized + Process Foundation
- ELF loader works on **first boot** (no more "run twice" bug).
- Bootloader identity-mapping conflict resolved.
- Safe HHDM-based user-space memory access in syscall handler (`safe_copy_from_user`).
- **Process Foundation:** PCB, `process_create()`, `proclist`, `proccreate`, `vmmclone`.
- **Dynamic HHDM mapping:** `ensure_hhdm_mapped()`.
- **BootInfo validation:** magic number and version checking.

### v0.4.2 — STAR MSR Fix
- Fixed IA32_STAR MSR configuration for SYSCALL/SYSRET.
- User CS = 0x30 → STAR[15:0] = 0x20.
- Enabled clean SYSRET return path.

### v0.4.1 — Syscall Stack Stability
- Unified kernel stack model.
- Correct SYSRET return path.
- Clean user → kernel → shell transitions.
- Verified ELF loader return path.

### v0.4.0 — ELF Loader
- Fully functional ELF64 loader.
- Correct user permissions.
- User stack allocation.
- IRETQ transition.
- `elfload` command.
- "Hello from Userland!" verified.

### v0.3.2 — System Calls
- SYSCALL/SYSRET MSR setup.
- SYS_WRITE + SYS_EXIT.
- Full x86_64 syscall ABI.
- Register preservation.
- `syscall` test command.

### v0.3.1 — NX Bit Support
- PT_NX flag.
- NX handling in VMM.
- `nxtest` command.
- Heap WRITE bit fix.
- Keyboard buffer corruption resolved.

### v0.3.0 — User Mode (Ring 3)
- GDT user segments.
- TSS stack switching.
- IRETQ transition.
- PT_USER mapping.
- `create_user_process()`.

---

## Next Steps (Recommended Order)

1. **`SYS_UNLINK`** — small syscall addition; completes the multi-file test in the user shell (option 6's delete phase).
2. **User Programs from Disk (`sys_exec`)** — add a syscall that opens an ELF file on the FAT volume, loads it, and spawns a process. Stop embedding programs in the kernel image. Enables a real shell with external commands.
3. **Kernel shell on a proper stack** — move `kmain_shell_loop` off the hardcoded `0xFFFFFFFF8008FF00` boot stack, allocate from the kernel stack pool. Companion: IST for `#DF`.
4. **User-Mode Processes (full)** — per-process tty / focus so multiple shells can coexist.
5. **Serial Console Debug Access** — kernel shell over COM1 (the right shape for runtime kernel-shell access; the magic-key-combo approach was tried and abandoned).
6. **Framebuffer Graphics** — Move from VGA text mode to graphics.
7. **File System (VFS)** — VFS layer above FatFs.

---

*Last Updated: September 2026 (v0.5.0)*
