# dons‑os  
### Educational x86_64 Boot Chain + 64‑bit Interrupt‑Driven Kernel (MIT Licensed)

**dons‑os** is a fully custom x86_64 operating system built from scratch, starting at the CPU's reset vector in **16‑bit real mode**, progressing through **32‑bit protected mode**, entering **64‑bit long mode**, and finally executing a **C‑based 64‑bit higher-half kernel** with working interrupts, timer, keyboard input, memory management, a preemptive round-robin scheduler, system calls, blocking I/O, a userland C library (newlib 4.x), a FAT16 filesystem, and a Ring 3 user shell written in ordinary C.

Related documents:
- [`ROADMAP.md`](ROADMAP.md) — planned features and completed milestones
- [`OSDev_Checklist.md`](OSDev_Checklist.md) — capability tracking
- [`MAINTENANCE.md`](MAINTENANCE.md) — known debt, latent bugs, and cleanup work

---

## 📁 Repository Structure

### Bootloaders
- **01_boot_16bit** — BIOS boot sector, INT 0x10 text, INT 0x13 disk loading  
- **02_boot_32bit** — A20 enable, GDT, protected mode, VGA text  
- **03_boot_64bit** — PAE paging, PML4/PDPT/PD/PT, IA32_EFER.LME, long‑mode entry  

### Kernel Development
- **04_kernel_64bit** — Standalone 64‑bit kernel (ELF → flat), IDT, ISR stubs, PIC remap, PIT timer, IRQ0 tick, IRQ1 keyboard, PMM, VMM, VGA, serial, kernel shell, heap allocator, system calls, ELF loader, process system, preemptive scheduler, blocking I/O, ATA PIO driver, FatFs integration, GDT/TSS diagnostics
- **04_kernel_64bit/userland/newlib** — Userland C library (newlib 4.x), syscall shims, `crt0`, reentrancy support, and the user shell application
- **04_kernel_64bit/fatfs** — Vendored FatFs R0.15 plus the `diskio.c` shim that maps FatFs onto the ATA PIO driver
- **04_kernel_64bit/include/fat_config.h** — The single compile-time switch that selects dual-drive vs single-drive storage
- **05_boot_kernel64** — Full boot chain: stage2 loads kernel via multi-pass segment incrementing, enters long mode, jumps to `_start`
- **test-files** — Files copied into the single-drive FAT partition at image-build time
- **run** — One-line-per-option QEMU launch script; uncomment the option you want and run `./run`

The top‑level Makefile builds and runs all components.

---

## 🚀 Building & Running

**Build everything**
```bash
make all
```

**Build + run the full long‑mode OS (dual-drive, the default)**

```bash
make bootkernel64
make runkernel64
```

**Build + run the single-drive layout**

```bash
make clean && make FAT_CONFIG=single && make runkernel64-kvm-single
```

**Or, with the convenience script at the repo root:**

Edit ./run, uncomment one line in the menu, save, then:

```bash
./run
```

The script parses its own menu() function and executes the single uncommented line. To switch tasks, move the # from one line to another. Never uncomment two lines at once. It runs a full make clean && make and boots the selected target — one edit, one command.

***Run individual boot demos***

```bash
make run16
```

```bash
make run32
```

```bash
make run64
```
Run with QEMU debug logging

```bash
make logkernel64
```

This boots:
    
     1. BIOS → stage1
     2. stage1 loads stage2
     3. stage2 builds page tables with recursive mapping
     4. stage2 reads the 64-bit kernel off disk in safe 128-sector chunks (64 KB steps) to completely bypass real-mode address wrap-around 
        ceilings
     5. stage2 enters long mode
     6. stage2 jumps to kernel at 0xFFFFFFFF80100000 (higher-half)
     7. kernel executes _start → kmain
     8. kernel initializes IDT, PIC, PIT, keyboard, PMM, VMM, Heap, ATA, FatFs, Syscalls, ELF loader, Process System, Preemptive Scheduler
     9. kernel presents the boot-time shell choice (see below)
    10.Either the kernel shell or the user shell starts, depending on the operator's key

