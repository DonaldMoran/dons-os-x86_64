# dons‑os  
### Educational x86_64 Boot Chain + 64‑bit Interrupt‑Driven Kernel (MIT Licensed)

**dons‑os** is a fully custom x86_64 operating system built from scratch, starting at the CPU's reset vector in **16‑bit real mode**, progressing through **32‑bit protected mode**, entering **64‑bit long mode**, and finally executing a **C‑based 64‑bit higher-half kernel** with working interrupts, timer, keyboard input, memory management, and a command shell.

The project emphasizes clarity, correctness, and educational value.  
Each stage is isolated, minimal, and fully bootable.

---

## 📁 Repository Structure

### Bootloaders
- **01_boot_16bit** — BIOS boot sector, INT 0x10 text, INT 0x13 disk loading  
- **02_boot_32bit** — A20 enable, GDT, protected mode, VGA text  
- **03_boot_64bit** — PAE paging, PML4/PDPT/PD/PT, IA32_EFER.LME, long‑mode entry  

### Kernel Development 
- **04_kernel_64bit** — Standalone 64‑bit kernel (ELF → flat), IDT, ISR stubs, PIC remap, PIT timer, IRQ0 tick, IRQ1 keyboard, PMM, VMM, VGA, serial, command shell, **heap allocator**, **system calls**, **ELF loader**, **process system foundation**, **process execution**, **cooperative scheduler**
- **05_boot_kernel64** — Full boot chain: stage2 loads kernel, enters long mode, jumps to `_start`

The top‑level Makefile builds and runs all components.

---

## 🚀 Building & Running

**Build everything**
```bash
make all
```

**Run individual boot demos**
```bash
make run16
make run32
make run64
```

**Build the 64‑bit kernel**
```bash
make kernel64
```

**Build + run the full long‑mode OS**
```bash
make bootkernel64
make runkernel64
```

**Run with QEMU debug logging**
```bash
make logkernel64
```

This boots:

 1. BIOS → stage1  
 2. stage1 loads stage2  
 3. stage2 builds page tables with **recursive mapping**  
 4. stage2 enters long mode  
 5. stage2 jumps to kernel at 0xFFFFFFFF80100000 (higher-half)  
 6. kernel executes `_start` → `kmain`  
 7. kernel initializes IDT, PIC, PIT, keyboard, PMM, VMM, Heap, Syscalls, **ELF loader**, **Process System**  
 8. kernel displays command prompt `>`  
 9. User can type commands and receive responses

---

### ELF Loader

The ELF loader is fully functional and can execute user programs from memory:

- ✅ Parses ELF64 headers and program headers
- ✅ Maps LOAD segments with correct permissions (Read, Write, Execute, User)
- ✅ Allocates and maps user stack pages
- ✅ Transitions to user mode via IRETQ with proper selectors (CS=0x33, SS=0x2B)
- ✅ Sets IOPL=3 for user I/O access
- ✅ Page table execute permissions at all levels (PML4 → PDPT → PD → PT)
- ✅ Uses HHDM for safe user‑space memory access from kernel
- ✅ **User programs loaded into dedicated user address space** (`USER_CODE_BASE = 0x0000008000000000`)
- ✅ Tested with "Hello from Userland!" output via serial

**User programs** can be embedded in the kernel and loaded with the `elfload` command.

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
| `vmmtest` | Test Virtual Memory Manager with HHDM |
| `serialtest` | Test serial output debugging |
| `heapstat` | Show heap statistics (used/free/total memory) |
| `maptest` | Test page mapping |
| `testrec` | Test recursive mapping address |
| `heaptest` | Test heap allocator with memory reuse |
| `nxtest` | Verify NX (No Execute) bit support |
| `syscall` | Test system call interface (SYS_WRITE, SYS_EXIT) |
| `elfload` | Load and run embedded ELF program from user mode Ring(3) |
| `proclist` | List all processes (idle + created) |
| `proccreate` | Create a test process (PCB infrastructure) |
| `vmmclone` | Clone the current page table (test process isolation) |
| `runproc` | Create and execute a test process |
| `schstat` | Show scheduler statistics |
| `testyield` | Test cooperative scheduling with yield |

