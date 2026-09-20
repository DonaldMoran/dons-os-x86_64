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

## 2. Core Kernel Features (41/41 Complete)

| # | Milestone | Status | Notes |
|---|-----------|--------|-------|
| 6 | **IDT + ISR Stubs** | ✅ Complete | Exception handlers, interrupt gates, error‑code support. All stubs that call C handlers preserve caller-saved registers. |
| 7 | **PIC Remap** | ✅ Complete | IRQ0–IRQ15 mapped to 0x20–0x2F |
| 8 | **PIT Timer** | ✅ Complete | IRQ0 tick counter, scheduling foundation |
| 9 | **Keyboard Driver** | ✅ Complete | IRQ1, scancode set 1, shift/caps, input buffer |
| 10 | **VGA Console Upgrade** | ✅ Complete | Scrolling, cursor control, shell‑ready console |
| 11 | **Kernel Shell** | ✅ Complete | Diagnostic console reached via `k` at boot. Commands: help, clear, info, mem, version, reboot, pmmtest, test, vmmtest, serialtest, heapstat, maptest, testrec, heaptest, heapcheck, heapstress, nxtest, syscall, elfload, proclist, proccreate, vmmclone, runproc, schstat, testyield, usershell, gdtdump, tssdump, selftest, atatest, fatmount, fatls, fatcat |
| 12 | **E820 Memory Map** | ✅ Complete | Memory detection, BootInfo struct passed to kernel |
| 13 | **Physical Memory Manager** | ✅ Complete | Bitmap allocator, page alloc/free, reserved region marking |
| 14 | **Virtual Memory Manager** | ✅ Complete | Recursive paging at PML4[510]. HHDM mapping at PML4[256]. Dynamic page table allocation. User-space page mapping with PT_USER. **NX bit support via PT_NX, with EFER.NXE enabled so the CPU enforces it (v0.5.1).** **Dynamic HHDM mapping via `ensure_hhdm_mapped()`.** **Page table cloning via `vmm_clone_page_table()`.** |
| 15 | **Serial Debug Output** | ✅ Complete | COM1 serial output for kernel debugging alongside VGA |
| 16 | **Kernel Heap Allocator** | ✅ Complete ⭐ v0.5.2 | `kmalloc()`/`kfree()` with free list, `heapstat`/`heaptest`. 1 MB initial heap with automatic expansion up to 24 MB. Rewritten in v0.5.2: `heap_extend` places new blocks at the start of the newly mapped region (the old version lost up to 4 KB per extension). Header padded to 48 bytes so payloads are 16-aligned. `heap_validate()` checks every block-list invariant; `heap_stress()` exercises the extension path (grows the heap to ~4.15 MB, zero leak). `heapcheck` and `heapstress` shell commands. |
| 17 | **User Mode (Ring 3)** | ✅ Complete | GDT with user segments (0x33 code, 0x2B data). TSS configured for stack switching. `iretq` transition. CPL=3 with page protection. |
| 18 | **NX (No Execute) Bit Support** | ✅ Complete | PT_NX flag in `vmm.h` (bit 63). NX handling in `vmm_map_page()`. `nxtest` and `vmmtest` verify. |
| 19 | **System Calls** | ✅ Complete ⭐ v0.5.2 | SYSCALL/SYSRET via MSRs. SYS_WRITE (#1), SYS_EXIT (#2), SYS_READ (#3), SYS_OPEN (#4), SYS_CLOSE (#6), SYS_UNLINK (#7), SYS_BRK (#10), SYS_GETPID (#20), SYS_REBOOT (#25). `syscall` test command. **Safe user-space access via `safe_copy_from_user()`/`safe_copy_to_user()`.** |
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
| 36 | **FatFs Integration** | ✅ Complete ⭐ v0.5.2 | FatFs R0.16 vendored with `diskio.c` shim over the ATA driver. `FF_FS_READONLY = 0`. Dual-drive storage: kernel on master, FAT16 volume on slave. Kernel shell commands `fatmount`, `fatls`, `fatcat`. Long filename support added in v0.5.2. |
| 37 | **Userland File I/O over FatFs** | ✅ Complete ⭐ v0.4.10 | Per-process file table in the PCB. `SYS_OPEN` (4), `SYS_CLOSE` (6), `SYS_GETPID` (20), fd branch in `SYS_READ` (3). Newlib `open`/`close`/`read`/`write` work from Ring 3. Verified create/write/close/reopen/read round-trip byte-exact. |
| 38 | **Single-Drive Layout** | ✅ Complete ⭐ v0.5.0 | `FAT_CONFIG=dual|single` selects the layout. FAT16 partition at LBA 2048. `diskio.c` applies `FAT_PARTITION_OFFSET`. `FF_MULTI_PARTITION = 0` means FatFs is not partition-aware, so the offset lives in `diskio.c`, not the BPB. `hdd-single.img` built by `mkfs.vfat --offset=2048 -h 2048` and populated by `mcopy`. |
| 39 | **Persistence Across Reboot** | ✅ Complete ⭐ v0.5.0 | Verified end-to-end: user shell option 5 writes in one boot, verifies byte-exact after reboot on the same image. Proves writes are durable, not just buffered. |
| 40 | **Config Diagnostic at Boot** | ✅ Complete ⭐ v0.5.0 | `kmain.c` prints a config-specific storage line to VGA and serial (`"Storage: single-drive, FAT@LBA 2048"` vs `"Storage: dual-drive, FAT@LBA 0 on slave"`). Mount failures now appear on VGA, not just serial. |
| 41 | **Self-Test Infrastructure** | ✅ Complete ⭐ v0.5.1 | `selftest` kernel shell command runs 15 tests and prints a pass/fail summary. Coverage: GDT descriptor contents, TSS fields, PMM allocation/free, VMM control registers, page mapping, recursive paging, heap integrity, NX bit in final PTE, syscall entry point, ATA reads, FatFs mount/directory listing, and the three exception handlers (#DE, #PF, #GP). Expected-fault protocol lets the exception tests recover cleanly. |
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

---

## 3. Memory Management (8/8 Complete)

| # | Milestone | Status | Notes |
|---|-----------|--------|-------|
| 52 | **Higher‑Half Kernel** | ✅ Complete | Kernel mapped to `0xFFFFFFFF80100000`, identity map preserved |
| 53 | **Virtual Memory Manager** | ✅ Complete | Recursive paging, HHDM, dynamic page tables, NX, dynamic HHDM mapping, page table cloning |
| 54 | **Serial Debug Output** | ✅ Complete | COM1 serial output, integrated with QEMU |
| 55 | **Kernel Heap** | ✅ Complete ⭐ v0.5.2 | `kmalloc()`/`kfree()` with free list. Memory reuse verified. Rewritten in v0.5.2 with `heap_validate()` and `heap_stress()`. |
| 56 | **User Memory Mapping** | ✅ Complete | Pages mapped with PT_USER for user/kernel isolation |
| 57 | **NX (No Execute) Bit** | ✅ Complete | PT_NX flag, `nxtest`, NX status in `vmmtest`. Since v0.5.1, `EFER.NXE` is enabled so the CPU actually enforces it. |
| 58 | **HHDM Dynamic Mapping** | ✅ Complete | `ensure_hhdm_mapped()` for on-demand physical memory access. Used by ELF loader and page table cloning. |
| 59 | **`sys_brk` Heap Growth** | ✅ Complete ⭐ v0.4.7 | Per-process heap state in `current->brk_virt`. Pages mapped with `invlpg` after map. |

---

## 4. Storage & File Systems (6/7 Complete)

| # | Milestone | Status | Notes |
|---|-----------|--------|-------|
| 60 | **ATA PIO Driver** | ✅ Complete ⭐ v0.4.10 | Primary channel, LBA28, polled. Per-drive entry points, FLUSH CACHE. Write-protect floor on master. `atatest` diagnostic. |
| 61 | **FatFs Integration** | ✅ Complete ⭐ v0.5.2 | FatFs R0.16, read and write, long filename support. Kernel shell and userland access. `ff.o` and `ffunicode.o` compiled with the cross-GCC. |
| 62 | **Single-Drive Layout** | ✅ Complete ⭐ v0.5.0 | Boot chain + kernel + FAT16 partition on one `hdd.img`. FAT at LBA 2048. |
| 63 | **Persistence Across Reboot** | ✅ Complete ⭐ v0.5.0 | User-shell option 5 proves writes survive reboot. |
| 64 | **Dual-Drive Layout (Retained)** | ✅ Complete | Kernel on master, FAT16 on slave. Remains the default; useful for debugging. |
| 65 | **`SYS_UNLINK`** | ✅ Complete ⭐ v0.5.2 | Syscall #7, wraps FatFs `f_unlink`. Kernel handler `sys_unlink` in `user_syscall.c`; userland `unlink()` shim in `arc2/syscalls.c`. The user shell's multi-file test (option 6) now deletes the files it creates and confirms they are gone. |
| 66 | **User Programs from Disk** | ☐ Not Started | `sys_exec`-style syscall, load ELF files from the FAT volume. Enables external commands in a real shell. |

---

## 5. User Space & Advanced Features (11/14 Complete)

| # | Milestone | Status | Notes |
|---|-----------|--------|-------|
| 67 | **System Calls** | ✅ Complete ⭐ v0.5.2 | SYS_WRITE, SYS_EXIT, SYS_READ, SYS_OPEN, SYS_CLOSE, SYS_UNLINK, SYS_BRK, SYS_GETPID, SYS_REBOOT. Safe user-space access. |
| 68 | **ELF Loader** | ✅ Complete ⭐ FINALIZED | ELF64 parsing, user-mode transition, `elfload`. Works on first boot. |
| 69 | **Process Foundation** | ✅ Complete | PCB, `process_create`, `proclist`, `vmmclone` |
| 70 | **Process Stack Setup** | ✅ Complete | Static kernel stack pool, user/kernel stacks, `runproc`, `process_destroy` |
| 71 | **Cooperative Scheduler** | ✅ Complete | Ready queue, `process_yield()`, `process_exit()`, `testyield`, `schstat` |
| 72 | **Preemptive Scheduler** | ✅ Complete | PIT timer preemption, quantum slicing, timer-driven kernel-mode preemption |
| 73 | **newlib Userland C Library** | ✅ Complete ⭐ v0.4.7 | Full newlib 4.x linked into user programs. Standard C available in Ring 3. |
| 74 | **Blocking I/O** | ✅ Complete ⭐ v0.4.7 | `sys_read` on fd 0 blocks via BLOCKED + `hlt`, woken by `irq1`. |
| 75 | **File I/O from Ring 3** | ✅ Complete ⭐ v0.5.2 | `open`/`close`/`read`/`write`/`unlink` on FAT files from userland, with long filename support. |
| 76 | **User-Mode Processes** | 🚧 In Progress | Kernel-mode processes work; per-process tty / focus is the remaining piece for multiple concurrent user shells. |
| 77 | **Process Cleanup on Exit** | ✅ Complete ⭐ v0.4.8 | `process_reclaim` frees ELF pages and user stack pages, marks PCB UNUSED, resets pid, decrements count. `process_exit` runs with interrupts disabled. Page-table teardown deferred. |
| 78 | **Expanded User-Shell Regression Harness** | ✅ Complete ⭐ v0.5.2 | Options 4 (FS round-trip), 5 (persistence across reboot), 6 (multi-file with delete phase), 7 (4 KB round-trip). All pass in dual-drive and single-drive. |
| 79 | **Slab Allocator** | ❌ Not Needed | Free list already provides memory reuse for kmalloc/kfree |

---

## Summary

| Phase | Completed | Total | Progress |
|-------|-----------|-------|----------|
| Boot & System Init | 5 | 5 | **100%** ✅ |
| Core Kernel | 41 | 41 | **100%** ✅ |
| Memory Management | 8 | 8 | **100%** ✅ |
| Storage & File Systems | 6 | 7 | **86%** 🚧 |
| User Space | 11 | 13 | **85%** 🚧 |
| **Overall** | **71** | **74** | **96%** |

The overall count grows as the checklist accounts for new milestones. Storage is at 86% (only `sys_exec` remains), and User Space at 85% (the remaining items are per-process tty, and one more). The roadmap is the authoritative list of what remains.

The Core Kernel count rose from 38 to 41 with the v0.5.2 additions (items 47–51).

---

## Recent Milestone Achievements (Chronological Order — Newest First)

### v0.5.2 — Heap Rewrite, LFN, SYS_UNLINK, Cross-GCC for FatFs, Makefile Dependency Tracking ⭐ NEW
- **heap.c rewritten.** `heap_extend` now returns the start of the newly mapped region and the caller places a single free block covering the entire region. The old version placed the new block at `heap_brk - requested_size`, losing up to `PAGE_SIZE - 1` bytes per extension and putting blocks at the wrong end of the mapped range. Blocks are maintained in address order and coalesced on free.
- **`heap_header_t` padded to 48 bytes.** Payloads are 16-aligned when the allocation size is a multiple of `HEAP_ALIGNMENT`. newlib's `malloc` is documented to return 16-aligned pointers on x86-64; the 40-byte header made payload addresses alternate between 8- and 16-aligned.
- **`heap_validate()` and `heap_stress()`.** `heap_validate` walks the block list and checks every invariant, including that the last block's end equals `heap_brk`. `heap_stress` runs 4096 ops across 512 slots with per-block patterns, grows the heap from 1 MB to ~4.15 MB (multiple extensions), and validates intact with zero leak.
- **`heapcheck` and `heapstress` shell commands.** Both added to `selftest`. Test count 17/17.
- **Long filename support.** `FF_USE_LFN = 2`, `FF_LFN_UNICODE = 2`, `FF_CODE_PAGE = 437`. `fatfs/ffunicode.o` linked into the kernel. Kernel grows from 158 KB to 163 KB, well under the 448 KB staging limit. `fatls` shows `HELLO-WORLD.TXT`; `fatcat HELLO-WORLD.TXT` reads it.
- **`SYS_UNLINK` (syscall #7).** Wraps FatFs `f_unlink`. Kernel handler `sys_unlink` in `user_syscall.c`; userland `unlink()` shim in `arc2/syscalls.c`. `test_multi_file` in the user shell now deletes the files it creates and verifies they are gone.
- **`sys_open` path truncation fix.** Was copying only 127 bytes of the path into a 300-byte buffer. Replaced with a `copy_user_string` helper that respects the buffer size. Paths up to `FF_MAX_LFN` (255) now fit. New `USER_PATH_MAX` constant (300).
- **Cross-GCC for FatFs.** `fatfs/ff.o` and `fatfs/ffunicode.o` compiled with `/opt/cross/bin/x86_64-elf-gcc` at `-O2`. Clang 22.1.8 miscompiles `ff.c` at every optimization level: `-O0` breaks the FILINFO read path (filenames decode as `@80(`, `f_opendir` returns `FR_INT_ERR`), `-O1` hangs in `f_unlink`, `-O2` produces LLD-truncated instructions in the linked binary. GCC produces correct code and links cleanly with the Clang-built kernel objects. See `LLD_BUG_REPORT.md`.
- **Makefile header dependency tracking.** `-MMD -MP` added to `CFLAGS`; `-include $(OBJS:.o=.d)` at the bottom. Header changes now trigger the right rebuilds automatically.

### v0.5.1 — Maintenance Batch: Self-Test, NX on Hardware, Kernel-Owned GDT
- **Self-test infrastructure (`selftest` command).** 15 tests covering GDT descriptor contents, TSS fields, PMM allocation and freeing, VMM control registers, page mapping, recursive paging, heap integrity, the NX bit in the final PTE, syscall entry point, ATA reads, FatFs mount and directory listing, and the three exception handlers (#DE, #PF, #GP). The exception tests use an expected-fault protocol: a small kernel-mode child takes the fault, the handler records the vector, terminates the child, and resumes the kernel shell.
- **`EFER.NXE` enabled.** Previously the kernel wrote NX bits into PTEs but the CPU ignored them because `EFER.NXE` was clear. Worked under KVM by accident (KVM's shadow MMU enables NX on the host side regardless of the guest's EFER); would have faulted on bare metal. `enable_nx()` in `kmain.c` now sets the bit, guarded by a CPUID check. `test_vmm` asserts on it.
- **Kernel-owned GDT.** The GDT lived in low memory (base `0x101DC`, inside `stage2.asm`'s loaded image). `gdt_init` now builds `kernel_gdt[16]` in `.bss` at a higher-half address, `lgdt`s it, and reloads the segment registers.
- **Print atomicity.** Kernel serial and VGA drivers now share a single print lock.
- **#DF through IST1.** A dedicated 4 KB stack and an IST entry on the `#DF` gate turn a double fault into a printed diagnostic.
- **Kernel shell is process-backed.** It runs as a real process (`kshell`) with a pool-allocated kernel stack.
- **Staging ceiling raised** from 176 KB to 448 KB by adding PASS 4–7 in `stage2.asm`.
- **Reboot flushes file handles.** The user shell's reboot path flushes open file handles before the hardware reset; option 9 added.

### v0.5.0 — Storage Layer Complete
- **ATA PIO block device driver** on the primary channel. `ata_init` probes master and slave via IDENTIFY, extracts model strings, and logs to serial. Per-drive entry points.
- **Three ATA bring-up bugs found and fixed:** PIC mask restoration, exception frame offsets, LBA mode bit.
- **FatFs R0.16 vendored** with a `diskio.c` shim. Read and write, kernel-side and userland.
- **Dual-drive storage:** kernel on master `hdd.img`, FAT16 volume on slave `fat.img`. Kernel shell `fatmount`, `fatls`, `fatcat`.
- **Userland file I/O:** per-process file table, `SYS_OPEN` (4), `SYS_CLOSE` (6), `SYS_GETPID` (20), fd branch in `SYS_READ` (3).
- **Single-drive layout:** `FAT_CONFIG=dual|single` selects. FAT16 partition at LBA 2048.
- **Key correction:** `FF_MULTI_PARTITION = 0` means FatFs is not partition-aware; the offset belongs in `diskio.c`, not the BPB.
- **Persistence across reboot** verified end-to-end by user shell option 5.

### v0.4.9 — Scheduler, TSS, and Interrupt ABI Stability Pass
- **ELF-load race at boot** fixed at all three call sites.
- **`TSS.RSP0` / `g_syscall_stack_top` lockstep.** Gate on `entry_point < KERNEL_BASE` removed.
- **`process_exit` fallback TSS restoration.**
- **Kernel stack slot aliasing.** Replaced `pid % MAX_PROCESSES` with an explicit `slot_owner[]` allocator and a `pcb_t::kernel_stack_slot` field.
- **`context_switch.asm` offsets.** Every offset at or after `user_stack_phys` updated by +8.
- **`scheduler_switch_to` preemption window.** `cli` added at the top.
- **`irq1_stub` ABI violation.** All stubs that call C handlers now preserve all 15 GPRs.
- **Frame validation in `context_switch.asm`.**

### v0.4.8 — Process Cleanup on Exit
- **PCB reclaim.** `process_reclaim(pcb_t*)` in `process.c`, called from `process_exit`.
- **`process_exit` timer race closed.**
- **Page-table teardown deferred.**

### v0.4.7 — newlib, Blocking I/O, Boot Choice
- **newlib 4.x in userland.**
- **Kernel-stack-on-syscall-entry.**
- **Blocking `sys_read`.**
- **Boot-time shell choice.**
- **User shell is the terminal console.**
- **Kernel diagnostics return to the kernel shell** on exit.
- **Userland heap test**: user shell menu option 3.
- **`gdtdump` / `tssdump`** kernel shell commands.
- **`testyield` fix.**
- **TSS.RSP0 / `g_syscall_stack_top` in lockstep** from boot.

### v0.4.6 — Preemptive & Unlocked Core Milestone
- Preemptive Scheduler 100% complete.
- Multi-pass segment register incrementing integrated into `stage2.asm`.
- `SYS_REBOOT` (syscall #25) links Ring 3 user shell option 4 to a safe Ring 0 triple-fault restart.

### v0.4.5 — Cooperative Scheduler
- Ready queue with round-robin scheduling.
- `process_yield()` for voluntary context switching.
- `process_exit()` for clean process termination.
- Assembly-level context switching (`context_switch.asm`).
- **`testyield` command**, **`schstat` command**.

### v0.4.4 — Process Stack Setup
- Static kernel stack pool for processes.
- Process creation with dedicated user and kernel stacks.
- Process cleanup with `process_destroy()`.
- **`runproc` command.**

### v0.4.3 — ELF Loader Stabilized + Process Foundation
- ELF loader works on **first boot**.
- Bootloader identity-mapping conflict resolved.
- Safe HHDM-based user-space memory access.
- **Process Foundation:** PCB, `process_create()`, `proclist`, `proccreate`, `vmmclone`.
- **Dynamic HHDM mapping.**

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

1. **User Programs from Disk (`sys_exec`)** — the next feature milestone. Add a syscall that opens an ELF file on the FAT volume, loads it, and spawns a process. Stop embedding programs in the kernel image. Enables a real shell with external commands (`ls`, `cat`, etc.).
2. **ELF Loader `PT_NX` Follow-up** — with `EFER.NXE` now enabled, `elf_load_into_process` can mark non-executable segments (data, BSS, user stack) with `PT_NX`. About an hour of work; closes the follow-up noted in `MAINTENANCE.md` §3g.
3. **User-Mode Processes (full)** — per-process tty / focus so multiple shells can coexist.
4. **Serial Console Debug Access** — kernel shell over COM1 (the right shape for runtime kernel-shell access; the magic-key-combo approach was tried and abandoned).
5. **Framebuffer Graphics** — Move from VGA text mode to graphics.
6. **File System (VFS)** — VFS layer above FatFs.

---

*Last Updated: September 2026 (v0.5.2)*
