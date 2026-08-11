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
- Built‑in commands: help, clear, info, mem, version, reboot, pmmtest, test, vmmtest, serialtest, heapstat, maptest, testrec, heaptest, **simple, user, user2**
- Command history with backspace
- Interactive prompt `>`

---

## 3. Core Kernel Features (Completed)

### ✔ 3.1 — Higher‑Half Kernel
- Map kernel to `0xFFFFFFFF80100000` (completed)  
- Update linker script (completed)  
- Clean identity map and higher-half mapping working (completed)

### ✔ 3.2 — Exception Handlers (Completed)
- ✅ Page Fault handler (dump CR2, error code, RIP)
- ✅ Divide by Zero handler (message and halt)
- ✅ General Protection Fault handler (dump ERR, RIP, CS)
- ✅ Double Fault handler (registered)
- Test command (`test`) for triggering all exceptions
- Stack trace on panic - ☐ Planned

### ✅ 3.3 — Virtual Memory Manager (VMM) - COMPLETED
- ✅ CR3/PML4 read and displayed at boot
- ✅ HHDM_START defined and printed
- ✅ vmmtest command implemented
- ✅ Serial debug output (COM1)
- ✅ **Recursive paging** implemented at PML4[510]
- ✅ PML4 read/write from higher-half kernel
- ✅ Dynamic page table allocation (PDPT, PD, PT)
- ✅ HHDM mapping (PML4[256] mapped)
- ✅ No GP faults when accessing page tables
- ✅ User-space page mapping with PT_USER flag

### ✅ 3.4 — Kernel Heap Allocator - COMPLETED
- ✅ `kmalloc()` working with bump allocator
- ✅ `kfree()` working with free list (memory reuse)
- ✅ Automatic heap expansion
- ✅ `heapstat` debugging command
- ✅ `heaptest` test command (verifies allocation and reuse)
- ✅ 64MB initial heap size
- ✅ Free list for memory reuse
- ☐ Slab/buddy allocator (future enhancement)
- ☐ Memory pools for small objects (future enhancement)

### ✅ 3.5 — User Mode (Ring 3) - COMPLETED ⭐ NEW
- ✅ GDT management with user segments (DPL=3)
- ✅ User code (0x2B) and user data (0x33) segments
- ✅ TSS initialization for stack switching on interrupts
- ✅ `iretq`-based transition from kernel to user mode
- ✅ User code executes at CPL=3 with page protection
- ✅ User memory mapped with PT_USER flag for user/kernel isolation
- ✅ `create_user_process()` for launching user code
- ✅ Test commands: `simple`, `user`, `user2` for user mode verification
- ⚠️ NX bit not yet enabled — all pages are executable by default

### ☐ 3.6 — NX Bit Support (Next)
- Enable NX bit in EFER
- Add PT_NX flag for proper execute permission control
- Mark data pages as non-executable

### ☐ 3.7 — Slab Allocator (Future Enhancement)
- Advanced memory allocation with size classes
- Memory pool for small objects
- Better performance for frequent allocations

### ☐ 3.8 — System Calls
- `syscall` instruction setup
- System call handler
- User library for system calls

### ☐ 3.9 — Process Model
- Process Control Block (PCB)
- Page table per process
- Context switching

### ☐ 3.10 — ELF Loader
- ELF parsing
- Program loading
- User program execution

### ☐ 3.11 — Scheduler Prototype
- Timer‑driven task switching  
- Process Control Block (PCB)  
- Cooperative or preemptive

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
- ✅ Serial/COM port (working)
- ☐ PCI enumeration
- ☐ AHCI disk driver
- ☐ PS/2 mouse

---

## 5. Development Tools

### ✅ QEMU Debug Mode
- `-serial stdio` for real-time serial console output
- `-serial file:qemu.log` for saving serial output
- `-d int,cpu_reset,guest_errors` for interrupt logging
- `-no-reboot -no-shutdown` for debugging crashes
- Multiple run modes: `run`, `run-log`, `run-debug`, `run-verbose`, `run-headless`, `run-kvm`