---
```text
DonsDOS v0.4.5
Type 'help'
> help

Available commands:
  help       - Show this help
  clear      - Clear the screen
  version    - Show version info
  reboot     - Reboot the system
  pmmtest    - Test Physical Memory Manager (allocate/free pages)
  info       - Show boot information (PML4, kernel addresses, E820 entries)
  mem        - Show memory information (usable/reserved RAM)
  test       - Exception Handling test (#DE, #PF, #GP)
  vmmtest    - Show VMM status (recursive paging, HHDM, CR3, NX support)
  serialtest - Test serial output debugging
  heapstat   - Show heap statistics (used/free/total memory)
  maptest    - Test page mapping (allocate and write to a physical page)
  testrec    - Test recursive mapping address (read PML4 entry)
  heaptest   - Test heap allocator with memory reuse
  nxtest     - Test NX (No Execute) bit support
  syscall    - Test system calls (SYS_WRITE, SYS_EXIT)
  elfload    - Load and run embedded ELF program in user mode
  proclist   - List all processes (idle + created)
  proccreate - Create a test process (PCB infrastructure)
  vmmclone   - Clone the current page table (test process isolation)
  runproc    - Create and execute a test process
  schstat    - Show scheduler statistics
  testyield  - Test cooperative scheduling with yield
```

---

## 🐞 Debug Mode (QEMU)

Debugging early boot code is notoriously difficult.  
QEMU’s built‑in logging makes it dramatically easier to diagnose faults, paging issues, and incorrect mode transitions.

Run **any** boot stage in debug mode using:

