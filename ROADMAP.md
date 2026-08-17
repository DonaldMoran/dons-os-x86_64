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
- Built‑in commands: help, clear, info, mem, version, reboot, pmmtest, test, vmmtest, serialtest, heapstat, maptest, testrec, heaptest, **user**, **user2**, nxtest, syscall, **elfload**
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

### ✔ 3.4 — Kernel Heap Allocator
- `kmalloc()` bump allocator
- `kfree()` free list (memory reuse)
- Automatic heap expansion
- `heapstat` debugging command
- `heaptest` allocation/reuse verification
- 64MB initial heap size

### ✔ 3.5 — User Mode (Ring 3)
- GDT with user segments (DPL=3)
- User code (0x2B) and user data (0x33) segments
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
- SYS_EXIT (syscall #60)
- Syscall dispatcher with x86_64 ABI
- `syscall` test command
- SYSRET returns to user mode

### ✔ 3.8 — ELF Loader ⭐ NEW
- Parses ELF64 headers and program headers
- Maps LOAD segments with correct permissions
- Allocates and maps user stack pages
- Transitions to user mode via IRETQ (CS=0x2B, SS=0x33)
- Sets IOPL=3 for user I/O access
- Page table execute permissions at all levels
- **`elfload` command** for running embedded ELF programs
- Tested with "Hello from Userland!" via serial

---

## ⭐ 3.9 — v0.4.1-syscall-stack-stable (August 2026)

**What was accomplished:**
- Unified kernel stack model for syscall entry/exit  
- Stabilized SYSRET path for user → kernel → shell transitions  
- Verified clean return from ELF user programs  
- Correct handling of RCX/R11 for SYSRET  
- **Removed the `simple` command** (redundant; replaced by ELF loader + future usermode tests)  
- `user` and `user2` retained as placeholders for upcoming process work

**Key learnings:**
- SYSRET requires valid user RIP and RFLAGS  
- Kernel stack must be restored before returning to shell  
- TSS.RSP0 must always point to a stable kernel stack  
- Redundant usermode tests can be retired once ELF loader is stable

**Known limitations:**
- No scheduler yet (SYS_EXIT halts instead of switching)  
- No process teardown beyond returning to shell  

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
| **ELF Loader** | **✔ Complete ⭐ NEW** |
| Syscall Stack Stability | **✔ Complete ⭐ NEW** |
| Process Model | ☐ Planned |
| Scheduler | ☐ Planned |

---

## Notes

The roadmap is intentionally incremental.  
Each milestone builds toward a fully functional x86_64 kernel while keeping the project educational and approachable.

MIT licensed — contributions welcome.