---

## 🔀 Boot Flow

On boot, the kernel prints a boot prompt:

```
Boot: press 'k' for kernel shell, any other key for user shell...
```

- **Press `k`** within ~2 seconds → the **kernel shell** starts. It is a diagnostic console: it can list processes, run scheduler tests, dump the GDT and TSS, list the FAT volume, and launch the user shell via the `usershell` command.
- **Any other key, or no key within 2 seconds** → the **user shell** starts directly.

The user shell is the default. It runs as an ordinary user process (Ring 3, its own page tables, its own user and kernel stacks). Once the user shell is running, **the kernel shell is not reachable again without a reboot**. This is intentional: after boot, the user shell is the only interactive console.

If the user shell exits, `sys_exit` halts the CPU. There is no fallback to a kernel shell. Kernel diagnostic processes spawned by the kernel shell (`runproc`, `testyield`) do return to the kernel shell on exit, because that is where the operator needs to be.

---

## 💾 Two Storage Configurations

The kernel supports two disk layouts. They are mutually exclusive at build time; the choice is a single make variable.

**Dual-drive (default)**

```table
hdd.img (master)              fat.img (slave)
───────────────               ───────────────
LBA 0..127   boot chain       LBA 0..      FAT16 volume
LBA 128..    kernel
```

The boot chain and kernel occupy the master disk. FatFs lives on a separate slave disk, a whole-disk FAT16 volume with no partition table. This layout is easy to debug because the OS disk and the data disk are physically separate.
bash

make clean && make && make runkernel64-kvm

**Single-drive**

```table
hdd.img (master only)
─────────────────────
LBA 0..127       boot chain
LBA 128..2047    kernel (up to 960 KB)
LBA 2048..       FAT16 partition (hidden_sectors = 2048)
```

Everything lives on one disk. The FAT16 partition starts at LBA 2048. FatFs talks to the master drive and diskio.c adds the partition offset to every sector number it hands to the ATA layer.
bash

```bash
make clean && make FAT_CONFIG=single && make runkernel64-kvm-single
```

**The switch**

FAT_CONFIG=dual (default) or FAT_CONFIG=single selects the configuration. The Makefile passes -DFAT_CONFIG_SINGLE_DRIVE=0|1, and include/fat_config.h turns that into:

    FAT_DRIVE — ATA_DRIVE_SLAVE in dual, ATA_DRIVE_MASTER in single.
    FAT_PARTITION_OFFSET (in diskio.c) — 0 in dual, 2048 in single.
    FAT_VOLUME_SECTORS — the size FatFs reports for free-space accounting.

The kernel source is identical between the two configurations. Only the drive selection and partition offset differ.

### Why the offset lives in diskio.c, not the BPB

FF_MULTI_PARTITION is 0 in ffconf.h, which means FatFs is not partition-aware. It treats the FAT volume as starting at LBA 0 of the physical drive and does not consult the partition table or the BPB's hidden_sectors field. diskio.c is therefore the correct place to translate volume-relative sector numbers into device LBAs.

An earlier iteration assumed FatFs would read hidden_sectors and skipped the offset. The mount then failed with FR_NO_FILESYSTEM (13), because FatFs read the boot sector at LBA 0 (which is the boot chain, not a FAT boot sector) instead of the FAT boot sector at LBA 2048. The offset must be applied in diskio.c for a partitioned volume with FF_MULTI_PARTITION = 0.

### Design note: why the kernel isn't in the FAT filesystem

A reader may wonder why the kernel lives at a fixed LBA rather than as a file in the FAT partition.

The boot chain predates FatFs by roughly ten commits. Adding a FAT reader to stage2.asm would have meant writing a FAT12/16/32 reader in 16-bit real mode, in a 34 KB budget, without a heap. That was a multi-week detour for zero new features. The kernel is at a fixed LBA because the boot chain loads it before any driver exists — this is what every BIOS-boot OS does. FatFs is for data: test files now, user programs later. The kernel's existence is a bootloader concern, not a filesystem concern.

