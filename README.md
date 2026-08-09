# dons‑os  
### Educational x86_64 Boot Chain + 64‑bit Interrupt‑Driven Kernel (MIT Licensed)

**dons‑os** is a fully custom x86_64 operating system built from scratch, starting at the CPU’s reset vector in **16‑bit real mode**, progressing through **32‑bit protected mode**, entering **64‑bit long mode**, and finally executing a **C‑based 64‑bit higher-half kernel** with working interrupts, timer, keyboard input, memory management, and a command shell.

The project emphasizes clarity, correctness, and educational value.  
Each stage is isolated, minimal, and fully bootable.

---

## 📁 Repository Structure

### Bootloaders
- **01_boot_16bit** — BIOS boot sector, INT 0x10 text, INT 0x13 disk loading  
- **02_boot_32bit** — A20 enable, GDT, protected mode, VGA text  
- **03_boot_64bit** — PAE paging, PML4/PDPT/PD/PT, IA32_EFER.LME, long‑mode entry  

### Kernel Development
- **04_kernel_64bit** — Standalone 64‑bit kernel (ELF → flat), IDT, ISR stubs, PIC remap, PIT timer, IRQ0 tick, IRQ1 keyboard, PMM, VGA, command shell  
- **05_boot_kernel64** — Full boot chain: stage2 loads kernel, enters long mode, jumps to `_start`

The top‑level Makefile builds and runs all components.

---

## 🚀 Building & Running

### Build everything
```bash
make all
```

### Run individual boot demos
```
make run16
make run32
make run64
```

### Build the 64‑bit kernel
```
make kernel64
```

### Build + run the full long‑mode OS
```
make bootkernel64
make runkernel64
```

### Run with QEMU debug logging
```
make logkernel64
```

This boots:

 1. BIOS → stage1  
 2. stage1 loads stage2  
 3. stage2 builds page tables  
 4. stage2 enters long mode  
 5. stage2 jumps to kernel at 0xFFFFFFFF80100000 (higher-half)  
 6. kernel executes `_start` → `kmain`  
 7. kernel prints boot banner and system info
 8. kernel initializes IDT, PIC, PIT, keyboard
 9. kernel displays command prompt >
10. User can type commands and receive responses

---
### Command Shell

