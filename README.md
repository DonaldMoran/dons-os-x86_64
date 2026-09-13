# dons‑os  
### Educational x86_64 Boot Chain + 64‑bit Interrupt‑Driven Kernel (MIT Licensed)

**dons‑os** is a fully custom x86_64 operating system built from scratch, starting at the CPU's reset vector in **16‑bit real mode**, progressing through **32‑bit protected mode**, entering **64‑bit long mode**, and finally executing a **C‑based 64‑bit higher-half kernel** with working interrupts, timer, keyboard input, memory management, a preemptive round-robin scheduler, system calls, blocking I/O, a userland C library (newlib 4.x), and a Ring 3 user shell written in ordinary C.

The project emphasizes clarity, correctness, and educational value.  
Each stage is isolated, minimal, and fully bootable.

---

## 📁 Repository Structure

### Bootloaders
- **01_boot_16bit** — BIOS boot sector, INT 0x10 text, INT 0x13 disk loading  
- **02_boot_32bit** — A20 enable, GDT, protected mode, VGA text  
- **03_boot_64bit** — PAE paging, PML4/PDPT/PD/PT, IA32_EFER.LME, long‑mode entry  

### Kernel Development 
- **04_kernel_64bit** — Standalone 64‑bit kernel (ELF → flat), IDT, ISR stubs, PIC remap, PIT timer, IRQ0 tick, IRQ1 keyboard, PMM, VMM, VGA, serial, kernel shell, heap allocator, system calls, ELF loader, process system, preemptive scheduler, blocking I/O, GDT/TSS diagnostics
- **04_kernel_64bit/userland/newlib** — Userland C library (newlib 4.x), syscall shims, `crt0`, reentrancy support, and the user shell application
- **05_boot_kernel64** — Full boot chain: stage2 loads kernel via multi-pass segment incrementing, enters long mode, jumps to `_start`

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
4. stage2 reads the 64-bit kernel off disk in safe **128-sector chunks (64 KB steps)** to completely bypass real-mode address wrap-around ceilings
5. stage2 enters long mode  
6. stage2 jumps to kernel at 0xFFFFFFFF80100000 (higher-half)  
7. kernel executes `_start` → `kmain`  
8. kernel initializes IDT, PIC, PIT, keyboard, PMM, VMM, Heap, Syscalls, ELF loader, Process System, Preemptive Scheduler
9. kernel presents the **boot-time shell choice** (see below)
10. Either the kernel shell or the user shell starts, depending on the operator's key

---

## 🔀 Boot Flow

On boot, the kernel prints a boot prompt:

```
Boot: press 'k' for kernel shell, any other key for user shell...
```

- **Press `k`** within ~2 seconds → the **kernel shell** starts. It is a diagnostic console: it can list processes, run scheduler tests, dump the GDT and TSS, and launch the user shell via the `usershell` command.
- **Any other key, or no key within 2 seconds** → the **user shell** starts directly.

The user shell is the default. It runs as an ordinary user process (Ring 3, its own page tables, its own user and kernel stacks). Once the user shell is running, **the kernel shell is not reachable again without a reboot**. This is intentional: after boot, the user shell is the only interactive console.

If the user shell exits, `sys_exit` halts the CPU. There is no fallback to a kernel shell. Kernel diagnostic processes spawned by the kernel shell (`runproc`, `testyield`) do return to the kernel shell on exit, because that is where the operator needs to be.

---

## 📚 Userland C Library (newlib)

DonsDOS ships with **newlib 4.x** as its userland C library. User programs are ordinary C, compiled with the cross-compiler `x86_64-elf-gcc`, statically linked against `libc.a` and `libm.a`, and loaded from the kernel as ELF64 binaries.

This is a substantial capability: it means user programs can use the standard C library rather than a hand-rolled mini-libc. `printf`, `malloc`/`free`, `memcpy`, `str*`, `atoi`, and the rest are available in Ring 3.

### What is wired up

- **`crt0.S` (arc2)** — userland startup. Sets up the stack, initializes newlib's reentrancy structure, calls `main`, and invokes `exit` on return.
- **`syscalls.c` (arc2)** — the syscall shims that newlib's internals call (`write`, `read`, `sbrk`, `_exit`, `fstat`, `isatty`, `close`, `lseek`, `getpid`, `kill`). Each is a thin wrapper around the `syscall` instruction with the appropriate syscall number.
- **`reent.c` (arc2)** — newlib reentrancy support. Sets `_impure_ptr = &_impure_data` so that newlib's global state is valid at startup. (Without this, the first `printf` faults.)
- **`include/`** — newlib's headers, vendored. `stdio.h`, `stdlib.h`, `string.h`, `unistd.h`, etc.
- **`lib/libc.a`** and **`lib/libm.a`** — the compiled newlib libraries, statically linked into each user program.
- **`user_newlib_linker.ld`** — the linker script that places the user program at `USER_CODE_BASE = 0x8000000000` and sets up the ELF layout newlib expects.