If the project ever moves to Limine or GRUB2, the kernel becomes a file loaded by the bootloader, and this distinction disappears. It is on the roadmap but not urgent.

---

## 📚 Userland C Library (newlib)

DonsDOS ships with **newlib 4.x** as its userland C library. User programs are ordinary C, compiled with the cross-compiler `x86_64-elf-gcc`, statically linked against `libc.a` and `libm.a`, and loaded from the kernel as ELF64 binaries.

This is a substantial capability: it means user programs can use the standard C library rather than a hand-rolled mini-libc. `printf`, `malloc`/`free`, `memcpy`, `str*`, `atoi`, and the rest are available in Ring 3.

### What is wired up

- **`crt0.S` (arc2)** — userland startup. Sets up the stack, initializes newlib's reentrancy structure, calls `main`, and invokes `exit` on return.
- **`syscalls.c` (arc2)** — the syscall shims that newlib's internals call (write, read, sbrk, _exit, fstat, isatty, open, close, lseek, getpid, kill). Each is a thin wrapper around the syscall instruction with the appropriate syscall number.
- **`reent.c` (arc2)** — newlib reentrancy support. Sets `_impure_ptr = &_impure_data` so that newlib's global state is valid at startup. (Without this, the first `printf` faults.)
- **`include/`** — newlib's headers, vendored. `stdio.h`, `stdlib.h`, `string.h`, `unistd.h`, etc.
- **`lib/libc.a`** and **`lib/libm.a`** — the compiled newlib libraries, statically linked into each user program.
- **`user_newlib_linker.ld`** — the linker script that places the user program at `USER_CODE_BASE = 0x8000000000` and sets up the ELF layout newlib expects.

### What works end to end

- ✅ printf — output reaches VGA and serial from Ring 3
- ✅ malloc / free — backed by sbrk → sys_brk → page mapping (see the heap test in the user shell, menu option 3)
- ✅ memcpy, memset, strcmp, and the rest of the string functions
- ✅ setvbuf — user shell sets stdout unbuffered so every printf immediately hits sys_write
- ✅ errno — newlib's error reporting is functional
- ✅ Reentrancy — newlib's per-thread state is initialized and used
- ✅ open / close / read / write on FAT files from Ring 3, via syscalls 3, 4, 6, and 1

### How to build a user program

User programs live in `04_kernel_64bit/userland/newlib/apps/`. The build chain:

1. `make -C 04_kernel_64bit/userland/newlib` compiles the app, links it against `libc.a`/`libm.a` with `user_newlib_linker.ld`, and produces an ELF.
2. The ELF is converted to a C array via `xxd -i` and embedded in the kernel image as `user_shell_data.c`.
3. The kernel loads the embedded ELF with `elf_load_into_process` and runs it as a Ring 3 process.

New apps can be added by dropping a `.c` file in `apps/`, adding it to the Makefile's source list, and giving the kernel a way to launch it (a new `kmain.c` command, or a menu option in the user shell).

### What's not there (yet)

- **No dynamic linking.** Programs are statically linked against `libc.a`. A shared library / dynamic loader would be a separate project.
- **No filesystem for program loading.** The kernel has FatFs, but user programs are still embedded in the kernel image at build time. Loading them from disk is the next storage milestone.
- **A subset of newlib is exercised.** `stdio`, `stdlib` (`malloc`), `string`, `unistd`, and `fcntl` are the primary use; `math.h` (`libm.a`) is linked but not exercised by anything in the tree. `signal`, `pthread`, `dirent`, and other subsystems are compiled in but untested on this kernel.

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
| `version` | Show version information (DonsDOS v0.5.0) |
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
| `atatest` | Read-only ATA diagnostic: MBR signature, kernel header, model strings |
| `fatmount` | Mount the FAT volume and report status |
| `fatls` | List the root directory of the FAT volume |
| `fatcat <file>` | Dump the contents of a FAT file to VGA and serial |

Example session:

```text
DonsDOS v0.5.0 
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
  atatest    - ATA read-only diagnostic
  fatmount   - Mount the FAT volume
  fatls      - List the root directory
  fatcat     - Dump a file's contents (usage: fatcat <file>)
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
| 4 | Test FatFs: create, write, close, reopen, read back (round-trip byte-exact) |
| 5 | Persistence check: verify USER.TXT written by a previous boot survived a reboot |
| 6 | Multi-file test: create 3 files, verify contents (deletion is skipped; no SYS_UNLINK yet) |
| 7 | Large write test: 4 KB round-trip through FatFs, catches multi-cluster bugs |
| 8 | List known files: probe a fixed set of names and report which exist, with a short preview |
| 9 | Reboot: call `SYS_REBOOT`, which flushes file handles and fires a hardware reset |

- Option 3 exercises malloc → sbrk → sys_brk → page mapping → user write → user read, confirming that user pages are correctly mapped and writable.
- Option 4 exercises open → write → close → open → read on the FAT volume from Ring 3. The round-trip is byte-exact.
- Option 5 is the strongest single test in the tree. It writes on boot N, and on boot N+1 (same image) it verifies the file is present and byte-exact. This is what proves writes are durable, not just buffered. Combined with option 9, the persistence check is a three-step sequence — write, reboot, verify — that can be run entirely from the user shell.
- Option 8 is a poor-man's `ls`: since there is no directory-iteration syscall yet, it probes a fixed set of filenames and reports which ones open successfully. A real `ls` needs `SYS_OPENDIR` / `SYS_READDIR` / `SYS_CLOSEDIR`.
- Option 9 calls `sys_reboot()`, which does `fflush(NULL)` from userland, then invokes `SYS_REBOOT`. The kernel closes every open file handle in the current process's file table (`f_close` triggers `f_sync` → `FLUSH CACHE`), then fires the hardware reset. Ring 3 reboot is a convenience for testing; a production OS would restrict it to a privileged process.

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
***Run single-drive with KVM***
```bash
make runkernel64-kvm-single
```
***Run single-drive under TCG (no KVM)***
```bash
make runkernel64-single
```
***Telnet serial console (connect with telnet localhost 4444)***
```bash
make runkernel64-telnet
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
- `v0.4.2` — **Fixed IA32_STAR MSR configuration** for SYSCALL/SYSRET. User CS = 0x30 → STAR[15:0] = 0x20. Enabled clean SYSRET return path.
- `v0.4.3` — **ELF loader fully stabilized on first boot + Process Foundation.**  
  - Fixed bootloader identity‑mapping conflict (now detects and replaces bootloader mappings with proper user‑mode PTEs).  
  - Added safe HHDM‑based user‑space memory access in syscall handler (`safe_copy_from_user`).  
  - Corrected STAR MSR for SYSCALL/SYSRET (User CS = 0x30 → STAR[15:0] = 0x20).  
  - Verified `elfload` works reliably on the first boot (no more "run twice" bug).  
  - **Process Foundation:** Process Control Block (PCB) structure, `process_create()`, `proclist`, `proccreate`, `vmmclone` (page table cloning).  
  - **Dynamic HHDM mapping:** `ensure_hhdm_mapped()` for on‑demand physical memory access.  
  - **BootInfo validation:** Magic number and version checking.  
  - All existing commands remain fully functional.
- `v0.4.4` — **Process Stack Setup complete.**  
  - Added static kernel stack pool for processes.  
  - Process creation with dedicated user and kernel stacks.  
  - Process execution via direct function call (kernel mode).  
  - Process cleanup with `process_destroy()` (frees user stack, marks PCB unused).  
  - **`runproc` command** to create and execute a test process.  
  - Shell returns properly after process execution.  
  - All previous features (`proclist`, `proccreate`, `vmmclone`, `elfload`) remain fully functional.
- `v0.4.5` — **Cooperative Scheduler complete.**  
  - Ready queue with round‑robin scheduling.  
  - `process_yield()` for voluntary context switching.  
  - `process_exit()` for clean process termination.  
  - Assembly‑level context switching (`context_switch.asm`).  
  - **`testyield` command** to test cooperative scheduling.  
  - **`schstat` command** to show scheduler statistics.  
  - `runproc` now uses the scheduler.  
  - All previous features (`proclist`, `proccreate`, `vmmclone`, `elfload`) remain fully functional.
