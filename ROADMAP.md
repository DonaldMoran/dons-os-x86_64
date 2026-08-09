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
- Built‑in commands: `help`, `clear`, `info`, `mem`, `version`, `reboot`
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

### ☐ 3.3 — Virtual Memory Manager (Next)
- Dynamic page mapping  
- Page table allocation  
- Memory protection (R/W/X bits)

### ☐ 3.4 — Kernel Heap Allocator
- kmalloc() / kfree()  
- Slab or buddy allocator  
- Memory pools for small objects

### ☐ 3.5 — Scheduler Prototype
- Timer‑driven task switching  
- Process Control Block (PCB)  
- Cooperative or preemptive

### ☐ 3.6 — Userspace Support
- Ring 3 (user mode)  
- System call interface  
- ELF loader for user programs

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
- Serial/COM port  
- PCI enumeration  
- AHCI disk driver  
- PS/2 mouse

---

## 5. Development Tools

### ✔ QEMU Debug Mode
- `-d int,cpu_reset,guest_errors`  
- `-no-reboot -no-shutdown`  
- `-serial file:qemu.log`

### ☐ GDB Remote Debugging
- Add `-s -S`  
- Document breakpoints

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
| Exception Handlers | ✔ Complete (#DE, #PF working) |
| VMM | ☐ Next |
| Heap Allocator | ☐ Planned |
| Scheduler | ☐ Planned |
| Userspace | ☐ Planned |

---

## Notes

The roadmap is intentionally incremental.  
Each milestone builds toward a fully functional x86_64 kernel while keeping the project educational and approachable.

MIT licensed — contributions welcome.
