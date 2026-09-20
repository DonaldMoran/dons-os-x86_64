# ROADMAP  
### dons‑os (x86_64) — Project Roadmap

This roadmap outlines the evolution of **dons‑os**, from the earliest boot stages to a functional interrupt‑driven kernel and beyond.

---

## 1. Boot Chain (Completed)

### ✔ 1.1 — 16‑bit Real Mode
- BIOS boot sector (`0x7C00`)
- INT 0x10 text output
- INT 0x13 disk loading
- Stage2 loader

### ✔ 1.2 — 32‑bit Protected Mode
- A20 enable
- GDT setup
- CR0.PE → protected mode
- VGA text output

### ✔ 1.3 — 64‑bit Long Mode
- PAE paging (PML4 → PDPT → PD → PT)
- IA32_EFER.LME set
- CR0.PG → paging enabled
- Far jump into 64‑bit code
- 64‑bit VGA text output

Boot chain is complete and stable.

---

## 2. Kernel Foundations (Completed)

### ✔ 2.1 — 64‑bit IDT + Exceptions
- IDT structure in long mode  
- ISR stubs in assembly  
- Basic exception handlers (Divide, Debug)

### ✔ 2.2 — PIC Remap
- Master → 0x20  
- Slave → 0x28  
- IRQs mapped to vectors 32–47

### ✔ 2.3 — PIT Timer (IRQ0)
- PIT programmed to 100 Hz  
- Global tick counter  
- On‑screen tick display

### ✔ 2.4 — Keyboard IRQ1
- IRQ1 handler  
- Raw scancode reader  
- Verified interrupt flow

### ✔ 2.5 — Physical Memory Manager (PMM)
- Parse E820 map  
- Page allocator (bitmap-based)  
- Page allocation test  
- Supports up to 512 MiB

### ✔ 2.6 — VGA Console with Scrolling
- Correct scrolling behavior  
- Hardware cursor control  
- Cursor shape control (block/underline)  
- Clean boot screen with system info

### ✔ 2.7 — Command Shell
- Command parser
- Built‑in commands: help, clear, info, mem, version, reboot, pmmtest, test, vmmtest, serialtest, heapstat, maptest, testrec, heaptest, nxtest, syscall, **elfload**, **proclist**, **proccreate**, **vmmclone**, **runproc**, **schstat**, **testyield**, **usershell**, **gdtdump**, **tssdump**, **atatest**, **fatmount**, **fatls**, **fatcat**, **selftest**
- Command history with backspace
- Interactive prompt `>`

---

## 3. Core Kernel Features (Completed)

### ✔ 3.1 — Higher‑Half Kernel
- Map kernel to `0xFFFFFFFF80100000`  
- Updated linker script  
- Clean identity map and higher-half mapping working

### ✔ 3.2 — Exception Handlers
- Page Fault handler (dump CR2, error code, RIP)
- Divide by Zero handler
- General Protection Fault handler (ERR, RIP, CS)
- Double Fault handler registered
- Test command (`test`) for triggering all exceptions

### ✔ 3.3 — Virtual Memory Manager (VMM)
- CR3/PML4 read and displayed at boot
- HHDM_START defined and printed
- vmmtest command implemented
- Serial debug output (COM1)
- **Recursive paging** at PML4[510]
- PML4 read/write from higher-half kernel
- Dynamic page table allocation (PDPT, PD, PT)
- HHDM mapping (PML4[256])
- User-space page mapping with PT_USER flag
- **NX (No Execute) bit support** via PT_NX flag
- `nxtest` command for verifying NX functionality
- **Dynamic HHDM mapping** via `ensure_hhdm_mapped()`
- **Page table cloning** via `vmm_clone_page_table()`
- **`vmmclone` command** for testing process isolation

### ✔ 3.4 — Kernel Heap Allocator
- `kmalloc()` bump allocator
- `kfree()` free list (memory reuse)
- Automatic heap expansion
- `heapstat` debugging command
- `heaptest` allocation/reuse verification
- 64MB initial heap size

### ✔ 3.5 — User Mode (Ring 3)
- GDT with user segments (DPL=3)
- User code (0x33) and user data (0x2B) segments
- TSS initialization for stack switching
- `iretq` transition from kernel to user mode
- User memory mapped with PT_USER flag
- `create_user_process()` for launching user code

### ✔ 3.6 — NX Bit Support
- NX bit enabled via PT_NX flag
- NX flag handling in `vmm_map_page()`
- `nxtest` command for verifying NX functionality
- WRITE bit fix for heap pages
- Keyboard buffer corruption resolved