- `v0.4.6` — **Preemptive Scheduler & Unlocked Core capacity complete.**
  - **Multi-Pass Segment Reader:** Configured `stage2.asm` to pull kernel blocks in safe 128-sector chunks, advancing segment offsets dynamically to entirely defeat real-mode 64 KB wrap limits.
  - **Kernel Size Limit Lifted:** Expanded kernel disk read thresholds up to 256 sectors (128 KB allocation ceiling).
  - **Userland Reboot System Call:** Added system call #25 (`SYS_REBOOT`) to cleanly wire Ring 3 Userland Shell option 4 right back into a Ring 0 hardware triple-fault motherboard reset.
  - **Preemptive Core Integration:** Validated PIT clock timer integration (`IRQ0` at 100Hz) enforcing forceful quantum task slicing across ready queues.
- `v0.4.7` - **newlib in userland, blocking reads, boot-time shell choice, userland heap test.**
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
- `v0.4.8` - **Process cleanup on exit; `process_exit` race closed.**
  - **PCB reclaim.** `process_exit` now calls `process_reclaim`, which frees the exiting process's ELF segment pages and user stack pages, sets `state = PROC_STATE_UNUSED`, resets `pid = 0`, and decrements `process_count`. The PCB slot can be reused by a future `process_create`. Running `testyield` or `runproc` many times in one boot no longer exhausts the 32-slot pool.
  - **`process_exit` timer race closed.** Interrupts are disabled for the entire critical section — from the first state mutation through the `context_switch` call — so the timer cannot re-add the exiting process to the ready queue after `scheduler_ready_queue_remove`, nor save the current stack frame into `next->rsp` before `context_switch` switches stacks. Interrupts are re-enabled by the `iretq` in `context_switch`, or by an explicit `sti` before the jump to the kernel shell in the no-runnable-process fallback.
  - **Page-table teardown deferred.** The process's `cr3` page tables still leak (a few pages per process). The teardown requires walking the page tables and freeing only the user-space portion without touching shared kernel mappings; it is a follow-up.