### What works end to end

- ✅ `printf` — output reaches VGA and serial from Ring 3
- ✅ `malloc` / `free` — backed by `sbrk` → `sys_brk` → page mapping (see the heap test in the user shell, menu option 3)
- ✅ `memcpy`, `memset`, `strcmp`, and the rest of the string functions
- ✅ `setvbuf` — user shell sets `stdout` unbuffered so every `printf` immediately hits `sys_write`
- ✅ `errno` — newlib's error reporting is functional
- ✅ Reentrancy — newlib's per-thread state is initialized and used

### How to build a user program

User programs live in `04_kernel_64bit/userland/newlib/apps/`. The build chain:

1. `make -C 04_kernel_64bit/userland/newlib` compiles the app, links it against `libc.a`/`libm.a` with `user_newlib_linker.ld`, and produces an ELF.
2. The ELF is converted to a C array via `xxd -i` and embedded in the kernel image as `user_shell_data.c`.
3. The kernel loads the embedded ELF with `elf_load_into_process` and runs it as a Ring 3 process.

New apps can be added by dropping a `.c` file in `apps/`, adding it to the Makefile's source list, and giving the kernel a way to launch it (a new `kmain.c` command, or a menu option in the user shell).

### What's not there (yet)

- **No dynamic linking.** Programs are statically linked against `libc.a`. A shared library / dynamic loader would be a separate project.
- **No filesystem.** Programs are embedded in the kernel at build time; there's no way to load a program from disk yet.
- **A subset of newlib is exercised.** `stdio`, `stdlib` (`malloc`), `string`, and `unistd` are the primary use; `math.h` (`libm.a`) is linked but not exercised by anything in the tree. `signal`, `pthread`, `dirent`, and other subsystems are compiled in but untested on this kernel.

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
- ✅ **Works reliably on first boot** (bootloader identity‑mapping handled)

**User programs** can be embedded in the kernel and loaded with the `elfload` command.

---

### Kernel Shell

Reached by pressing `k` at the boot prompt. It provides a diagnostic console with a `>` prompt.

| Command | Description |
|---------|-------------|
| `help` | Show available commands |
| `clear` | Clear the screen |
| `version` | Show version information (`DonsDOS v0.4.7`) |
| `info` | Display system information (PML4, kernel addresses, E820 entries) |
| `mem` | Display memory information (usable/reserved RAM) |
| `reboot` | Reboot the system (Ring 0 supervisor sequence) |
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
| `elfload` | Load and run embedded ELF program from user mode Ring 3 |
| `proclist` | List all processes (idle + created) |
| `proccreate` | Create a test process (PCB infrastructure) |
| `vmmclone` | Clone the current page table (test process isolation) |
| `runproc` | Create and execute a test kernel process; returns to the kernel shell on exit |
| `schstat` | Show scheduler statistics |
| `testyield` | Cooperative yield test: two kernel processes alternate via `process_yield` |
| `usershell` | Launch the Ring 3 user shell |
| `gdtdump` | Decode and print the current GDT descriptors |
| `tssdump` | Print current TSS fields (rsp0, ist1–7, iopb_base, TR, g_syscall_stack_top) |

Example session:

```text
DonsDOS v0.4.7
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
  usershell  - Launch Ring 3 unprivileged shell interface
  gdtdump    - Decode and print the current GDT
  tssdump    - Print current TSS fields
```

---

### User Shell

Launched either by the boot-time default (no `k` key) or by typing `usershell` in the kernel shell.

It runs as an ordinary Ring 3 process using newlib. Its menu currently offers:

| Option | Description |
|--------|-------------|
| 1 | Print a message via printf (exercises the syscall path and newlib stdout) |
| 2 | Exit the user shell (halts the CPU; reboot to return) |
| 3 | Test malloc/free via newlib (`malloc`, write pattern, read back, `free`, second allocation) |

The heap test in option 3 exercises `malloc` → `sbrk` → `sys_brk` → page mapping → user write → user read, confirming that user pages are correctly mapped and writable.

---

## 🐞 Debug Mode (QEMU)