### ✔ 3.7 — System Calls
- `syscall` instruction setup via MSRs (IA32_STAR, IA32_LSTAR, IA32_FMASK)
- System call handler with register preservation
- SYS_WRITE (syscall #1)
- SYS_EXIT (syscall #2)
- SYS_READ (syscall #3)
- SYS_OPEN (syscall #4)
- SYS_CLOSE (syscall #6)
- SYS_BRK (syscall #10)
- SYS_REBOOT (syscall #25)
- Syscall dispatcher with x86_64 ABI
- `syscall` test command
- SYSRET returns to user mode
- **Safe user‑space memory access** via `safe_copy_from_user()` and `safe_copy_to_user()` using HHDM

### ✔ 3.8 — ELF Loader ⭐ FINALIZED
- Parses ELF64 headers and program headers
- Maps LOAD segments with correct permissions (Read, Write, Execute, User)
- Allocates and maps user stack pages
- Transitions to user mode via IRETQ (CS=0x33, SS=0x2B)
- Sets IOPL=3 for user I/O access
- Page table execute permissions at all levels
- **`elfload` command** for running embedded ELF programs
- **Works reliably on first boot** (handles bootloader identity‑mapping conflict)
- **Safe HHDM‑based copying** of program segments and stack
- Tested with "Hello from Userland!" via serial

### ✔ 3.9 — Process Foundation ⭐ COMPLETE
- Process Control Block (PCB) structure
- Process creation (`process_create`)
- Process listing (`proclist`)
- Page table cloning (`vmm_clone_page_table`)
- **`vmmclone` command** for testing page table isolation
- Ready queue infrastructure (foundation for scheduler)

### ✔ 3.10 — Process Stack Setup
- Static kernel stack pool for processes
- Process creation with dedicated user and kernel stacks
- Process execution via direct function call (kernel mode)
- Process cleanup with `process_destroy()` (frees user stack, marks PCB unused)
- **`runproc` command** to create and execute a test process
- Shell returns properly after process execution
- All previous features remain fully functional

### ✔ 3.11 — Cooperative Scheduler
- Ready queue with round‑robin scheduling
- `process_yield()` for voluntary context switching
- `process_exit()` for clean process termination
- Assembly‑level context switching (`context_switch.asm`)
- **`testyield` command** for testing cooperative scheduling
- **`schstat` command** for scheduler statistics
- `runproc` now uses the scheduler
- All previous features remain fully functional

### ✔ 3.12 — Preemptive Scheduler
- PIT timer (IRQ0, 100 Hz) preempts user-mode and kernel-mode processes
- Quantum-based task slicing (`SCHED_QUANTUM = 2`)
- Timer saves the interrupted frame into the PCB and resumes via `irq0_stub`
- Kernel-mode preemption: a process blocked in `sys_read` does not hold the CPU
- **`testyield` now alternates cleanly** between both test processes

### ✔ 3.13 — Userland C Library (newlib 4.x)
- newlib 4.x statically linked into user programs (`libc.a`, `libm.a`)
- `crt0.S`, `syscalls.c`, `reent.c` provide startup, syscall shims, and reentrancy
- **`_impure_ptr = &_impure_data`** in `reent.c` — without this, the first `printf` faults
- `printf`, `malloc`/`free`, `memcpy`, `str*`, `setvbuf`, `errno` all work from Ring 3
- User programs written in ordinary C, compiled with `x86_64-elf-gcc`
- Loaded as ELF64 binaries embedded in the kernel image

### ✔ 3.14 — Blocking I/O
- `sys_read` on fd 0 no longer spins the CPU
- Blocked process sets `PROC_STATE_BLOCKED`, executes `sti; hlt`
- Timer skips blocked processes (narrowed re-add condition to `state == RUNNING`)
- `irq1_handler` wakes all blocked processes on a key
- Timer resumes the process via its saved kernel frame
- Multiple processes can be blocked; whichever wakes first consumes the byte

### ✔ 3.15 — Boot-Time Shell Choice
- 2-second window at boot: `k` → kernel shell, any other key or timeout → user shell
- Kernel shell is a diagnostic console (not reachable after user shell starts)
- User shell is the default interactive console
- `sys_exit` on a user process halts the CPU; kernel diagnostics return to the kernel shell
- `gdtdump` and `tssdump` added as on-demand kernel shell diagnostics

### ✔ 3.16 — Process Yield Queue Fix
- `process_yield` no longer corrupts the ready queue by calling
  `scheduler_ready_queue_remove(current_process)` on an off-queue process
- Defensive check added to `scheduler_ready_queue_remove` for off-queue processes
- `testyield` demonstrably alternates between both test processes

### ✔ 3.17 — TSS.RSP0 / `g_syscall_stack_top` Sync
- `process_init` runs after `tss_init` in `kmain`
- `process_init` calls `tss_set_kernel_stack(idle->kernel_stack_top)` alongside `tss_set_syscall_stack`
- `tssdump` on a fresh boot shows `rsp0` and `g_syscall_stack_top` as the same address

### ✔ 3.18 — Process Cleanup on Exit
- `process_reclaim(pcb_t*)` in `process.c`, called from `process_exit`
- Frees ELF segment pages and user stack pages via `process_cleanup_elf_pages`
- Sets PCB `PROC_STATE_UNUSED`, resets `pid = 0`, decrements `process_count`
- PCB slot can be reused by a future `process_create`; repeated `testyield` and `runproc` no longer exhaust the 32-slot pool
- `process_exit` now disables interrupts for the entire critical section (state transition + queue removal + reclaim + context switch), closing a timer race where the timer could re-add the exiting process to the ready queue or save the current stack frame into `next->rsp`
- Page-table teardown deferred (a few pages per process)

### ✔ 3.19 — Scheduler, TSS, and Interrupt ABI Stability Pass ⭐ v0.4.9
- **ELF-load race at boot.** `process_create` queues the PCB before the ELF is loaded, so a PIT tick between create and `elf_load_into_process` could schedule a process whose entry page was not yet mapped. Fixed at the three call sites (`kmain` boot path, `elfload`, `usershell`): remove from queue, load ELF, set `entry_point`, add back.
- **TSS.RSP0 / `g_syscall_stack_top` lockstep.** The update was gated on `entry_point < KERNEL_BASE`; kernel-mode switches left `g_syscall_stack_top` stale. Gate removed in `scheduler_switch_to`, `process_exit`, `timer_preempt_handler`.
- **`process_exit` fallback TSS restoration.** The no-runnable fallback zeroed only `g_syscall_stack_top`, leaving `TSS.RSP0` pointing at a dead process's stack. Now restores both to idle's kernel stack top.
- **Kernel stack slot aliasing.** Kernel stack slots were computed as `pid % MAX_PROCESSES`. `next_pid` is monotonic and never decremented on teardown, so after 32+ creations the modulo wrapped and a new process aliased a live one's slot. Replaced with `slot_owner[]` and a `pcb_t::kernel_stack_slot` field.
- **`context_switch.asm` offsets.** Inserting `kernel_stack_slot` at `pcb_t` offset `0x58` shifted later fields by 8 bytes; the asm hardcoded the old offsets. Every offset updated by +8, and a `_Static_assert` block in `process.c` now pins the offsets the asm depends on.
- **`scheduler_switch_to` preemption window.** The function set `current_process = next` with interrupts enabled, then called `context_switch(prev, next)`. A PIT tick in that window saved the wrong stack frame into `next`. Fixed with `cli` at the top of the function.
- **`irq1_stub` ABI violation.** `irq1_handler` was called without preserving caller-saved registers. A keyboard interrupt between `sti` and a subsequent indirect `jmp *%rax` corrupted `%rax` and landed mid-instruction inside `kmain_shell_loop`. Fixed on two fronts: all stubs that call C handlers now preserve all 15 GPRs, and the fallback uses a direct `jmp kmain_shell_loop`.
- **`process_dump_all` cosmetic fix.** Detached PCBs show `DETACHED` instead of `READY`.
- **Frame validation in `context_switch.asm`.** `.kernel_task` validates the resume frame's `rip` and `cs` before popping, and emits a serial marker byte on failure.

### ✔ 3.20 — ATA PIO Block Device Driver
- Primary-channel ATA in PIO (polled) mode, LBA28
- `ata_init()` probes master and slave, IDENTIFY, model strings
- Per-drive read/write entry points (`ata_read_sectors_drive`, `ata_write_sectors_drive`, etc.)
- `ata_flush_cache_drive()` for durability
- Write-protect floor on the master: refuses below `ATA_WRITE_PROTECT_LBAS` (2048)
- `atatest` kernel shell diagnostic: MBR signature, kernel header bytes, model strings
- Three bring-up bugs found and fixed: PIC mask restoration, exception frame offsets, LBA mode bit
- Files: `04_kernel_64bit/ata.c`, `04_kernel_64bit/include/ata.h`, `04_kernel_64bit/include/io.h`

### ✔ 3.21 — FatFs Integration (Read/Write)
- FatFs R0.15 vendored in `04_kernel_64bit/fatfs/`
- `diskio.c` shim maps FatFs `disk_read` / `disk_write` / `disk_ioctl` onto the ATA driver
- `FF_FS_READONLY = 0`, `FF_FS_MINIMIZE = 0`, `FF_MULTI_PARTITION = 0`
- `FF_USE_LFN = 0`, `FF_VOLUMES = 1`, `FF_MIN_SS = FF_MAX_SS = 512`
- Kernel shell commands: `fatmount`, `fatls`, `fatcat <file>`
- Dual-drive storage: kernel on master `hdd.img`, FAT16 volume on slave `fat.img`
- `ata.c` drive-parameterized API so FatFs can target the slave independently of the boot chain
- Kernel size grew by ~25–30 KB for FatFs; kernel-size tripwire added to the Makefile

### ✔ 3.22 — Userland File I/O over FatFs
- Per-process file table in the PCB: slots 0, 1, 2 reserved for stdin/stdout/stderr; slots 3..7 for `open`/`close`
- Syscalls wired: `SYS_OPEN` (4), `SYS_CLOSE` (6), `SYS_GETPID` (20), and an fd branch in `SYS_READ` (3)
- `sys_exit` closes any files still open
- Userland `arc2/syscalls.c` exposes `open`, `close`, `read`, `write` shims over the syscall instruction
- `sys_open` flag handling uses newlib BSD-style values (`O_CREAT = 0x200`, `O_TRUNC = 0x400`, etc.) with access mode in the low 2 bits
- Verified end-to-end: create, write, close, reopen, read, byte-exact round trip from Ring 3
- A latent bug was fixed along the way: `sys_open` only set `FA_WRITE` when `flags == 1`, so `O_WRONLY | O_CREAT | O_TRUNC` (0x601) never enabled write mode

### ✔ 3.23 — Single-Drive Layout ⭐ v0.5.0
- Boot chain, kernel, and FAT16 partition on one `hdd.img`
- Partition starts at LBA 2048; kernel region is LBA 128..2047 (up to 960 KB)
- `FAT_CONFIG=dual|single` at build time selects the layout
- `include/fat_config.h` derives `FAT_DRIVE` (`ATA_DRIVE_MASTER` in single, `ATA_DRIVE_SLAVE` in dual) and `FAT_VOLUME_SECTORS` from `FAT_CONFIG_SINGLE_DRIVE`
- `diskio.c` translates volume-relative sector numbers into device LBAs via `FAT_PARTITION_OFFSET` (2048 in single, 0 in dual)
- **Key correction:** with `FF_MULTI_PARTITION = 0`, FatFs is not partition-aware. It treats the FAT volume as starting at LBA 0 of the physical drive and does not consult the partition table or the BPB `hidden_sectors` field. The offset therefore must live in `diskio.c`. An earlier iteration assumed FatFs would read `hidden_sectors` and skipped the offset; the mount failed with `FR_NO_FILESYSTEM` (13)
- `atatest` write leg removed: LBA 1024 is inside the kernel region in single-drive mode. Read-only checks remain
- `kmain.c` prints a config-specific boot-time storage line to VGA and serial
- `hdd-single.img` built by `mkfs.vfat -F 16 -h 2048 --offset=2048` and populated by `mcopy` from `test-files/`
- Image-build rule verifies the FAT boot sector and the BPB `hidden_sectors` field after assembly
- Kernel-size tripwire now checks two limits: the stage2 staging ceiling (176 KB) and the disk-layout ceiling (983 KB, the region below LBA 2048)
- User shell gains options 5 (persistence), 6 (multi-file), 7 (4 KB round-trip). `[FS TEST]` fixed: `msg_len` was hardcoded to 19 but the string is 20 bytes

### ✔ 3.24 — Maintenance Batch: Self-Test, NX on Hardware, Kernel-Owned GDT ⭐ v0.5.1
- **Self-test infrastructure (`selftest` command).**  15 tests covering GDT descriptor contents, TSS fields, PMM allocation and freeing, VMM control registers, page mapping, recursive paging, heap integrity, the NX bit in the final PTE, syscall entry point, ATA reads, FatFs mount and directory listing, and the three exception handlers (#DE, #PF, #GP).  The exception tests use an expected-fault protocol: a small kernel-mode child takes the fault, the handler records the vector, terminates the child, and resumes the kernel shell.  First time the exception handlers have been exercised by anything.  (`interrupts.c`, `interrupts.h`, `kmain.c`.)
- **EFER.NXE enabled.**  Previously the kernel wrote NX bits into PTEs but the CPU ignored them because `EFER.NXE` was clear.  Worked under KVM by accident (KVM's shadow MMU enables NX on the host side regardless of the guest's EFER); would have faulted on bare metal.  `enable_nx()` in `kmain.c` now sets the bit, guarded by a CPUID check.  `test_vmm` asserts on it.  (`kmain.c`.)
- **Kernel-owned GDT.**  The GDT lived in low memory (base `0x101DC`, inside `stage2.asm`'s loaded image), read through the bootloader's identity map.  `gdt_init` now builds `kernel_gdt[16]` in `.bss` at a higher-half address, `lgdt`s it, and reloads the segment registers.  `gdt_set_tss` writes the TSS descriptor directly.  `gdt_fix_user_segments` became a no-op.  `test_gdt`'s strict assertion is restored.  (`gdt.c`, `gdt.h`, `kmain.c`.)
- **Print atomicity.**  Kernel serial and VGA drivers now share a single print lock (cli/sti critical section with a nesting counter and RFLAGS save/restore).  Multi-part boot messages wrap in `serial_lock`/`serial_unlock`; ATA's read-path prints are gated behind `ATA_DEBUG`.  The DonsDOS banner truncation and interleaved boot trace are gone.  (`serial.c`, `vga.c`, `ata.c`.)
- **#DF through IST1.**  A dedicated 4 KB stack and an IST entry on the `#DF` gate turn a double fault into a printed diagnostic instead of a silent triple-fault reset.  (`idt.c`, `tss.c`.)
- **Kernel shell is process-backed.**  It runs as a real process with a pool-allocated kernel stack, not a hardcoded address (`0xFFFFFFFF8008FF00` is gone).  (`kmain.c`, `process.c`.)
- **Staging ceiling raised** from 176 KB to 448 KB by adding PASS 4–7 in `stage2.asm`.
- **Reboot flushes file handles.**  The user shell's reboot path flushes open file handles before the hardware reset; option 9 added.  (`user_shell.c`, `user_syscall.c`.)
- **MAINTENANCE.md.**  No open findings remain.  Sections 5b (boot-time self-test mode) and 5c (make test) deferred with the conditions that would make them worth revisiting.

**Key learnings:**
- A self-test suite is the right investment: it found two real latent bugs (EFER.NXE off, GDT in low memory) that no one was looking for.
- "Works under KVM" is not the same as "works on hardware."  KVM's shadow MMU is more forgiving than the architecture requires.  When a feature depends on a CPU feature flag, enable the flag explicitly rather than relying on the hypervisor to enable it for you.
- Low-memory structures are fragile.  The GDT was the second such structure (after the boot stack in item 3a) to be relocated to the higher half.  Anything the bootloader left in low memory is a candidate for the same treatment.
- Print atomicity is not just cosmetic.  Interleaved serial output obscures real diagnostics.  A single shared lock is enough until per-user ttys exist.

---

## ⭐ v0.4.2 — STAR MSR Fix (August 2026)

**What was accomplished:**
- Fixed IA32_STAR MSR configuration for SYSCALL/SYSRET
- User CS = 0x30 → STAR[15:0] = 0x20
- Enabled clean SYSRET return path

**Key learnings:**
- STAR MSR requires careful selector calculation (`User CS - 16`)
- SYSRET uses STAR[15:0] + 16 for CS and STAR[15:0] + 8 for SS

---

## ⭐ v0.4.3 — ELF Loader Stabilized + Process Foundation (August 2026)

**What was accomplished:**
- ELF loader now works on **first boot** (no more "run twice" bug)
- Bootloader identity‑mapping conflict resolved (detect and replace with proper user‑mode PTEs)
- `PT_EXEC` (PWT bit) handling added to `vmm_map_page()`
- Safe HHDM‑based user‑space memory access in syscall handler (`safe_copy_from_user`)
- **Process Foundation:** PCB, `process_create()`, `proclist`, `proccreate`, `vmmclone`
- **Dynamic HHDM mapping:** `ensure_hhdm_mapped()`
- **BootInfo validation:** Magic number and version checking
- Removed redundant `simple` command
- All existing commands remain fully functional

**Key learnings:**
- Bootloader identity mappings must be replaced, not trusted
- `vmm_get_phys()` is more reliable than `vmm_is_mapped()` for detecting valid mappings
- HHDM is essential for safe kernel‑to‑user memory operations
- STAR MSR requires careful selector calculation (`User CS - 16`)

---

## ⭐ v0.4.4 — Process Stack Setup (August 2026)

**What was accomplished:**
- Added static kernel stack pool for processes (eliminates PMM corruption)
- Process creation with dedicated user and kernel stacks
- Process execution via direct function call (kernel mode)
- Process cleanup with `process_destroy()` (frees user stack, marks PCB unused)
- **`runproc` command** to create and execute a test process
- Shell returns properly after process execution
- All previous features (`proclist`, `proccreate`, `vmmclone`, `elfload`) remain fully functional

**Key learnings:**
- Static kernel stacks avoid dynamic allocation and PMM corruption
- Direct function call is simpler for testing than `iretq` user-mode transitions
- Process cleanup is essential to prevent memory leaks

## ⭐ v0.4.5 — Cooperative Scheduler (August 2026)

**What was accomplished:**
- Ready queue with round‑robin scheduling
- `process_yield()` for voluntary context switching
- `process_exit()` for clean process termination
- Assembly‑level context switching (`context_switch.asm`)
- **`testyield` command** to test cooperative scheduling
- **`schstat` command** to show scheduler statistics
- `runproc` now uses the scheduler
- All previous features (`proclist`, `proccreate`, `vmmclone`, `elfload`) remain fully functional

**Key learnings:**
- Context switching requires saving/restoring all registers
- The idle process needs special handling (no entry point)
- Processes must explicitly call `process_exit()` to terminate cleanly
- Round‑robin scheduling requires moving processes to the end of the ready queue

---

## ⭐ v0.4.6 — Preemptive Scheduler & Unlocked Core (August 2026)

**What was accomplished:**
- Multi-Pass Segment Reader: `stage2.asm` pulls kernel blocks in safe 128-sector chunks, advancing segment offsets dynamically to defeat the 64 KB real-mode wrap limit
- Kernel disk read threshold lifted to 256 sectors (128 KB)
- `SYS_REBOOT` (syscall #25) added; Ring 3 user shell option 4 reboots via a Ring 0 triple-fault
- Preemptive core integration: PIT clock (`IRQ0` at 100 Hz) enforces quantum-based task slicing across ready queues

**Key learnings:**
- Real-mode segment arithmetic matters for large kernels
- A userland reboot needs a controlled, tested path back to ring 0
- Preemptive slicing is stable when the timer path saves and restores per-process kernel frames

---

## ⭐ v0.4.7 — newlib, Blocking I/O, Boot Choice (September 2026)

**What was accomplished:**
- **newlib 4.x in userland**: `printf`, `malloc`/`free`, `memcpy`, `str*` work in Ring 3, statically linked against `libc.a`/`libm.a`
- **Kernel-stack-on-syscall-entry**: syscall path runs on a per-process kernel stack (from A2, tag `20260912I`)
- **Blocking `sys_read`**: reads no longer spin the CPU; blocked processes yield via the timer and are woken by `irq1`
- **Boot-time shell choice**: 2-second window, `k` for kernel shell, any other key (or timeout) for the user shell
- **User shell is the terminal console**: kernel shell is not reachable after boot without a reboot
- **Kernel diagnostics return to the kernel shell** on exit
- **Userland heap test**: user shell menu option 3 exercises `malloc`/`free` over `sys_brk`
- **`gdtdump` and `tssdump`** kernel shell commands
- **`testyield` fix**: `process_yield` no longer corrupts the ready queue
- **TSS.RSP0 / `g_syscall_stack_top` in lockstep** from boot

**Key learnings:**
- Kernel code must run on a kernel stack, not the user stack; the earlier `sti; hlt` spin in `sys_read` blocked the whole scheduler
- The timer path already does everything a cooperative switch needs; use it
- A second context-switch primitive (or fabricating iretq frames from PCB slots) causes more bugs than it solves
- `process_yield` must not remove the current process from the ready queue if it is not on it — remove on an off-queue process corrupts the queue
- Newlib reentrancy requires `_impure_ptr = &_impure_data` at startup
- `sys_brk` must `invlpg` after mapping, or the first user write faults

---

## ⭐ v0.4.8 — Process Cleanup on Exit (September 2026)

**What was accomplished:**
- **PCB reclaim.** `process_exit` calls a new `process_reclaim`, which frees the exiting process's ELF segment pages and user stack pages and marks the PCB slot reusable. Repeated `testyield` and `runproc` in one boot no longer exhaust the 32-slot pool.
- **`process_exit` timer race closed.** Interrupts are disabled for the entire critical section (state transition + queue removal + reclaim + context switch). The timer can no longer re-add the exiting process to the ready queue or save the current stack frame into `next->rsp`.
- **Page-table teardown deferred.** The process's page tables still leak; the teardown requires walking the page tables and freeing only the user-space portion.

**Key learnings:**
- Reclaiming the PCB made slot reuse happen sooner, which exposed a latent timer race in `process_exit`. The two fixes are coupled: the reclaim is correct, but only with the `cli` window.
- `context_switch` is not safe against timer preemption mid-switch. Any code path that mutates `current_process` and then calls `context_switch` must run with interrupts disabled, or the timer can save the wrong stack frame into the wrong PCB.
- A `noreturn` exit path that can fall through to a kernel shell or a halt must explicitly `sti` on that fallback, because the `cli` from the critical section persists until the next `iretq`.

---

## ⭐ v0.4.9 — Scheduler, TSS, and Interrupt ABI Stability Pass (September 2026)

**What was accomplished:**
- **ELF-load race at boot.** Fixed at all three call sites (`kmain` boot path, `elfload`, `usershell`). Eliminated the intermittent user-mode `#PF` at `CR2 == RIP == 0x8000000000` that reproduced when a key was pressed during the boot-choice window.
- **`TSS.RSP0` / `g_syscall_stack_top` lockstep.** Gate on `entry_point < KERNEL_BASE` removed in `scheduler_switch_to`, `process_exit`, and `timer_preempt_handler`. Both fields now update on every switch.
- **`process_exit` fallback TSS restoration.** The no-runnable-process fallback now restores both `TSS.RSP0` and `g_syscall_stack_top` to idle's kernel stack top.
- **Kernel stack slot aliasing.** Replaced `pid % MAX_PROCESSES` with an explicit `slot_owner[]` allocator and a `pcb_t::kernel_stack_slot` field. Survives 20+ `testyield` runs per boot with no fault.
- **`context_switch.asm` offsets.** Every offset at or after `user_stack_phys` updated by +8 to account for the new `kernel_stack_slot` field. `_Static_assert` block in `process.c` pins the offsets.
- **`scheduler_switch_to` preemption window.** Added `__asm__ volatile("cli")` at the top of the function to close the race between `current_process = next` and `context_switch(prev, next)`.
- **`irq1_stub` ABI violation.** All stubs that call C handlers now preserve all 15 GPRs. `isr8_stub` also fixed a latent missing `add rsp, 8` for the CPU's error code. `process_exit`'s fallback now uses a direct `jmp kmain_shell_loop`.
- **Frame validation in `context_switch.asm`.** `.kernel_task` now checks the resume frame's `rip` and `cs` before popping, and emits a serial marker byte (`R` or `C`) and halts on failure.

**Key learnings:**
- **Two-variable invariants need to be updated everywhere or nowhere.** The TSS.RSP0 / `g_syscall_stack_top` pair was updated in one path and skipped in another; the divergence was invisible until it wasn't.
- **Monotonic counters and modulo indexing don't mix.** `next_pid % MAX_PROCESSES` looks fine until `next_pid` exceeds `MAX_PROCESSES`, at which point it aliases live slots.
- **Any struct change in C needs to be reflected in the assembly that reads it.** The `_Static_assert` guard is the durable fix; the offset update is the one-time fix.
- **Stubs that call C code must save caller-saved registers.** The SysV ABI lets the callee clobber `rax`, `rcx`, `rdx`, `rsi`, `rdi`, `r8`–`r11`. Interrupt/exception entry points that return via `iretq` must restore them.
- **Indirect jumps through registers are fragile.** A direct jump (`jmp symbol`) emits a `rel32` with no register involved, and no interrupt can corrupt the target.

---

## ⭐ v0.4.10 — ATA PIO + FatFs (September 2026)

**What was accomplished:**
- **ATA PIO block device driver** on the primary channel. `ata_init` probes master and slave via IDENTIFY, extracts model strings, and logs to serial.
- **Per-drive API.** `ata_read_sector_drive`, `ata_read_sectors_drive`, `ata_write_sector_drive`, `ata_write_sectors_drive`, `ata_flush_cache_drive`. Default-drive wrappers remain for existing callers.
- **Write-protect floor on master.** Refuses writes below `ATA_WRITE_PROTECT_LBAS` (initially 64, later raised to 2048 with single-drive). Slave is unrestricted.
- **Three bring-up bugs found and fixed:**
  - **PIC mask restoration.** `pic_remap` was saving and restoring the BIOS PIC mask values; the BIOS leaves IRQ14 unmasked, so the first IDENTIFY asserted IRQ14 → vector 46 → `#GP` (no gate). Fixed by writing a known-good mask.
  - **Exception frame offsets.** `isr13_handler` and `isr14_handler` read the CPU-pushed error code / RIP / CS at offsets 0..4 from the frame base, but the frame base points at the top of the 15-GPR save area. Fixed by indexing from `EXC_OFF_ERROR_CODE = 15` upward.
  - **LBA mode bit.** `ata_select_drive` wrote 0xA0/0xB0 to the drive/head register, leaving bit 6 (the "L" bit) clear. IDENTIFY ignores L, but READ/WRITE SECTORS interpret the LBA registers as a CHS tuple when L=0. Fixed by using `ATA_DRIVE_MASTER_LBA = 0xE0` / `ATA_DRIVE_SLAVE_LBA = 0xF0` in the read/write path.
- **`atatest`** kernel shell diagnostic: read LBA 0 and verify the MBR signature, read LBA 64/128 and dump the kernel header, dump master and slave model strings.
- **FatFs R0.15 vendored** in `04_kernel_64bit/fatfs/` with a `diskio.c` shim over the ATA driver. `FF_FS_READONLY = 0`, `FF_FS_MINIMIZE = 0`, `FF_MULTI_PARTITION = 0`, `FF_USE_LFN = 0`.
- **Dual-drive storage.** Kernel on master `hdd.img`, FAT16 volume on slave `fat.img` (whole disk, no partition table).
- **Kernel shell commands:** `fatmount`, `fatls`, `fatcat <file>`.
- **Userland file I/O.** Per-process file table, `SYS_OPEN` (4), `SYS_CLOSE` (6), `SYS_GETPID` (20), and an fd branch in `SYS_READ` (3). `arc2/syscalls.c` wires newlib's `open`/`close`/`read`/`write` to the syscall instruction.
- **Latent bug fixed:** `sys_open` only set `FA_WRITE` when `flags == 1`, so `O_WRONLY | O_CREAT | O_TRUNC` (0x601) never enabled write mode.
- **`FF_FS_READONLY = 1 → 0`** exposed a `.userelf` corruption bug: `stage2.asm` was staging the third 64 KB read into VGA graphics memory (`0xA0000`), which QEMU did not reliably persist. Fixed by moving PASS 3 to `0x20000` (plain RAM). Also added `.bss` zeroing in `entry.asm` and stripped the embedded user ELF before `xxd -i`.

**Key learnings:**
- BIOS PIC mask values are not safe to restore; write a known-good mask instead
- Exception frames from stubs that push 15 GPRs have the CPU-pushed fields at offset 15, not 0
- The ATA drive/head "L" bit is required for LBA mode on READ/WRITE SECTORS
- `0xA0000–0xAFFFF` is VGA graphics memory in any mode, including text mode 03h; it is not free RAM
- `FF_MULTI_PARTITION = 0` in FatFs means FatFs is not partition-aware (see v0.4.11)

---

## ⭐ v0.4.11 — Single-Drive Layout (September 2026)

**What was accomplished:**
- **`FAT_CONFIG=dual|single`** at build time selects the storage layout. The Makefile passes `-DFAT_CONFIG_SINGLE_DRIVE=0|1`; `include/fat_config.h` derives the drive selection and volume-sector count.
- **`diskio.c` partition offset.** `FAT_PARTITION_OFFSET` is 0 in dual, 2048 in single. `disk_read` and `disk_write` add it to every FatFs-supplied sector number before calling the ATA layer. `disk_ioctl(GET_SECTOR_COUNT)` returns `FAT_VOLUME_SECTORS` (the volume size, not the device size).
- **Key correction to an earlier design.** With `FF_MULTI_PARTITION = 0`, FatFs reads the FAT boot sector at LBA 0 of the physical drive and does not consult the BPB `hidden_sectors` field. An earlier iteration assumed FatFs would read `hidden_sectors` and skipped the offset in `diskio.c`; the mount then failed with `FR_NO_FILESYSTEM` (13), because FatFs read the boot sector at LBA 0 (boot chain) instead of the FAT boot sector at LBA 2048.
- **Write-protect floor raised** from LBA 64 to LBA 2048. In single-drive mode the kernel lives in LBA 128..2047, so a stray write below 2048 would corrupt the kernel. The floor also covers the boot chain and the gap.
- **`atatest` write leg removed.** LBA 1024 is inside the kernel region in single-drive mode. The raw-sector round-trip was a footgun on a disk that now has a filesystem on it; the user shell's `[FS TEST]` exercises the write path through FatFs.
- **`kmain.c` config diagnostic.** Boot-time mount prints a config-specific line to VGA and serial: `"Storage: single-drive, FAT@LBA 2048"` or `"Storage: dual-drive, FAT@LBA 0 on slave"`. Mount failures now appear on VGA, not just serial.
- **`hdd-single.img`** built by `mkfs.vfat -F 16 -h 2048 --offset=2048` and populated by `mcopy` from `test-files/`. The image-build rule verifies the FAT boot sector and the BPB `hidden_sectors` field after assembly.
- **Kernel-size tripwire** now checks two limits: the stage2 staging ceiling (176 KB) and the disk-layout ceiling (983 KB, the region below LBA 2048).
- **User shell gains options 5, 6, 7.** Option 5 verifies persistence across reboot; option 6 creates 3 files and verifies contents (delete skipped, no `SYS_UNLINK`); option 7 is a 4 KB round-trip that catches multi-cluster bugs.
- **`[FS TEST]` fixed.** `msg_len` was hardcoded to 19 but `"Hello from ring 3!\r\n"` is 20 bytes; the trailing `\n` was being silently truncated.

**Key learnings:**
- `FF_MULTI_PARTITION = 0` means the partition offset belongs in `diskio.c`, not the BPB
- A write-protect floor must be maintained when the kernel or partition layout changes; a stale floor is a latent bug
- The user shell is the right place for regression tests: it exercises the syscall path, the FatFs path, and the persistence path through the same code real programs use

---

## ⭐ v0.5.0 — Storage Layer Complete (September 2026)

**What was accomplished:**
- ATA PIO block device driver (see v0.4.10)
- FatFs R0.15 vendored and integrated, read and write, kernel-side and userland (see v0.4.10)
- Single-drive layout with FAT16 partition at LBA 2048 (see v0.4.11)
- Persistence across reboot verified end-to-end
- Expanded user-shell regression harness (options 4, 5, 6, 7)
- Config-specific boot-time storage diagnostic on VGA and serial
- Kernel-size tripwire checks both the stage2 staging ceiling (176 KB) and the disk-layout ceiling (983 KB)
- A new `run` script at the repo root: one-line-per-option QEMU launcher

**Key learnings:**
- The storage layer is a natural milestone: it exercises the boot chain, the ATA driver, the FatFs shim, the syscall layer, the userland C library, and the file I/O path all at once
- Two configurations (dual-drive, single-drive) with a single kernel source is achievable when the difference is expressed as compile-time constants and a small amount of `diskio.c` offset arithmetic
- Persistence is the right shape for a storage test; buffered writes that pass in-memory round-trips are not proof of durability

**Not yet implemented:**
- `SYS_UNLINK` (option 6's delete phase is skipped)
- Loading user programs from disk (`sys_exec`-style)

---

## ⭐ v0.5.1 — Maintenance Batch: Self-Test, NX, Kernel-Owned GDT (September 2026) ⭐ NEW

**What was accomplished:**
- Self-test infrastructure (`selftest` command, 15 tests) — see §3.24
- `EFER.NXE` enabled so NX bits in PTEs are enforced on hardware — see §3.24
- Kernel-owned GDT at a higher-half address — see §3.24
- Print atomicity (shared serial/VGA lock) — see §3.24
- `#DF` routed through IST1 — see §3.24
- Kernel shell runs on a pool-allocated stack; the hardcoded `0xFFFFFFFF8008FF00` is gone — see §3.24
- Staging ceiling raised from 176 KB to 448 KB
- Reboot flushes file handles before the reset

**Key learnings:**
- A self-test suite is the right investment: it found two real latent bugs (EFER.NXE off, GDT in low memory) that no one was looking for.
- "Works under KVM" is not the same as "works on hardware."
- Low-memory structures are fragile. Anything the bootloader left in low memory is a candidate for the same relocation treatment the boot stack (item 3a) and GDT (item 3h) received.
- Print atomicity is not just cosmetic. Interleaved serial output obscures real diagnostics.

**Not yet implemented:**
- `SYS_UNLINK`
- Loading user programs from disk (`sys_exec`)
- Boot-time self-test mode and `make test` (deferred; see `MAINTENANCE.md` §5b–5c)

## ⭐ v0.5.2 — Heap Rewrite and Validator (September 2026) ⭐ NEW

**What was accomplished:**
- `heap_extend` rewritten to place new blocks at the start of the newly mapped region. The previous version placed them at `heap_brk - requested_size`, which lost up to `PAGE_SIZE - 1` bytes per extension and put blocks at the wrong end of the mapped range. See the new `heap.c`.
- `heap_header_t` padded from 40 to 48 bytes so payloads are 16-aligned when the allocation size is a multiple of `HEAP_ALIGNMENT`.
- `heap_validate()` walks the block list and checks every invariant, including that the last block's end equals `heap_brk`. The old implementation violated that invariant on every extension.
- `heap_stress()` runs 4096 operations across 512 slots with per-block patterns, then validates. The run grows the heap from 1 MB to ~4.15 MB and reports zero leak with the heap intact.
- `heapcheck` and `heapstress` kernel shell commands; both also run from `selftest`.
- **LLD 22.1.8 workaround.** At `-O2`, linking `kernel.elf` with `ld.lld` 22.1.8 produces truncated instructions in `check_fs` and `move_window`. The standalone object file is correct, so the truncation happens at link time. `fatfs/ff.o` is compiled at `-O1` to avoid it. Bug filed upstream; see `LLD_BUG_REPORT.md` in the repo root.

**Key learnings:**
- A heap allocator without a validator is a heap allocator you cannot trust. The `heap_validate` invariant that the last block's end must equal `heap_brk` is what would have caught the old extension bug immediately, and it is what proves the new code is correct.
- The `heap_stress` test must be sized to actually exceed the initial heap. The first version allocated only ~256 KB and never forced an extension; the size constants were wrong. The version in the tree allocates several megabytes and forces multiple extensions.
- A link-time truncation bug is much harder to diagnose than a compiler bug. The two look identical in a disassembly of the linked binary; they are distinguished by compiling the same source standalone and comparing. `LLD_BUG_REPORT.md` documents the reproducer.
- A Makefile without header dependency tracking turns every header change into a two-step rebuild. `make clean` is required after `heap.h` changes. Adding `-MMD -MP` would fix it; not done yet.

**Not yet implemented:**
- Header dependency tracking in `04_kernel_64bit/Makefile` (`-MMD -MP`).
- Long filename support (`FF_USE_LFN`).
- `SYS_UNLINK`.
- Loading user programs from disk (`sys_exec`).

---

---

## 4. User‑Facing Features

### ✔ 4.1 — Permanent Storage Layer ⭐ v0.5.0
- ~~ATA PIO block device driver (read/write sectors from long mode)~~ ✅
- ~~FatFs integration (FAT12/FAT16/FAT32)~~ ✅
- ~~Mount a filesystem, `f_open` / `f_read` / `f_write` / `f_close`~~ ✅
- ~~Kernel shell commands: `fatmount`, `fatls`, `fatcat`~~ ✅
- ~~Userland file I/O via `SYS_OPEN` (4), `SYS_CLOSE` (6), fd branch in `SYS_READ` (3)~~ ✅
- ~~Single-drive layout with FAT16 partition at LBA 2048~~ ✅
- ~~Persistence across reboot verified~~ ✅
- ☐ Load user programs from disk rather than embedding them — see §4.9 (`sys_exec`)

### ☐ 4.2 — Framebuffer Graphics
- Switch from VGA text mode
- Draw pixels, shapes, text
- Simple GUI experiments

### ☐ 4.3 — Serial Console Debug Access
- Kernel shell reachable over COM1 (input and output)
- Physically separate from the user's keyboard; cannot be triggered from user code
- This is the right shape for runtime kernel-shell access. The magic-key-combo approach was tried and abandoned: it is a security backdoor, and the kernel shell is not a process the scheduler can suspend and resume.

### ☐ 4.4 — Per-Process tty / Console Focus
- Prerequisite for multiple concurrent shells
- Route keyboard input to the focused shell instead of the current shared-buffer behavior

### ☐ 4.5 — File System (VFS)
- Virtual File System (VFS) layer above FatFs
- File operations (`open`, `read`, `write`, `close`)
- Path resolution, mount points

### ☐ 4.6 — Device Drivers
- Serial/COM port (working for output; input needed for the serial console)
- PCI enumeration
- AHCI disk driver
- PS/2 mouse

### ✔ 4.7 — Kernel Shell on a Proper Stack ⭐ v0.5.1
- ~~Move `kmain_shell_loop` off the hardcoded `0xFFFFFFFF8008FF00` boot stack~~ ✅
- ~~Allocate the shell's stack from the kernel stack pool~~ ✅
- ~~Eliminates the `process_exit` fallback's dependency on a specific low-memory address being mapped~~ ✅
- ~~Companion: IST stack for `#DF`, so double faults produce printed diagnostics instead of triple faults~~ ✅

Done in v0.5.1.  The kernel shell is now a real process (`kshell`) created by `kmain` on the `k` branch, with a stack allocated from the kernel stack pool.  `process_exit`'s fallback resumes it via `scheduler_switch_to`.  The `#DF` handler runs on IST1, a dedicated 4 KB stack, so a double fault produces a printed diagnostic rather than a triple-fault reset.

### ☐ 4.8 — `SYS_UNLINK`
- Small syscall addition: `f_unlink` behind `SYS_UNLINK`
- Wires `unlink()` in `arc2/syscalls.c`
- Completes the multi-file test in the user shell (option 6's delete phase)

### ☐ 4.9 — User Programs from Disk
- `sys_exec`-style syscall
- Load ELF files from the FAT volume
- Stop embedding user programs in the kernel image
- Enables a real shell with external commands (`ls`, `cat`, etc.)
- This is the next feature milestone.

### ☐ 4.10 — ELF Loader `PT_NX` Follow-up
- With `EFER.NXE` now enabled (v0.5.1), `elf_load_into_process` can mark non-executable segments (data, BSS, user stack) with `PT_NX`.
- Currently every segment is mapped with the same flags.  About an hour of work.
- Closes the follow-up noted in `MAINTENANCE.md` §3g.

---

## 5. Development Tools

### ✔ QEMU Debug Mode
- `-serial stdio` for real-time serial console output
- `-serial file:qemu.log` for saving serial output
- `-d int,cpu_reset,guest_errors` for interrupt logging
- `-no-reboot -no-shutdown` for debugging crashes
- Multiple run modes: run, run-log, run-debug, run-verbose, run-headless, run-kvm, run-single, run-kvm-single, run-telnet

### ✔ GDB Remote Debugging
- `make runkernel64-debug` for GDB server
- Connect with: `gdb -ex "target remote localhost:1234" kernel.elf`

### ✔ Build Automation
- Top‑level Makefile with debug targets  
- `make logkernel64` for debug runs
- `FAT_CONFIG=dual|single` selects the storage layout
- `./run` convenience script with a menu of preconfigured targets

---

## Status Summary

| Stage | Status |
|-------|--------|
| Boot chain | ✔ Complete |
| Long‑mode kernel | ✔ Complete |
| Interrupts (IRQ0/IRQ1) | ✔ Complete |
| PMM | ✔ Complete |
| VGA Console | ✔ Complete |
| Command Shell | ✔ Complete |
| Higher‑half kernel | ✔ Complete |
| Exception Handlers | ✔ Complete |
| Virtual Memory Manager | ✔ Complete |
| Serial Debug Output | ✔ Complete |
| Heap Allocator | ✔ Complete |
| User Mode (Ring 3) | ✔ Complete |
| NX Bit Support | ✔ Complete |
| System Calls | ✔ Complete |
| **ELF Loader** | **✔ Complete ⭐ FINALIZED** |
| **STAR MSR Fix** | **✔ Complete ⭐ v0.4.2** |
| **Process Foundation** | **✔ Complete ⭐ v0.4.3** |
| **ELF Loader Stabilized** | **✔ Complete ⭐ v0.4.3** |
| **Syscall Stack Stability** | **✔ Complete ⭐ v0.4.3** |
| **HHDM Dynamic Mapping** | **✔ Complete ⭐ v0.4.3** |
| **BootInfo Validation** | **✔ Complete ⭐ v0.4.3** |
| **Process Stack Setup** | **✔ Complete ⭐ v0.4.4** |
| **Process Execution** | **✔ Complete ⭐ v0.4.4** |
| **Process Cleanup** | **✔ Complete ⭐ v0.4.4** |
| **Cooperative Scheduler** | **✔ Complete ⭐ v0.4.5** |
| **Context Switching** | **✔ Complete ⭐ v0.4.5** |
| **Process Yield/Exit** | **✔ Complete ⭐ v0.4.5** |
| **Preemptive Scheduler** | **✔ Complete ⭐ v0.4.6** |
| **Segment-Shifting Bootloader** | **✔ Complete ⭐ v0.4.6** |
| **Userland Syscall Reboot** | **✔ Complete ⭐ v0.4.6** |
| **Kernel-Stack-on-Syscall-Entry** | **✔ Complete ⭐ v0.4.7** |
| **newlib 4.x in userland** | **✔ Complete ⭐ v0.4.7** |
| **Blocking sys_read** | **✔ Complete ⭐ v0.4.7** |
| **Boot-Time Shell Choice** | **✔ Complete ⭐ v0.4.7** |
| **Userland Heap via sys_brk** | **✔ Complete ⭐ v0.4.7** |
| **gdtdump / tssdump** | **✔ Complete ⭐ v0.4.7** |
| **Process Cleanup on Exit** | **✔ Complete ⭐ v0.4.8** |
| **Scheduler / TSS / Interrupt ABI Stability** | **✔ Complete ⭐ v0.4.9** |
| **ATA PIO Driver** | **✔ Complete ⭐ v0.4.10** |
| **FatFs Integration** | **✔ Complete ⭐ v0.4.10** |
| **Userland File I/O over FatFs** | **✔ Complete ⭐ v0.4.10** |
| **Single-Drive Layout** | **✔ Complete ⭐ v0.5.0** |
| **Persistence Across Reboot** | **✔ Complete ⭐ v0.5.0** |
| **Self-Test Infrastructure (15 tests)** | **✔ Complete ⭐ v0.5.1** |
| **EFER.NXE enabled (NX on hardware)** | **✔ Complete ⭐ v0.5.1** |
| **Kernel-Owned GDT (higher-half)** | **✔ Complete ⭐ v0.5.1** |
| **Print Atomicity (shared serial/VGA lock)** | **✔ Complete ⭐ v0.5.1** |
| **#DF through IST1** | **✔ Complete ⭐ v0.5.1** |
| **Kernel Shell on a Pool-Allocated Stack** | **✔ Complete ⭐ v0.5.1** |
| **Heap Rewrite and Validator** | **✔ Complete ⭐ v0.5.2** |
| **LLD 22.1.8 Workaround** | **✔ Complete ⭐ v0.5.2** |
| **Staging Ceiling Raised to 448 KB** | **✔ Complete ⭐ v0.5.1** |
| `SYS_UNLINK` | ☐ Planned |
| User Programs from Disk (`sys_exec`) | ☐ Planned (next feature) |
| ELF Loader `PT_NX` Follow-up | ☐ Planned |
| Serial Console Debug Access | ☐ Planned |
| Framebuffer Graphics | ☐ Planned |
| File System (VFS) | ☐ Planned |

---

## Notes

The roadmap is intentionally incremental.  
Each milestone builds toward a fully functional x86_64 kernel while keeping the project educational and approachable.

For known debt, latent bugs, and cleanup work, see [`MAINTENANCE.md`](MAINTENANCE.md). That file is the counterpart to this roadmap: this file lists features, that file lists everything else that needs attention.

MIT licensed — contributions welcome.
