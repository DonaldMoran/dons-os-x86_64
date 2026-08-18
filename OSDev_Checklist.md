# Full OSDev Milestone Checklist
## dons‑os (x86_64) — Project Progress

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

## 2. Core Kernel Features (17/17 Complete)

| # | Milestone | Status | Notes |
|---|-----------|--------|-------|
| 6 | **IDT + ISR Stubs** | ✅ Complete | Exception handlers, interrupt gates, error‑code support |
| 7 | **PIC Remap** | ✅ Complete | IRQ0–IRQ15 mapped to 0x20–0x2F |
| 8 | **PIT Timer** | ✅ Complete | IRQ0 tick counter, scheduling foundation |
| 9 | **Keyboard Driver** | ✅ Complete | IRQ1, scancode set 1, shift/caps, input buffer |
| 10 | **VGA Console Upgrade** | ✅ Complete | Scrolling, cursor control, shell‑ready console |
| 11 | **Shell** | ✅ Complete | Command interpreter: help, clear, info, mem, version, reboot, pmmtest, test, vmmtest, serialtest, heapstat, maptest, testrec, heaptest, **user**, **user2**, nxtest, syscall, **elfload**, **proclist**, **proccreate**, **vmmclone** |
| 12 | **E820 Memory Map** | ✅ Complete | Memory detection, BootInfo struct passed to kernel |
| 13 | **Physical Memory Manager** | ✅ Complete | Bitmap allocator, page alloc/free, reserved region marking |
| 14 | **Virtual Memory Manager** | ✅ Complete | Recursive paging implemented at PML4[510]. VMM can read/write PML4 from higher-half kernel. HHDM mapping at PML4[256]. Dynamic page table allocation (PDPT, PD, PT) working. No GP faults when accessing page tables. `vmmtest` command verifies functionality. User-space page mapping with PT_USER flag working. **NX (No Execute) bit support via PT_NX flag.** **Dynamic HHDM mapping via `ensure_hhdm_mapped()`.** **Page table cloning via `vmm_clone_page_table()`.** |
| 15 | **Serial Debug Output** | ✅ Complete | COM1 serial output for kernel debugging alongside VGA |
| 16 | **Heap Allocator** | ✅ Complete | `kmalloc()` and `kfree()` working with free list. `heapstat` command for debugging. `heaptest` command for verification. 64MB initial heap with automatic expansion. Memory reuse verified. **WRITE bit fix for heap pages.** |
| 17 | **User Mode (Ring 3)** | ✅ Complete | GDT with user segments (0x30 code, 0x28 data). TSS configured for stack switching. `iretq`-based transition from kernel to user mode. User code executes at CPL=3 with page protection. `create_user_process()` for launching user code. Test commands: **user**, **user2** (placeholders for future usermode tests). |
| 18 | **NX (No Execute) Bit Support** | ✅ Complete | PT_NX flag added to vmm.h (bit 63). NX flag handling in `vmm_map_page()` on final PTE. `nxtest` command for verifying NX functionality. NX status displayed in `vmmtest` output. **8KB .bss padding** to prevent keyboard buffer corruption. **`keyboard_init()` moved after memory management initialization.** |
| 19 | **System Calls** | ✅ Complete | SYSCALL/SYSRET instruction interface via MSRs (IA32_STAR, IA32_LSTAR, IA32_FMASK). SYS_WRITE (syscall #1) and SYS_EXIT (syscall #2) implemented. Syscall dispatcher with proper x86_64 ABI. `syscall` test command for verification. Proper register preservation across syscalls. **Safe user‑space memory access via `safe_copy_from_user()` using HHDM.** |
| 20 | **ELF Loader** | ✅ Complete ⭐ FINALIZED | Parses ELF64 headers and program headers. Maps LOAD segments with correct permissions (Read, Write, Execute, User). Allocates and maps user stack pages. Transitions to user mode via IRETQ with proper selectors (CS=0x33, SS=0x2B). Sets IOPL=3 for user I/O access. Page table execute permissions at all levels (PML4 → PDPT → PD → PT). **`elfload` command** to load and run embedded ELF programs. **Works reliably on first boot** (handles bootloader identity‑mapping conflict). **Safe HHDM‑based copying** of program segments and stack. Tested with "Hello from Userland!" via serial. |
| 21 | **Process Foundation** | ✅ Complete ⭐ NEW | Process Control Block (PCB) structure. Process creation (`process_create`). Process listing (`proclist`). Page table cloning (`vmm_clone_page_table`). **`vmmclone` command** for testing page table isolation. Ready queue infrastructure (foundation for scheduler). |
| 22 | **BootInfo Fix** | ✅ Complete ⭐ NEW | Fixed BootInfo structure alignment between bootloader and kernel. Added magic number and version validation. Proper memory map detection from BIOS E820. |

---

## 3. Memory Management (7/7 Complete)

| # | Milestone | Status | Notes |
|---|-----------|--------|-------|
| 23 | **Higher‑Half Kernel** | ✅ Complete | Kernel mapped to `0xFFFFFFFF80100000`, identity map preserved |
| 24 | **Virtual Memory Manager** | ✅ Complete | Recursive paging at PML4[510], HHDM mapping at PML4[256], dynamic page table allocation, `vmmtest` working, **NX bit support**, **dynamic HHDM mapping**, **page table cloning** |
| 25 | **Serial Debug Output** | ✅ Complete | COM1 serial output for kernel debugging, integrated with QEMU |
| 26 | **Kernel Heap** | ✅ Complete | `kmalloc()` and `kfree()` working with free list. Memory reuse verified via `heaptest`. **WRITE bit fix for heap pages.** |
| 27 | **User Memory Mapping** | ✅ Complete | Pages mapped with PT_USER flag for user/kernel isolation |
| 28 | **NX (No Execute) Bit** | ✅ Complete | PT_NX flag in VMM, `nxtest` command, NX status in `vmmtest`, **8KB .bss padding**, **keyboard_init() moved after memory management** |
| 29 | **HHDM Dynamic Mapping** | ✅ Complete | `ensure_hhdm_mapped()` for on‑demand physical memory access. All physical memory mapped into HHDM region. Used by ELF loader and page table cloning. |

---

## 4. User Space & Advanced Features (3/6 Complete)

| 30 | **System Calls** | ✅ Complete | SYSCALL/SYSRET with SYS_WRITE and SYS_EXIT, MSR configuration, `syscall` test command, **safe user‑space memory access** |
| 31 | **ELF Loader** | ✅ Complete ⭐ FINALIZED | Parse and load ELF64 files, map user code and stack, transition to user mode, `elfload` command, "Hello from Userland!" tested, **works on first boot** |
| 32 | **Process Foundation** | ✅ Complete ⭐ NEW | PCB, process creation, process listing, page table cloning, `vmmclone` command |
| 33 | **Process Model** | ☐ Not Started | Page table per process, context switching |
| 34 | **Scheduler** | ☐ Not Started | Cooperative → preemptive, PIT‑driven task switching |
| 35 | **Slab Allocator** | ❌ Not Needed | Free list already provides memory reuse for kmalloc/kfree |

---

## Summary

| Phase | Completed | Total | Progress |
|-------|-----------|-------|----------|
| Boot & System Init | 5 | 5 | **100%** ✅ |
| Core Kernel | 17 | 17 | **100%** ✅ |
| Memory Management | 7 | 7 | **100%** ✅ |
| User Space | 3 | 6 | **50%** 🚧 |
| **Overall** | **32** | **35** | **91%** |

---

## Recent Milestone Achievements (Chronological Order - Newest First)

### v0.4.3 — ELF Loader Stabilized + Process Foundation ⭐ NEW
- ELF loader works on **first boot** (no more "run twice" bug)
- Bootloader identity‑mapping conflict resolved (detect and replace with proper user‑mode PTEs)
- `PT_EXEC` (PWT bit) handling added to `vmm_map_page()`
- Safe HHDM‑based user‑space memory access in syscall handler (`safe_copy_from_user`)
- **Process Foundation:** PCB, `process_create()`, `proclist`, `proccreate`, `vmmclone` (page table cloning)
- **Dynamic HHDM mapping:** `ensure_hhdm_mapped()`
- **BootInfo validation:** Magic number and version checking
- Removed redundant `simple` command
- All existing commands remain fully functional

### v0.4.2 — BootInfo & Process Foundation ⭐ NEW
- Fixed BootInfo structure alignment between bootloader and kernel
- Added magic number and version validation
- Proper memory map detection from BIOS E820
- Dynamic HHDM mapping (`ensure_hhdm_mapped`)
- Page table cloning (`vmm_clone_page_table`)
- Process Control Block (PCB) infrastructure
- `proclist`, `proccreate`, `vmmclone` commands

### v0.4.1 — Syscall Stack Stability
- Unified kernel stack model  
- Correct SYSRET return path  
- Clean user → kernel → shell transitions  
- Verified ELF loader return path  
- Removed `simple` command (redundant)  
- `user` and `user2` retained as placeholders  

### v0.4.0 — ELF Loader
- Fully functional ELF64 loader  
- Correct user permissions  
- User stack allocation  
- IRETQ transition  
- `elfload` command  
- “Hello from Userland!” verified  
- Page table execute permissions at all levels  
- Serial output verified  

### v0.3.2 — System Calls
- SYSCALL/SYSRET MSR setup  
- SYS_WRITE + SYS_EXIT  
- Full x86_64 syscall ABI  
- Register preservation  
- `syscall` test command  

### v0.3.1 — NX Bit Support
- PT_NX flag  
- NX handling in VMM  
- `nxtest` command  
- Heap WRITE bit fix  
- Keyboard buffer corruption resolved  

### v0.3.0 — User Mode (Ring 3)
- GDT user segments  
- TSS stack switching  
- IRETQ transition  
- PT_USER mapping  
- `create_user_process()`  
- Test commands: simple, user, user2 (historical)

---

### Previous Tags
*(unchanged — historical accuracy preserved)*

---

## Next Steps (Recommended Order)

1. **Cooperative Scheduler** — Ready queue, `process_yield()`, round‑robin task switching
2. **Preemptive Scheduler** — Timer interrupt integration, preemptive task switching
3. **Ring0 Kernel Threads** — Kernel daemons, system services
4. **Framebuffer Graphics** — Move from VGA text mode to graphics
5. **File System** — Virtual File System (VFS) layer

---

*Last Updated: August 2026*
