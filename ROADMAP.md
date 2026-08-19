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
- Built‑in commands: help, clear, info, mem, version, reboot, pmmtest, test, vmmtest, serialtest, heapstat, maptest, testrec, heaptest, nxtest, syscall, **elfload**, **proclist**, **proccreate**, **vmmclone**, **runproc**, **schstat**, **testyield**
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
- User code (0x30) and user data (0x28) segments
- TSS initialization for stack switching
- `iretq` transition from kernel to user mode
- User memory mapped with PT_USER flag
- `create_user_process()` for launching user code
- Test commands: **user**, **user2** (placeholders for future usermode tests)

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
- Syscall dispatcher with x86_64 ABI
- `syscall` test command
- SYSRET returns to user mode
- **Safe user‑space memory access** via `safe_copy_from_user()` using HHDM

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

### ✔ 3.10 — Process Stack Setup ⭐ NEW
- Static kernel stack pool for processes
- Process creation with dedicated user and kernel stacks
- Process execution via direct function call (kernel mode)
- Process cleanup with `process_destroy()` (frees user stack, marks PCB unused)
- **`runproc` command** to create and execute a test process
- Shell returns properly after process execution
- All previous features remain fully functional

### ✔ 3.11 — Cooperative Scheduler ⭐ NEW
- Ready queue with round‑robin scheduling
- `process_yield()` for voluntary context switching
- `process_exit()` for clean process termination
- Assembly‑level context switching (`context_switch.asm`)
- **`testyield` command** for testing cooperative scheduling
- **`schstat` command** for scheduler statistics
- `runproc` now uses the scheduler
- All previous features remain fully functional

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

## 4. User‑Facing Features

### ☐ 4.1 — Framebuffer Graphics
- Switch from VGA text mode  
- Draw pixels, shapes, text  
- Simple GUI experiments

### ☐ 4.2 — File System
- Virtual File System (VFS) layer  
- FAT32 or ext2 support  
- File operations (open, read, write, close)

### ☐ 4.3 — Device Drivers
- Serial/COM port (working)
- PCI enumeration
- AHCI disk driver
- PS/2 mouse

---

## 5. Development Tools

### ✔ QEMU Debug Mode
- `-serial stdio` for real-time serial console output
- `-serial file:qemu.log` for saving serial output
- `-d int,cpu_reset,guest_errors` for interrupt logging
- `-no-reboot -no-shutdown` for debugging crashes
- Multiple run modes: run, run-log, run-debug, run-verbose, run-headless, run-kvm

### ✔ GDB Remote Debugging
- `make runkernel64-debug` for GDB server
- Connect with: `gdb -ex "target remote localhost:1234" kernel.elf`

### ✔ Build Automation
- Top‑level Makefile with debug targets  
- `make logkernel64` for debug runs

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
| Preemptive Scheduler | ☐ Planned (Next) |
| Framebuffer Graphics | ☐ Planned |
| File System | ☐ Planned |

---

## Notes

The roadmap is intentionally incremental.  
Each milestone builds toward a fully functional x86_64 kernel while keeping the project educational and approachable.

MIT licensed — contributions welcome.