### ✅ GDB Remote Debugging
- `make runkernel64-debug` for GDB server
- `-s -S` flags enabled
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
| Exception Handlers | ✔ Complete (#DE, #PF, #GP) |
| **Virtual Memory Manager** | **✔ Complete (recursive paging)** |
| Serial Debug Output | ✔ Complete |
| **Heap Allocator (kmalloc/kfree)** | **✔ Complete** |
| **User Mode (Ring 3)** | **✔ Complete** ⭐ NEW |
| NX Bit Support | ☐ Planned |
| Slab Allocator | ☐ Planned |
| System Calls | ☐ Planned |
| Process Model | ☐ Planned |
| ELF Loader | ☐ Planned |
| Scheduler | ☐ Planned |

---

## Milestones

### v0.3.0-userland (August 2026) ⭐ NEW

**What was accomplished:**
- ✅ GDT management with user segments (DPL=3)
- ✅ User code (0x2B) and user data (0x33) segments
- ✅ TSS initialization for stack switching on interrupts
- ✅ `iretq`-based transition from kernel to user mode
- ✅ User code executes at CPL=3 with page protection
- ✅ User memory mapped with PT_USER flag for user/kernel isolation
- ✅ `create_user_process()` for launching user code
- ✅ Test commands: `simple`, `user`, `user2` for user mode verification

**Key learnings:**
- User mode requires proper GDT entries with DPL=3
- TSS must be configured for stack switching
- `iretq` is the correct way to transition from kernel to user mode
- PT_USER flag is essential for user memory access
- NX bit is the next logical step for security

**Known limitations:**
- NX bit not yet enabled — all pages are executable by default
- No system calls yet (user code can't request kernel services)

### v0.2.6-heap-stable (August 2026)

**What was accomplished:**
- ✅ `kfree()` fully implemented with free list
- ✅ Memory reuse verified (`heaptest` shows p3 == p1)
- ✅ `heaptest` command for testing allocation and reuse
- ✅ Heap allocator complete with both allocation and freeing
- ✅ Stable and tested across multiple runs

**Key learnings:**
- Free list is a simple but effective way to reuse memory
- Inline assembly can work around compiler issues
- Testing with `heaptest` verifies memory reuse

**Known limitations:**
- Identity mapping used (kernel memory visible)
- 256MB memory limit (bootloader constraint)

### v0.2.5-heap-working (August 2026)

**What was accomplished:**
- ✅ Heap allocator (`kmalloc`) working with bump allocator
- ✅ `heapstat` command for debugging
- ✅ 64MB initial heap with automatic expansion
- ✅ 256MB physical memory mapped
- ✅ Identity mapping for page table access
- ✅ All shell commands working and stable

**Key learnings:**
- Identity mapping works for page table access but limits to 256MB
- Bump allocator is simple but has memory leak (kfree stub)
- Heap expansion requires VMM mapping

**Known limitations:**
- Identity mapping used (kernel memory visible)
- 256MB memory limit (bootloader constraint)
- `kfree()` was a stub (resolved in v0.2.6)

### v0.2.3-vmm-stable (August 2026)

**What was accomplished:**
- ✅ Recursive paging implemented in bootloader
- ✅ VMM can read and write PML4 from higher-half kernel
- ✅ PML4[510] maps to itself for page table access
- ✅ HHDM region mapped at PML4[256]
- ✅ Dynamic page table allocation working
- ✅ Serial console fully integrated with QEMU
- ✅ Multiple QEMU run modes (serial, debug, headless, KVM)

**Key learnings:**
- Recursive paging is essential for accessing page tables from higher-half
- PMM must be initialized before VMM can allocate page tables
- Serial output is invaluable for debugging OS development

---

## Notes

The roadmap is intentionally incremental.  
Each milestone builds toward a fully functional x86_64 kernel while keeping the project educational and approachable.

MIT licensed — contributions welcome.