- `v0.4.9` — **Six scheduler, TSS, and interrupt ABI bugs fixed; user shell exit path stabilized.**
  - **ELF-load race at boot.** `process_create` adds the PCB to the ready queue before the ELF is loaded, so a PIT tick between create and `elf_load_into_process` could schedule a process whose entry page was not yet mapped. Fixed at all three call sites (`kmain` boot path, `elfload`, `usershell`) by removing the PCB from the ready queue until the ELF is loaded and `entry_point` is set. This eliminated the intermittent user-mode `#PF` at `CR2 == RIP == 0x8000000000` that reproduced when a key was pressed during the boot-choice window.
  - **`TSS.RSP0` / `g_syscall_stack_top` lockstep.** These two are documented to move in lockstep with `current`, unconditionally. The update was gated on `entry_point < KERNEL_BASE`, so kernel-mode switches left `g_syscall_stack_top` stale. Gate removed in `scheduler_switch_to`, `process_exit`, and `timer_preempt_handler`.
  - **`process_exit` fallback TSS restoration.** The no-runnable-process fallback zeroed only `g_syscall_stack_top`, leaving `TSS.RSP0` pointing at a dead process's stack. Now restores both to idle's kernel stack top.
  - **Kernel stack slot aliasing.** Kernel stack slots were computed as `pid % MAX_PROCESSES`. `next_pid` is monotonic and never decremented on teardown, so after 32+ process creations the modulo wrapped and a new process aliased a live one's slot — including idle's. Replaced with an explicit `slot_owner[]` allocator and a `pcb_t::kernel_stack_slot` field. Reproduced reliably after ~12 `testyield` runs; confirmed fixed by 20+ clean runs.
  - **`context_switch.asm` offsets.** Inserting `kernel_stack_slot` at `pcb_t` offset `0x58` shifted every later field by 8 bytes. `context_switch.asm` hardcoded the old offsets, causing every context switch to load the wrong registers. Every offset at or after `user_stack_phys` updated by +8. A `_Static_assert` block in `process.c` now pins every offset the asm depends on, turning a future `pcb_t` change into a build error instead of a silent crash.
  - **`scheduler_switch_to` preemption window.** The function set `current_process = next` with interrupts enabled, then called `context_switch(prev, next)`. A PIT tick in that window saw `current == next` while the CPU was still on `prev`'s stack, and saved the in-flight CPU state into `next`'s PCB fields — corrupting `next->rsp` with a pointer into the wrong stack. Fixed with `__asm__ volatile("cli")` at the top of `scheduler_switch_to`. The target's `iretq` re-enables interrupts via its saved `RFLAGS`.
  - **`irq1_stub` ABI violation.** The keyboard interrupt stub called `irq1_handler` without preserving caller-saved registers. If an IRQ1 fired between the `sti` and a subsequent `jmp *%rax` in `process_exit`'s fallback, `irq1_handler` clobbered `%rax` and the jump landed mid-instruction inside `kmain_shell_loop`. Reproduced consistently after user shell exit. Fixed on two fronts: `irq1_stub` (and the other stubs that call C handlers) now preserve all 15 GPRs, and the fallback uses a direct `jmp kmain_shell_loop` (`rel32`) with no register involved.
  - **`process_dump_all` cosmetic fix.** Detached PCBs (e.g. placeholders left by the `proccreate` command) now display as `DETACHED` instead of `READY`.
  - **Frame validation in `context_switch.asm`.** `.kernel_task` validates the resume frame's `rip` (must be ≥ `KERNEL_BASE`) and `cs` (must be `0x18`) before popping. A corrupt frame now emits a single serial marker byte (`R` or `C`) and halts, instead of producing an opaque kernel-mode `#GP` at the stub's `iretq`.
- `v0.5.0` ⭐ NEW — **Storage layer complete: ATA PIO driver, FatFs, single-drive and dual-drive layouts, persistence verified.**
  - **ATA PIO block device driver** on the primary channel: `ata_init`, `ata_read_sector`, `ata_read_sectors`, `ata_write_sector`, `ata_write_sectors`, `ata_flush_cache`, per-drive entry points, and `atatest` diagnostics. Three bring-up bugs found and fixed (PIC mask restoration, exception frame offsets, LBA mode bit).
  - **FatFs R0.15 vendored** with a `diskio.c` shim that maps FatFs onto the ATA PIO driver.
  - **Dual-drive storage:** kernel on master `hdd.img`, FAT16 volume on slave `fat.img`. Kernel shell commands `fatmount`, `fatls`, `fatcat`.
  - **Userland file I/O:** `SYS_OPEN` (4), `SYS_CLOSE` (6), and the `SYS_READ` fd branch, so newlib's `open`/`read`/`write`/`close` work from Ring 3. Verified with a create/write/close/reopen/read round-trip.
  - **Single-drive layout:** `FAT_CONFIG=dual|single` at build time selects the storage layout. FAT16 partition at LBA 2048. `diskio.c` translates volume-relative sector numbers to device LBAs via `FAT_PARTITION_OFFSET` (2048 in single, 0 in dual).
  - **`FF_MULTI_PARTITION = 0`** means FatFs is not partition-aware, so the offset lives in `diskio.c`, not the BPB.
  - **Persistence across reboot verified** end-to-end by the user shell option 5.
  - **Expanded user-shell regression harness:** options 4 (FS round-trip), 5 (persistence), 6 (multi-file), 7 (4 KB round-trip). `[FS TEST]` fixed: `msg_len` was hardcoded to 19 but the string is 20 bytes.
  - **Config diagnostic at boot** (`kmain.c`): VGA and serial report which storage layout the kernel booted with.
  - **Kernel-size tripwire** checks both the stage2 staging ceiling (176 KB) and the disk-layout ceiling (983 KB).
  - `SYS_UNLINK` is not yet implemented; option 6's delete phase is skipped.

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
- ✅ Fully functional IDT and ISR stubs (all stubs preserve caller-saved registers across C calls)
- ✅ Stable IRQ0 (PIT timer) and IRQ1 (keyboard)
- ✅ #DE (Divide by Zero) handler working
- ✅ #PF (Page Fault) handler with CR2, ERR, RIP dump
- ✅ #GP (General Protection Fault) handler with ERR, RIP, CS dump
- ✅ Test command (`test`) for triggering all three exceptions
- ✅ Frame validation in `context_switch.asm` catches corrupt resume frames before `iretq`