Debugging early boot code is notoriously difficult.  
QEMU's built‑in logging makes it dramatically easier to diagnose faults, paging issues, and incorrect mode transitions.

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
- **`-serial file:qemu.log`** — serial output saved to file
- **`-D qemu_debug.log`** — all debug output saved to file
- **`-serial stdio`** — real-time serial debug output in your terminal

### 🧩 Useful for diagnosing

- invalid far jumps  
- incorrect segment selectors  
- paging faults  
- triple faults  
- CR0/CR4/EFER misconfiguration  
- long‑mode entry failures  

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
- `v0.3.0-userland` — User mode (Ring 3) working, GDT with user segments, TSS configured for stack switching, user code execution at CPL=3 with memory protection
- `v0.3.1-nx-support` — **NX (No Execute) bit support enabled**, PT_NX flag in VMM, `nxtest` command, heap WRITE bit fix, keyboard buffer corruption resolved
- `v0.3.2-syscalls` — **System call interface implemented** (SYS_WRITE, SYS_EXIT), SYSCALL/SYSRET support via MSRs, `syscall` test command
- `v0.4.0-elf-loader` — Fully functional ELF64 loader. Parses and maps ELF segments with correct user permissions, builds a user stack, transitions cleanly into Ring 3, executes embedded user programs (e.g., "Hello from Userland!"), and returns safely back to the Ring 0 shell via the syscall exit path. `elfload` command added.
- `v0.4.1-syscall-stack-stable` — Stabilized SYSRET return path, corrected RCX/R11 handling, removed `simple`, and verified clean returns from ELF Ring 3 programs to the Ring 0 shell.
- `v0.4.2-star-msr-fix` — **Fixed IA32_STAR MSR configuration** for SYSCALL/SYSRET. User CS = 0x30 → STAR[15:0] = 0x20. Enabled clean SYSRET return path.
- `v0.4.3-elfloader-fixed` — **ELF loader fully stabilized on first boot + Process Foundation.**  
  - Fixed bootloader identity‑mapping conflict (now detects and replaces bootloader mappings with proper user‑mode PTEs).  
  - Added safe HHDM‑based user‑space memory access in syscall handler (`safe_copy_from_user`).  
  - Corrected STAR MSR for SYSCALL/SYSRET (User CS = 0x30 → STAR[15:0] = 0x20).  
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
- `v0.4.5-cooperative-scheduler` — **Cooperative Scheduler complete.**  
  - Ready queue with round‑robin scheduling.  
  - `process_yield()` for voluntary context switching.  
  - `process_exit()` for clean process termination.  
  - Assembly‑level context switching (`context_switch.asm`).  
  - **`testyield` command** to test cooperative scheduling.  
  - **`schstat` command** to show scheduler statistics.  
  - `runproc` now uses the scheduler.  
  - All previous features (`proclist`, `proccreate`, `vmmclone`, `elfload`) remain fully functional.
- `v0.4.6-preemptive-unlocked` — **Preemptive Scheduler & Unlocked Core capacity complete.**
  - **Multi-Pass Segment Reader:** Configured `stage2.asm` to pull kernel blocks in safe 128-sector chunks, advancing segment offsets dynamically to entirely defeat real-mode 64 KB wrap limits.
  - **Kernel Size Limit Lifted:** Expanded kernel disk read thresholds up to 256 sectors (128 KB allocation ceiling).
  - **Userland Reboot System Call:** Added system call #25 (`SYS_REBOOT`) to cleanly wire Ring 3 Userland Shell option 4 right back into a Ring 0 hardware triple-fault motherboard reset.
  - **Preemptive Core Integration:** Validated PIT clock timer integration (`IRQ0` at 100Hz) enforcing forceful quantum task slicing across ready queues.