Once booted, you'll see a prompt > where you can type commands:
| Command | Description |
|---------|-------------|
| `help` | Show available commands |
| `clear` | Clear the screen |
| `version` | Show version information |
| `info` | Display system information (PML4, kernel addresses, E820 entries) |
| `mem` | Display memory statistics (usable/reserved RAM) |
| `reboot` | Reboot the system |
| `pmmtest` | Test Physical Memory Manager |
| `test` | Test exception handlers (#DE, #PF, #GP) |

```
DonsDOS v0.1
Type 'help'
> help

Available commands:
  help     - Show this help
  clear    - Clear the screen
  version  - Show version info
  reboot   - Reboot the system
  pmmtest  - Test Physical Memory Manager
  info     - Show boot information
  mem      - Show memory information
> pmmtest

PMM Test:
  Page1: 0x0000000000108000
  Page2: 0x0000000000109000
  Page3: 0x000000000010A000
  Freed page2
  Page4: 0x0000000000109000
Test complete.
> 
```
---

## 🐞 Debug Mode (QEMU)

Debugging early boot code is notoriously difficult.  
QEMU’s built‑in logging makes it dramatically easier to diagnose faults, paging issues, and incorrect mode transitions.

Run **any** boot stage in debug mode using:

### Quick debug run:
```
make logkernel64
```
### Manual debug command:
```
qemu-system-x86_64 \
  -drive file=hdd.img,format=raw \
  -d int,cpu_reset \
  -no-reboot \
  -no-shutdown
```

### 🔍 What this enables

- **`-d int`** — logs all CPU interrupts (hardware + software)
- **`-d cpu_reset`** — logs CPU resets (critical for diagnosing triple faults)
- **`-d guest_errors`** — logs guest errors (page faults, etc.)
- **`-d page`** — logs page faults
- **`-no-reboot`** — prevents QEMU from instantly restarting on a fault
- **`-no-shutdown`** — keeps QEMU open so you can read the debug output
- **`-serial file:qemu.log**` — serial output saved to file
- **`-D qemu_debug.log**` — all debug output saved to file

### 🧩 Useful for diagnosing

- invalid far jumps  
- incorrect segment selectors  
- paging faults  
- triple faults  
- CR0/CR4/EFER misconfiguration  
- long‑mode entry failures  

This debug mode was instrumental in getting the 64‑bit kernel working.

---

## 🎓 Purpose

This project is designed to be:

- **Readable** — minimal, clean assembly and C  
- **Incremental** — each stage builds on the last  
- **Accurate** — follows x86_64 architectural rules  
- **Practical** — boots in QEMU with simple commands  
- **Educational** — a reference for anyone learning OS development  

---

## 🌱 Tags & Milestones

- `**v0.0.1-longmode**`	      First successful long-mode boot and flat binary kernel
- `**v0.0.2-interrupts**`	    IDT, PIC remap, PIT timer, IRQ0 (tick), IRQ1 (keyboard)
- `**v0.0.2-pmm-working**`	  PMM bitmap init fixed, E820 validated, allocator stable
- `**v0.1-stable-keyboard**`	Stable buffered keyboard (shift/caps/backspace/space), clean VGA console, correct IRQ handling
- `v0.1.1-shell**`	          Command shell, cursor control, improved console, bug fixes
- `v0.1.2-stable` 	Full shell with PMM, info, mem commands, linker padding fix, and stable kernel
- `v0.2.0-higher-half`	Higher-half kernel transition complete (kernel runs at 0xFFFFFFFF80100000)
- `v0.2.1-exception-handlers`	All exception handlers working (#DE, #PF, #GP)

---

## 📌 Project Status (as of August 2026)

### ✅ Current Capabilities

**Boot & Architecture**
- Full boot chain: 16‑bit → 32‑bit → 64‑bit long mode
- Working GDT and TSS
- Higher-half kernel region (kernel runs at 0xFFFFFFFF80100000)

**Interrupts & Exceptions**
- Fully functional IDT and ISR stubs
- Stable IRQ0 (PIT timer) and IRQ1 (keyboard)
- ✅ #DE (Divide by Zero) handler working
- ✅ #PF (Page Fault) handler with CR2, ERR, RIP dump
- ✅ #GP (General Protection Fault) handler with ERR, RIP, CS dump
- Test command (`test`) for triggering all three exceptions

**Drivers**
- VGA text console (80×25) with scrolling and cursor control
- Keyboard driver with shift/caps/backspace support
- PIT timer incrementing `g_ticks`

**Shell / Console**
- Interactive prompt (`>`)
- Commands: `help`, `clear`, `version`, `info`, `mem`, `reboot`, `pmmtest`
- Clean command parsing and line editing

**Memory**
- Full BIOS E820 memory map parsing
- Memory map passed to kernel via BootInfo
- Physical Memory Manager (PMM) with bitmap allocator
- Page allocation, freeing, and reuse verified

**Build System**
- Organized source tree with Makefile
- QEMU bootable disk image
- Clean Clang + NASM build
- Debug logging support

## 🌱 Next Steps (Roadmap)

### Short-term
1. ~~**Higher‑half kernel** — Map kernel to `0xFFFFFFFF80000000`~~ ✅ COMPLETED (kernel now runs at `0xFFFFFFFF80100000`)
2. ~~**Exception handlers** — Page fault, GPF, double fault with register dumps~~ ✅ COMPLETED (#DE and #PF working, #GP partially working)
3. **Virtual memory manager** — Dynamic page tables, map/unmap
4. **Heap allocator** — `kmalloc`/`kfree` implementation

### Long-term
4. **Scheduler** — Task switching (cooperative → preemptive)
5. **Framebuffer graphics** — Move from VGA text mode to graphics
6. **ELF loader** — Load and execute user programs
7. **User‑space processes** — Process model, isolation, syscalls

---

## 📜 License

This project is licensed under the **MIT License**.  
Use freely, modify freely, credit appreciated.