**Drivers**
- ✅ VGA text console (80×25) with scrolling and cursor control
- ✅ Keyboard driver with shift/caps/backspace support
- ✅ PIT timer incrementing `g_ticks`
- ✅ Serial (COM1) output for kernel debugging

**Storage**
- ✅ FatFs R0.15 integrated, read and write
- ✅ Dual-drive layout: boot + kernel on master, FAT16 volume on slave
- ✅ Single-drive layout: boot + kernel + FAT16 partition at LBA 2048 on master
- ✅ Kernel-shell access: fatmount, fatls, fatcat
- ✅ Userland access: open, close, read, write on FAT files, via syscalls 1, 3, 4, 6
- ✅ Persistence across reboot verified end-to-end
- ✅ Config diagnostic at boot (VGA + serial)

**Shells / Console**
- ✅ Kernel shell (diagnostic, reached via k at boot) with commands including gdtdump, tssdump, atatest, fatmount, fatls, fatcat
- ✅ User shell (Ring 3, newlib) as the default interactive console
- ✅ Unknown command handling
- ✅ Serial console output (COM1) for debugging alongside VGA

**Userland C Library (newlib)**
- ✅ **newlib 4.x** linked into user programs
- ✅ `printf` / `fprintf` / `puts` work from Ring 3
- ✅ `malloc` / `free` backed by `sbrk` → `sys_brk`
- ✅ `memcpy`, `memset`, `str*`, and the rest of the standard C string functions
- ✅ newlib reentrancy initialized at startup (`_impure_ptr = &_impure_data`)
- ✅ open / close / read / write on FAT files from Ring 3
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
- ✅ SYS_WRITE (syscall #1) — Writes to serial and VGA output, returns count
- ✅ SYS_EXIT (syscall #2) — Terminates the current process
- ✅ SYS_READ (syscall #3) — Blocking read from fd 0; file-descriptor read on fd ≥ 3
- ✅ SYS_OPEN (syscall #4) — Opens a FAT file
- ✅ SYS_CLOSE (syscall #6) — Closes a file descriptor
- ✅ SYS_BRK (syscall #10) — Grow or shrink the process heap
- ✅ SYS_REBOOT (syscall #25) — Ring 3 → Ring 0 hardware reset
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
- ✅ **Kernel stack slot allocator** decoupled from pid (`slot_owner[]` map)
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
- FAT_CONFIG=dual|single selects the storage layout
- ./run convenience script with a menu of preconfigured targets
- Multiple QEMU run modes (serial, debug, headless, KVM, single-drive)
- Debug logging support with serial console

---

## ⚠️ Known Limitations

- SYS_UNLINK is not implemented. FatFs has f_unlink, but there is no syscall for it. The user shell's multi-file test (option 6) creates and verifies files but skips the delete phase and reports the omission.
- **Page tables are not freed on process exit.** `process_exit` reclaims the process's PCB slot and its ELF/user-stack pages, but the process's page tables (cr3) leak. This is a few pages per process. The teardown is deferred; it requires walking the page tables and freeing the user-space portion without touching shared kernel mappings.
- **The boot stack address is hardcoded.** `process_exit`'s fallback to `kmain_shell_loop` sets `rsp = 0xFFFFFFFF8008FF00`. This address is in the low 1 MB region, currently mapped because the bootloader identity-maps it. Nothing in the kernel guarantees it stays mapped. A follow-up refactor will move the kernel shell onto a proper stack allocated from the kernel stack pool.
- **No IST stack for `#DF`.** A real double fault triple-faults with no diagnostic. Adding a small IST stack and pointing the `#DF` IDT gate at it would turn future double faults into printed diagnostics.
- **The keyboard buffer is shared.** Multiple shells reading from fd 0 will compete for bytes. Each keystroke goes to whichever blocked process the scheduler picks first. A per-process tty or a console-focus mechanism would be required to make multiple shells usable side by side.
- **`sys_brk`'s `heap_base` is a single constant.** Each process's heap starts at the same *virtual* address (`0x8000200000`) and grows in its own address space (different `cr3`), so there is no address conflict. The shared constant is a code-cleanliness issue, not a functional one.
- **`vmm_map_page_in_cr3` does not flush the TLB.** Callers must `invlpg` after mapping if the address may have a stale translation. `sys_brk` does this; new callers should too.
- **User programs are still embedded in the kernel ELF.** They are not loaded from the FAT volume. Loading programs from disk is the next storage milestone, and requires a `sys_exec`-style syscall.
- **Single-drive vs dual-drive is a build-time choice.** One kernel binary cannot serve both layouts. The `FAT_CONFIG` variable selects which layout the kernel expects; running the wrong image under the wrong kernel will fail to mount FatFs.
- **A subset of newlib is exercised.** `stdio`, `stdlib` (`malloc`), `string`, `unistd`, and `fcntl` are the primary use; `math.h` (`libm.a`) is linked but not exercised. `signal`, `pthread`, `dirent`, and other subsystems are compiled in but untested on this kernel.

---

## 🌱 Next Steps (Roadmap)

### Short-term (Next)
- ~~Cooperative scheduler~~ ✅
- ~~Preemptive scheduler~~ ✅
- ~~User-mode shell~~ ✅
- ~~Blocking reads~~ ✅
- ~~Userland heap via sys_brk (newlib malloc)~~ ✅
- ~~newlib 4.x linked into user programs~~ ✅
- ~~Process cleanup on exit (PCB reclaim)~~ ✅
- ~~Scheduler / TSS / interrupt ABI stability pass~~ ✅ v0.4.9
- ~~ATA PIO block device driver~~ ✅ v0.4.10
- ~~FatFs integration, kernel-side and userland~~ ✅ v0.4.10
- ~~Single-drive layout~~ ✅ v0.5.0
- **`SYS_UNLINK`** — add `f_unlink` behind a syscall; completes the multi-file test (option 6)
- **Kernel maintenance pass** — see [`MAINTENANCE.md`](MAINTENANCE.md) for the full list. In priority order: kernel size ceiling (done in v0.5.0's staging update), boot stack off the hardcoded address, IST for `#DF`, code hygiene, testing infrastructure.

### Medium-term
- **Move the kernel shell off the hardcoded boot stack** — allocate the shell's stack from the kernel stack pool, so `process_exit`'s fallback no longer depends on a specific low-memory address being mapped.
- **IST for `#DF`** — turn future double faults into printed diagnostics.
- **User programs from disk** — add a `sys_exec`-style syscall, load ELF files from the FAT volume, and stop embedding programs in the kernel image.
- **Serial console debug access** — kernel shell reachable over COM1, physically separate from the user's keyboard. This is the right shape for runtime kernel-shell access; the magic-key-combo approach was tried and abandoned (it's a security backdoor and the kernel shell isn't a process the scheduler can suspend).

### Long-term
- **File System (VFS)** — a VFS layer above FatFs, with mount points and path resolution
- **Framebuffer graphics** — move off VGA text mode
- **Page-table teardown on process exit** — walk the process's page tables and free the user-space portion, completing the cleanup story from the short-term item
- **Bootloader migration to Limine** — replaces the hand-written stage2, removes the 176 KB kernel ceiling, and turns the kernel into a file loaded by the bootloader rather than a fixed-LBA blob

---

## 📜 License

This project is licensed under the **MIT License**.  
Use freely, modify freely, credit appreciated.