- `v0.4.7-blocking-shell` ⭐ NEW — **newlib in userland, blocking reads, boot-time shell choice, userland heap test.**
  - **newlib 4.x linked into user programs**: `printf` reaches VGA and serial from Ring 3, `malloc`/`free` are backed by `sbrk` → `sys_brk`, newlib reentrancy is initialized at startup (`_impure_ptr = &_impure_data`). Userland C is now standard C.
  - **Kernel-stack-on-syscall-entry** (from A2 tag `20260912I`): the syscall path runs on a per-process kernel stack, not the user stack. Fixes a class of frame-corruption bugs and enables proper blocking.
  - **Blocking `sys_read`**: reads no longer spin the CPU in the kernel. A shell waiting for input marks itself BLOCKED, yields via the timer, and is woken by `irq1`.
  - **Boot-time shell choice**: a 2-second window at boot where `k` selects the kernel shell and any other key (or timeout) selects the user shell. The kernel shell is a diagnostic console; the user shell is the default.
  - **User shell is the terminal console**: once the user shell is running, the kernel shell is not reachable without a reboot. `sys_exit` on a user process halts the CPU.
  - **Kernel diagnostics return to the kernel shell**: `runproc`, `testyield`, and similar commands come back to the kernel shell when their process exits.
  - **Userland heap test**: user shell menu option 3 exercises newlib `malloc`/`free` over `sys_brk`, writing to newly-mapped user pages.
  - **`gdtdump` and `tssdump`**: on-demand kernel shell commands to inspect the GDT and TSS.
  - **`testyield` fix**: `process_yield` no longer corrupts the ready queue by calling `scheduler_ready_queue_remove` on an off-queue process. Both test processes now alternate cleanly.
  - **TSS.RSP0 and `g_syscall_stack_top` in lockstep from boot**: `process_init` runs after `tss_init` and sets `rsp0` to idle's kernel stack top.
  - All prior commands and features remain functional.

---

## 📌 Project Status (as of September 2026)

### ✅ Current Capabilities

**Boot & Architecture**
- ✅ Full boot chain: 16‑bit → 32‑bit → 64‑bit long mode
- ✅ Working GDT and TSS
- ✅ Higher-half kernel region (kernel runs at 0xFFFFFFFF80100000)
- ✅ **Recursive paging** at PML4[510] for page table access from higher-half
- ✅ 128MB physical memory detected and mapped (expandable via BootInfo)
- ✅ **User Mode (Ring 3) Support** — Full privilege separation with user code execution at CPL=3
- ✅ **Real user address space** — User programs loaded at `USER_CODE_BASE = 0x0000008000000000`
- ✅ **Boot-time choice between kernel shell and user shell**

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

**Shells / Console**
- ✅ Kernel shell (diagnostic, reached via `k` at boot) with commands including `gdtdump` and `tssdump`
- ✅ User shell (Ring 3, newlib) as the default interactive console
- ✅ Unknown command handling
- ✅ Serial console output (COM1) for debugging alongside VGA

**Userland C Library (newlib)**
- ✅ **newlib 4.x** linked into user programs
- ✅ `printf` / `fprintf` / `puts` work from Ring 3
- ✅ `malloc` / `free` backed by `sbrk` → `sys_brk`
- ✅ `memcpy`, `memset`, `str*`, and the rest of the standard C string functions
- ✅ newlib reentrancy initialized at startup (`_impure_ptr = &_impure_data`)
- ✅ Statically linked (`libc.a`, `libm.a`); no dynamic linking yet
- ✅ User programs written in ordinary C, compiled with `x86_64-elf-gcc`, loaded from the kernel ELF image

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
- ✅ **Userland heap via newlib `malloc`/`free` over `sys_brk`**, exercised by user shell menu option 3