### Quick debug run:
```bash
make logkernel64
```
### Manual debug command:
```bash
qemu-system-x86_64 \
  -drive file=hdd.img,format=raw \
  -serial stdio \
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
- **`-serial stdio`** — real-time serial debug output in your terminal

### 🧩 Useful for diagnosing

- invalid far jumps  
- incorrect segment selectors  
- paging faults  
- triple faults  
- CR0/CR4/EFER misconfiguration  
- long‑mode entry failures  

This debug mode was instrumental in getting the 64‑bit kernel working.

---

### Additional run modes

***Run with serial output to terminal (default)***
```bash
make runkernel64
```
***With serial output saved to file***
```bash
make runkernel64-log
```
***Run with GDB debug server***
```bash
make runkernel64-debug
```
***Run with verbose debug logging***
```bash
make runkernel64-verbose
```
***Run headless (no VGA window)***
```bash
make runkernel64-headless
```
***Run with KVM acceleration (faster)***
```bash
make runkernel64-kvm
```
---

## 🎓 Purpose

This project is designed to be:

- **Readable** — minimal, clean assembly and C  
- **Incremental** — each stage builds on the last  
- **Accurate** — follows x86_64 architectural rules  
- **Practical** — boots in QEMU with simple commands  
- **Educational** — a reference for anyone learning OS development  

---

## 🏷️ Tags & Milestones

- `v0.0.1-longmode` — First successful long-mode boot and flat binary kernel
- `v0.0.2-interrupts` — IDT, PIC remap, PIT timer, IRQ0 (tick), IRQ1 (keyboard)
- `v0.0.2-pmm-working` — PMM bitmap init fixed, E820 validated, allocator stable
- `v0.1-stable-keyboard` — Stable buffered keyboard (shift/caps/backspace/space), clean VGA console, correct IRQ handling
- `v0.1.1-shell` — Command shell, cursor control, improved console, bug fixes
- `v0.1.2-stable` — Full shell with PMM, info, mem commands, linker padding fix, and stable kernel
- `v0.2.0-higher-half` — Higher-half kernel transition complete (kernel runs at 0xFFFFFFFF80100000)
- `v0.2.1-exception-handlers` — All exception handlers working (#DE, #PF, #GP)
- `v0.2.2-vmm-working` — Virtual Memory Manager with HHDM, `vmmtest` command, serial debug output
- `v0.2.3-vmm-stable` — Recursive paging implemented, VMM can read/write PML4, stable HHDM mapping, serial console fully integrated
- `v0.2.5-heap-working` — Heap allocator (kmalloc) working, heapstat command, 256MB memory mapping
- `v0.2.6-heap-stable` — Heap fully working with kfree and memory reuse, free list implemented, heaptest command
- `v0.3.0-userland` — User mode (Ring 3) working, GDT with user segments, TSS stack switching, user code execution at CPL=3 with memory protection
- `v0.3.1-nx-support` — **NX (No Execute) bit support enabled**, PT_NX flag in VMM, `nxtest` command, heap WRITE bit fix, keyboard buffer corruption resolved
- `v0.3.2-syscalls` — **System call interface implemented** (SYS_WRITE, SYS_EXIT), SYSCALL/SYSRET support via MSRs, `syscall` test command
- `v0.4.0-elf-loader` — Fully functional ELF64 loader. Parses and maps ELF segments with correct user permissions, builds a user stack, transitions cleanly into Ring 3, executes embedded user programs (e.g., “Hello from Userland!”), and returns safely back to the Ring 0 shell via the syscall exit path. `elfload` command added.
- `v0.4.1-syscall-stack-stable` — Stabilized SYSRET return path, corrected RCX/R11 handling, removed `simple`, and verified clean returns from ELF Ring 3 programs to the Ring 0 shell.
- `v0.4.2-star-msr-fix` — **Fixed IA32_STAR MSR configuration** for SYSCALL/SYSRET. User CS = 0x30 → STAR[15:0] = 0x20. Enabled clean SYSRET return path.
- `v0.4.3-elfloader-fixed` — **ELF loader fully stabilized on first boot + Process Foundation.**  
  - Fixed bootloader identity‑mapping conflict (now detects and replaces bootloader mappings with proper user‑mode PTEs).  
  - Added safe HHDM‑based user‑space memory access in syscall handler (`safe_copy_from_user`).  
  - Corrected STAR MSR for SYSCALL/SYSRET (User CS = 0x30 → STAR[15:0] = 0x20).  
  - Added `PT_EXEC` (PWT bit) handling in `vmm_map_page()` to ensure user pages are executable.  
  - Verified `elfload` works reliably on the first boot (no more "run twice" bug).  
  - **Process Foundation:** Process Control Block (PCB) structure, `process_create()`, `proclist`, `proccreate`, `vmmclone` (page table cloning).  
  - **Dynamic HHDM mapping:** `ensure_hhdm_mapped()` for on‑demand physical memory access.  
  - **BootInfo validation:** Magic number and version checking.  
  - All existing commands remain fully functional.
- `v0.4.4-process-stacks` — **Process Stack Setup complete.**  
  - Added static kernel stack pool for processes.  
  - Process creation with dedicated user and kernel stacks.  
  - Process execution via direct function call (kernel mode).  
  - Process cleanup with `process_destroy()` (frees user stack, marks PCB unused).  
  - **`runproc` command** to create and execute a test process.  
  - Shell returns properly after process execution.  
  - All previous features (`proclist`, `proccreate`, `vmmclone`, `elfload`) remain fully functional.
- **`v0.4.5-cooperative-scheduler`** — **Cooperative Scheduler complete.**  
  - Ready queue with round‑robin scheduling.  
  - `process_yield()` for voluntary context switching.  
  - `process_exit()` for clean process termination.  
  - Assembly‑level context switching (`context_switch.asm`).  
  - **`testyield` command** to test cooperative scheduling.  
  - **`schstat` command** to show scheduler statistics.  
  - `runproc` now uses the scheduler.  
  - All previous features (`proclist`, `proccreate`, `vmmclone`, `elfload`) remain fully functional.

---

## 📌 Project Status (as of August 2026)

### ✅ Current Capabilities

**Boot & Architecture**
- ✅ Full boot chain: 16‑bit → 32‑bit → 64‑bit long mode
- ✅ Working GDT and TSS
- ✅ Higher-half kernel region (kernel runs at 0xFFFFFFFF80100000)
- ✅ **Recursive paging** at PML4[510] for page table access from higher-half
- ✅ 128MB physical memory detected and mapped (expandable via BootInfo)
- ✅ **User Mode (Ring 3) Support** — Full privilege separation with user code execution at CPL=3
- ✅ **Real user address space** — User programs loaded at `USER_CODE_BASE = 0x0000008000000000`

**Interrupts & Exceptions**
- ✅ Fully functional IDT and ISR stubs
- ✅ Stable IRQ0 (PIT timer) and IRQ1 (keyboard)
- ✅ #DE (Divide by Zero) handler working
- ✅ #PF (Page Fault) handler with CR2, ERR, RIP dump
- ✅ #GP (General Protection Fault) handler with ERR, RIP, CS dump
- ✅ Test command (`test`) for triggering all three exceptions

**Drivers**
- ✅ VGA text console (80×25) with scrolling and cursor control
- ✅ Keyboard driver with shift/caps/backspace support
- ✅ PIT timer incrementing `g_ticks`
- ✅ Serial (COM1) output for kernel debugging

**Shell / Console**
- ✅ Interactive prompt (`>`)
- ✅ Commands: help, clear, version, info, mem, reboot, pmmtest, test, vmmtest, serialtest, heapstat, maptest, testrec, heaptest, nxtest, **syscall**, **elfload**, **proclist**, **proccreate**, **vmmclone**, **runproc**, **schstat**, **testyield**
- ✅ Clean command parsing and line editing
- ✅ Unknown command handling with suggestions
- ✅ Serial console output (COM1) for debugging alongside VGA

### Memory Management

#### Physical Memory Manager (PMM)
- ✅ Parses BIOS E820 memory map
- ✅ Bitmap-based page allocator
- ✅ Tracks allocated and free pages
- ✅ Supports up to 128 MiB (expandable)

#### Virtual Memory Manager (VMM)
- ✅ **Recursive paging** at PML4[510]
- ✅ **HHDM** (`0xFFFF800000000000`) for physical memory access
- ✅ Dynamic page table allocation (PDPT → PD → PT)
- ✅ User page mapping with `PT_USER` flag
- ✅ **NX (No Execute) bit** support via `PT_NX`
- ✅ Page table entries correctly zeroed on allocation
- ✅ Proper present-bit checking in `vmm_is_mapped()`
- ✅ **User address space isolated from kernel identity map**

#### Heap Allocator
- ✅ `kmalloc()` and `kfree()` with free list
- ✅ Block headers for memory tracking
- ✅ Automatic heap expansion
- ✅ `heapstat` and `heaptest` debugging commands

**System Calls**
- ✅ **SYSCALL/SYSRET support** via MSR (IA32_STAR, IA32_LSTAR, IA32_FMASK)
- ✅ **SYS_WRITE** (syscall #1) — Writes to serial output, returns count
- ✅ **SYS_EXIT** (syscall #2) — Terminates process, prints status, returns to shell
- ✅ **Syscall dispatcher** with argument handling (x86_64 syscall ABI)
- ✅ **Test command** (`syscall`) to verify both system calls
- ✅ **Proper register preservation** across syscalls
- ✅ **Safe user‑space memory access** via `safe_copy_from_user()` using HHDM

**User Mode & Process System**
- ✅ GDT with user code (0x33) and user data (0x2B) segments (DPL=3)
- ✅ TSS configured for stack switching on interrupts from user mode
- ✅ `iretq`-based transition from kernel to user mode
- ✅ User code executes at CPL=3 with page protection
- ✅ **User programs in isolated address space** (separate from kernel identity map)
- ✅ **Process Control Block (PCB)** infrastructure
- ✅ **Page table cloning** (`vmmclone`) for process isolation
- ✅ **Process creation** (`proccreate`) and **listing** (`proclist`)
- ✅ **Process execution** (`runproc`) with dedicated stacks
- ✅ **Process cleanup** (`process_destroy`) with resource freeing
- ✅ Safe user‑space memory access via HHDM

**ELF Loader**
- ✅ Parses ELF64 headers and program headers
- ✅ Maps LOAD segments with correct permissions (Read, Write, Execute, User)
- ✅ Allocates and maps user stack pages
- ✅ Transitions to user mode via IRETQ with proper selectors (CS=0x33, SS=0x2B)
- ✅ Sets IOPL=3 for user I/O access
- ✅ Page table execute permissions at all levels (PML4 → PDPT → PD → PT)
- ✅ **`elfload` command** to load and run embedded ELF programs
- ✅ **Tested with "Hello from Userland!" output via serial**
- ✅ **Works reliably on first boot** (bootloader identity‑mapping handled)
- ✅ **User programs loaded at `USER_CODE_BASE = 0x0000008000000000`**

---

### Process System

The process system provides a foundation for multitasking:

- ✅ **Process Control Block (PCB)** with PID, state, and stack tracking
- ✅ **Process creation** with dedicated user and kernel stacks
- ✅ **Static kernel stack pool** for process execution
- ✅ **Process cleanup** with resource deallocation
- ✅ **`runproc` command** to create and execute test processes
- ✅ **Process listing** via `proclist`

### Scheduler
- ✅ **Cooperative scheduler** with ready queue and round‑robin scheduling
- ✅ **`process_yield()`** for voluntary context switching
- ✅ **`process_exit()`** for clean process termination
- ✅ **Assembly‑level context switching** (`context_switch.asm`)
- ✅ **`testyield` command** for testing cooperative scheduling
- ✅ **`schstat` command** for scheduler statistics

**Build System**
- Organized source tree with Makefile
- QEMU bootable disk image
- Clean Clang + NASM build
- Multiple QEMU run modes (serial, debug, headless, KVM)
- Debug logging support with serial console

---

## 🌱 Next Steps (Roadmap)

### Short-term (Next)
- 1. ~~**Cooperative scheduler** — Ready queue, `process_yield()`, round‑robin task switching~~ ✅
- 2. **Preemptive scheduler** — Timer interrupt integration, preemptive task switching
- 3. **User‑mode shell** — Move shell from Ring 0 to Ring 3 (Linux-style architecture)

### Medium-term
- 3. **Ring0 kernel threads** — Kernel daemons, system services
- 4. **Framebuffer graphics** — Move from VGA text mode to graphics

### Long-term
- 5. **File system** — Virtual File System (VFS) layer
- 6. **User‑space programs** — Build and run actual user applications

---

## 📜 License

This project is licensed under the **MIT License**.  
Use freely, modify freely, credit appreciated.
