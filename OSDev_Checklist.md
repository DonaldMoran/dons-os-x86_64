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

## 2. Core Kernel Features (26/26 Complete)

| # | Milestone | Status | Notes |
|---|-----------|--------|-------|
| 6 | **IDT + ISR Stubs** | ✅ Complete | Exception handlers, interrupt gates, error‑code support |
| 7 | **PIC Remap** | ✅ Complete | IRQ0–IRQ15 mapped to 0x20–0x2F |
| 8 | **PIT Timer** | ✅ Complete | IRQ0 tick counter, scheduling foundation |
| 9 | **Keyboard Driver** | ✅ Complete | IRQ1, scancode set 1, shift/caps, input buffer |
| 10 | **VGA Console Upgrade** | ✅ Complete | Scrolling, cursor control, shell‑ready console |
| 11 | **Kernel Shell** | ✅ Complete | Diagnostic console reached via `k` at boot. Commands: help, clear, info, mem, version, reboot, pmmtest, test, vmmtest, serialtest, heapstat, maptest, testrec, heaptest, nxtest, syscall, elfload, proclist, proccreate, vmmclone, runproc, schstat, testyield, usershell, gdtdump, tssdump |
| 12 | **E820 Memory Map** | ✅ Complete | Memory detection, BootInfo struct passed to kernel |
| 13 | **Physical Memory Manager** | ✅ Complete | Bitmap allocator, page alloc/free, reserved region marking |
| 14 | **Virtual Memory Manager** | ✅ Complete | Recursive paging at PML4[510]. HHDM mapping at PML4[256]. Dynamic page table allocation. User-space page mapping with PT_USER. **NX bit support via PT_NX.** **Dynamic HHDM mapping via `ensure_hhdm_mapped()`.** **Page table cloning via `vmm_clone_page_table()`.** |
| 15 | **Serial Debug Output** | ✅ Complete | COM1 serial output for kernel debugging alongside VGA |
| 16 | **Kernel Heap Allocator** | ✅ Complete | `kmalloc()`/`kfree()` with free list, `heapstat`/`heaptest`. 64MB initial heap with automatic expansion. |
| 17 | **User Mode (Ring 3)** | ✅ Complete | GDT with user segments (0x33 code, 0x2B data). TSS configured for stack switching. `iretq` transition. CPL=3 with page protection. |
| 18 | **NX (No Execute) Bit Support** | ✅ Complete | PT_NX flag in `vmm.h` (bit 63). NX handling in `vmm_map_page()`. `nxtest` and `vmmtest` verify. |
| 19 | **System Calls** | ✅ Complete | SYSCALL/SYSRET via MSRs. SYS_WRITE (#1), SYS_EXIT (#2), SYS_READ (#3), SYS_BRK (#10). `syscall` test command. **Safe user-space access via `safe_copy_from_user()`/`safe_copy_to_user()`.** |
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

---

## 3. Memory Management (7/7 Complete)

| # | Milestone | Status | Notes |
|---|-----------|--------|-------|
| 34 | **Higher‑Half Kernel** | ✅ Complete | Kernel mapped to `0xFFFFFFFF80100000`, identity map preserved |
| 35 | **Virtual Memory Manager** | ✅ Complete | Recursive paging, HHDM, dynamic page tables, NX, dynamic HHDM mapping, page table cloning |
| 36 | **Serial Debug Output** | ✅ Complete | COM1 serial output, integrated with QEMU |
| 37 | **Kernel Heap** | ✅ Complete | `kmalloc()`/`kfree()` with free list. Memory reuse verified. |
| 38 | **User Memory Mapping** | ✅ Complete | Pages mapped with PT_USER for user/kernel isolation |
| 39 | **NX (No Execute) Bit** | ✅ Complete | PT_NX flag, `nxtest`, NX status in `vmmtest` |
| 40 | **HHDM Dynamic Mapping** | ✅ Complete | `ensure_hhdm_mapped()` for on-demand physical memory access. Used by ELF loader and page table cloning. |
| 41 | **`sys_brk` Heap Growth** | ✅ Complete ⭐ v0.4.7 | Per-process heap state in `current->brk_virt`. Pages mapped with `invlpg` after map. |

---

## 4. User Space & Advanced Features (10/13 Complete)

| # | Milestone | Status | Notes |
|---|-----------|--------|-------|
| 42 | **System Calls** | ✅ Complete | SYS_WRITE, SYS_EXIT, SYS_READ, SYS_BRK, SYS_REBOOT. Safe user-space access. |
| 43 | **ELF Loader** | ✅ Complete ⭐ FINALIZED | ELF64 parsing, user-mode transition, `elfload`. Works on first boot. |
| 44 | **Process Foundation** | ✅ Complete | PCB, `process_create`, `proclist`, `vmmclone` |
| 45 | **Process Stack Setup** | ✅ Complete | Static kernel stack pool, user/kernel stacks, `runproc`, `process_destroy` |
| 46 | **Cooperative Scheduler** | ✅ Complete | Ready queue, `process_yield()`, `process_exit()`, `testyield`, `schstat` |
| 47 | **Preemptive Scheduler** | ✅ Complete | PIT timer preemption, quantum slicing, timer-driven kernel-mode preemption |
| 48 | **newlib Userland C Library** | ✅ Complete ⭐ v0.4.7 | Full newlib 4.x linked into user programs. Standard C available in Ring 3. |
| 49 | **Blocking I/O** | ✅ Complete ⭐ v0.4.7 | `sys_read` on fd 0 blocks via BLOCKED + `hlt`, woken by `irq1`. |
| 50 | **User-Mode Processes** | 🚧 In Progress | ... |
| 51 | **Process Cleanup on Exit** | ✅ Complete ⭐ v0.4.8 | `process_reclaim` frees ELF pages and user stack pages, marks PCB UNUSED, resets pid, decrements count. `process_exit` runs with interrupts disabled. Page-table teardown deferred. |
| 52 | **Permanent Storage Layer** | ☐ Not Started | ATA PIO block device + FatFs integration. Follows cleanup. |
| 53 | **Slab Allocator** | ❌ Not Needed | Free list already provides memory reuse for kmalloc/kfree |

---

## Summary

| Phase | Completed | Total | Progress |
|-------|-----------|-------|----------|
| Boot & System Init | 5 | 5 | **100%** ✅ |
| Core Kernel | 26 | 26 | **100%** ✅ |
| Memory Management | 8 | 8 | **100%** ✅ |
| User Space | 11 | 12 | **92%** 🚧 |
| **Overall** | **50** | **52** | **96%** |

---

## Recent Milestone Achievements (Chronological Order — Newest First)

### v0.4.8 — Process Cleanup on Exit ⭐ NEW
- **PCB reclaim.** `process_reclaim(pcb_t*)` in `process.c`, called from `process_exit`. Frees ELF segment pages and user stack pages, sets `PROC_STATE_UNUSED`, resets `pid = 0`, decrements `process_count`. Repeated `testyield` and `runproc` no longer exhaust the 32-slot pool.
- **`process_exit` timer race closed.** Interrupts are disabled for the entire critical section (state transition + queue removal + reclaim + context switch). The timer cannot re-add the exiting process or save the current stack frame into `next->rsp`. Interrupts re-enabled by `iretq`, or by explicit `sti` before the kernel-shell jump in the no-runnable fallback.
- **Page-table teardown deferred.** A few pages per process.
- Verified: 19 consecutive `testyield` runs and 5 `runproc` runs in one boot, only idle in the process table afterward, no fault.

### v0.4.7 — newlib, Blocking I/O, Boot Choice ⭐ NEW
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

1. **Permanent Storage Layer** — ATA PIO block device driver (read/write sectors from long mode), then FatFs integration (FAT12/FAT16/FAT32).
2. **User-Mode Processes (full)** — per-process tty / focus so multiple shells can coexist.
3. **Serial Console Debug Access** — kernel shell over COM1 (the right shape for runtime kernel-shell access; the magic-key-combo approach was tried and abandoned).
4. **Framebuffer Graphics** — Move from VGA text mode to graphics.
5. **File System (VFS)** — VFS layer above FatFs.

---

*Last Updated: September 2026*