**System Calls**
- ✅ **SYSCALL/SYSRET support** via MSR (IA32_STAR, IA32_LSTAR, IA32_FMASK)
- ✅ **SYS_WRITE** (syscall #1) — Writes to serial and VGA output, returns count
- ✅ **SYS_EXIT** (syscall #2) — Terminates the current process. Kernel diagnostic processes return to the kernel shell; user processes halt the CPU
- ✅ **SYS_READ** (syscall #3) — Blocking read from fd 0; see Blocking I/O below
- ✅ **SYS_BRK** (syscall #10) — Grow or shrink the process heap
- ✅ **Syscall dispatcher** with argument handling (x86_64 syscall ABI)
- ✅ **Proper register preservation** across syscalls
- ✅ **Safe user‑space memory access** via `safe_copy_from_user()` / `safe_copy_to_user()` using HHDM

### Blocking I/O

`sys_read` on fd 0 does not spin the CPU. When the keyboard buffer is empty:

1. The calling process sets its state to `PROC_STATE_BLOCKED` and executes `sti; hlt`.
2. The next timer tick sees the blocked state and does not re-add the process to the ready queue; it switches to another runnable process (or idle).
3. When a key arrives, `irq1_handler` puts the byte in the shared buffer and wakes all blocked processes (BLOCKED → READY, back on the ready queue).
4. The timer resumes the process via its saved kernel frame; `sys_read` rechecks the buffer, consumes the byte, and returns.

This means a shell waiting for input does not monopolize the CPU. Other processes — including idle — run while the shell is blocked.

**Keyboard buffer is shared.** If multiple processes are blocked in `read`, whichever wakes first and is scheduled first consumes the byte. A per-process tty layer would be required to route input to a specific shell.

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
- ✅ **`process_exit`** distinguishes kernel diagnostics (return to kernel shell) from user processes (halt)
- ✅ Safe user‑space memory access via HHDM

### Process System

- ✅ **Process Control Block (PCB)** with PID, state, and stack tracking
- ✅ **Process creation** with dedicated user and kernel stacks
- ✅ **Static kernel stack pool** for process execution
- ✅ **`process_exit`** with mode-dependent fallback (kernel shell or halt)
- ✅ **`runproc` command** to create and execute test kernel processes
- ✅ **Process listing** via `proclist`

### Scheduler

- ✅ **Cooperative scheduler** with ready queue and round‑robin scheduling
- ✅ **Preemptive scheduler** driven by the PIT timer (100 Hz)
- ✅ **`process_yield()`** for voluntary context switching
- ✅ **`process_exit()`** for clean process termination
- ✅ **Assembly‑level context switching** (`context_switch.asm`)
- ✅ **`testyield`** now alternates cleanly between both test processes
- ✅ **`schstat`** for scheduler statistics
- ✅ **Timer preempts kernel-mode processes** (except idle), so a process blocked in `sys_read` does not hold the CPU

**Build System**
- Organized source tree with Makefile
- QEMU bootable disk image
- Clean Clang + NASM build
- Multiple QEMU run modes (serial, debug, headless, KVM)
- Debug logging support with serial console

---

## ⚠️ Known Limitations

- **`runproc` and `testyield` leak PCB slots.** Their cleanup code after `scheduler_switch_to` is unreachable, because `scheduler_switch_to` never returns to its caller when the switched-to process exits. Running these diagnostics many times in one boot will exhaust the 32-slot PCB pool. A proper fix would have `process_exit` reclaim the exiting process's PCB instead of leaving it `TERMINATED`.
- **`testyield` is timing-sensitive.** If the timer preempts during `testyield`'s setup (between the `process_create` calls and the initial `scheduler_switch_to`), the ready queue state at the first yield can differ. On a fresh boot it works reliably; after a sequence of other process-creating commands in the same boot it may misbehave. Reboot to reset.
- **The keyboard buffer is shared.** Multiple shells reading from fd 0 will compete for bytes. Each keystroke goes to whichever blocked process the scheduler picks first. A per-process tty or a console-focus mechanism would be required to make multiple shells usable side by side.
- **`sys_brk`'s `heap_base` is a single constant.** Each process's heap starts at the same *virtual* address (`0x8000200000`) and grows in its own address space (different `cr3`), so there is no address conflict. The shared constant is a code-cleanliness issue, not a functional one.
- **`vmm_map_page_in_cr3` does not flush the TLB.** Callers must `invlpg` after mapping if the address may have a stale translation. `sys_brk` does this; new callers should too.
- **No filesystem yet.** All user programs are embedded in the kernel ELF at build time and loaded from memory.
- **A subset of newlib is exercised.** `stdio`, `stdlib` (`malloc`), `string`, and `unistd` are the primary use; `math.h` (`libm.a`) is linked but not exercised. `signal`, `pthread`, `dirent`, and other subsystems are compiled in but untested on this kernel.

---

## 🌱 Next Steps (Roadmap)

### Short-term (Next)
- ~~Cooperative scheduler~~ ✅
- ~~Preemptive scheduler~~ ✅
- ~~User-mode shell~~ ✅
- ~~Blocking reads~~ ✅
- ~~Userland heap via sys_brk (newlib malloc)~~ ✅
- ~~newlib 4.x linked into user programs~~ ✅
- **Kernel-shell re-entry** — a way to reach the kernel shell after boot without a full reboot (serial console or a gated debug flag)
- **Process cleanup on exit** — reclaim PCBs so diagnostics don't leak

### Medium-term
- ~~Ring0 kernel threads~~ ✅
- **Dynamic linking** — a userland ELF loader so programs don't need to be statically linked
- **Framebuffer graphics** — move from VGA text mode to graphics
- **Per-process tty / console focus** — prerequisite for multiple concurrent shells

### Long-term
- **File system** — Virtual File System (VFS) layer, starting with FAT
- **User-space programs from disk** — once there's a filesystem, load programs from it instead of embedding them

---

## 📜 License

This project is licensed under the **MIT License**.  
Use freely, modify freely, credit appreciated.
