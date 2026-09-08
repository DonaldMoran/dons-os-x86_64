# dons‑os Boot Chain Documentation  
### Real Mode → Protected Mode → Long Mode → 64‑bit Kernel

This document explains the complete boot process of **dons‑os**, from the CPU’s reset state in **16‑bit real mode**, through **32‑bit protected mode**, into **64‑bit long mode**, and finally into the **C‑based 64‑bit kernel**.  
It describes the architecture, paging structures, mode transitions, disk layout, and kernel loading pipeline.

---

## 🧭 Overview

The boot chain consists of five stages:

1. **01_boot_16bit** — BIOS boot sector  
2. **02_boot_32bit** — Protected mode entry  
3. **03_boot_64bit** — Paging + long mode entry  
4. **04_kernel_64bit** — Kernel build (ELF → flat binary)  
5. **05_boot_kernel64** — Full bootloader + kernel pipeline

Each stage is isolated, minimal, and educational.

---

## 🧩 Stage 1 — 16‑bit Real Mode (01_boot_16bit)

### Responsibilities
- Runs at CPU reset (0x7C00)  
- Sets up the stack  
- Prints early text  
- Loads Stage2 from disk  
- Performs BIOS disk reads  
- Enables A20 line  
- Prepares for protected mode

### Key Concepts
- BIOS interrupts (`int 0x10`, `int 0x13`)  
- Real‑mode segmentation  
- A20 gate enabling  
- Disk layout: Stage2 begins at LBA 1

### Output
Loads Stage2 into memory and jumps to it.

---

## 🧩 Stage 2 — 32‑bit Protected Mode (02_boot_32bit)

### Responsibilities
- Build a minimal **GDT**  
- Load GDT with `lgdt`  
- Set CR0.PE = 1 (protected mode enable)  
- Far jump into 32‑bit code segment  
- Switch to flat memory model  
- Print text using VGA memory  
- Prepare paging structures for long mode

### GDT Layout
- Null descriptor  
- 32‑bit code segment  
- 32‑bit data segment  
- 64‑bit code segment (for later)  
- 64‑bit data segment (for later)

### Output
Paging structures are prepared; control passes to Stage3.

---

## 🧩 Stage 3 — 64‑bit Long Mode Entry (03_boot_64bit)

### Responsibilities
- Build full paging hierarchy:
  - PML4  
  - PDPT  
  - PD  
  - PT  
- Identity‑map low memory  
- Map kernel physical address (0x00100000)  
- Enable PAE (CR4.PAE = 1)  
- Load PML4 into CR3  
- Enable long mode (EFER.LME = 1)  
- Enable paging (CR0.PG = 1)  
- Far jump into 64‑bit code segment

### Paging Model
- 4‑level paging  
- Identity mapping for early boot  
- Kernel mapped into higher physical memory  
- 2MiB or 4KiB pages depending on configuration

### Output
CPU enters long mode and jumps to the kernel entry point.

---

## 🧩 Stage 4 — Kernel Build (04_kernel_64bit)

### Responsibilities
- Compile kernel C code with `-ffreestanding`  
- Use custom linker script to place kernel at 0x00100000  
- Convert ELF → flat binary  
- Provide `_start` and `kmain`  
- Provide VGA text output  
- Provide minimal runtime environment

### Kernel Entry Flow
```
_start:
    - set up stack
    - call kmain()

kmain():
    - print text
    - run kernel logic
```

### Output
Produces `kernel.bin`, a flat binary loaded by Stage2.

---

## 🧩 Stage 5 — Full Boot Pipeline (05_boot_kernel64)

### Responsibilities
- Load kernel.bin from disk  
- Place kernel at physical 0x00100000  
- Build page tables  
- Enter long mode  
- Jump to kernel `_start`  
- Provide VGA text output  
- Provide stable RIP flow

### Disk Layout
```
LBA 0: boot.bin (stage1)
LBA 1–40: stage2.bin
LBA 64+: kernel.bin
```

### Output
A fully booting 64‑bit OS.

---

## 🧠 Mode Transition Summary

### Real Mode → Protected Mode
- Set CR0.PE = 1  
- Far jump to 32‑bit segment  
- Load GDT

### Protected Mode → Long Mode
- Build PML4  
- Set CR4.PAE = 1  
- Set EFER.LME = 1  
- Set CR0.PG = 1  
- Far jump to 64‑bit segment

### Long Mode → Kernel
- Jump to `_start`  
- Set up stack  
- Call `kmain`

---

## 🗺 Memory Map (Early Boot)

```
0x00000000 — Real mode IVT
0x00000400 — BIOS data area
0x00007C00 — Boot sector (stage1)
0x00080000 — Stage2 load address
0x00100000 — Kernel load address
0x00200000 — Paging structures
```

---

## 🧱 Paging Structure (4‑Level)

```
PML4
 └── PDPT
      └── PD
           └── PT
                └── 4KiB pages
```

Identity mapping ensures early code runs safely.

---

## 🧪 QEMU Debugging

### Debug Mode
```
qemu-system-x86_64 \
  -drive file=hdd.img,format=raw \
  -d int,cpu_reset \
  -no-reboot \
  -no-shutdown
```

### Useful For
- Triple fault diagnosis  
- Paging verification  
- Segment selector debugging  
- Long mode entry validation

---

## 🎉 Current Milestone: Long Mode Kernel Boot

The system successfully:

- Loads kernel from disk  
- Builds page tables  
- Enters long mode  
- Executes `_start`  
- Runs `kmain`  
- Prints text in 64‑bit mode  
- Boots without triple faults

## 🏷 Tag Index
- v0.0.1-longmode — first successful long‑mode boot
- v0.0.2-pmm-working — physical memory manager fully operational

---

## 🌱 Next Steps

- **[64‑bit IDT + interrupts](ca://s?q=Help_me_set_up_a_64bit_IDT)**  
- **[Serial logging](ca://s?q=Help_me_add_serial_logging_in_long_mode)**  
- **[E820 memory map](ca://s?q=Help_me_parse_E820_memory_map)**  
- Higher‑half kernel  
- Physical page allocator  
- Kernel heap  
- Framebuffer graphics  
- Scheduler  
- Shell

---

## 📜 License

MIT License — free to use, modify, and learn from.
