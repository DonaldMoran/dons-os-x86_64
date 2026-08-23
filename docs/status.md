git commit -m "v0.4.6: ELF Loader Stabilized & Reliable User Mode Execution

- Fixed scheduler infinite loop causing processes to run twice
- Fixed process cleanup with proper resource freeing
- Fixed TSS corruption by resetting stack before shell return
- Fixed SYS_EXIT return path using jmp instead of call
- Added KERNEL_BASE check to distinguish kernel vs user processes
- Fixed IRETQ selectors to use 0x2B/0x33 (original working values)
- Only allocate user stack for user-space processes
- process_exit() now calls process_destroy() for proper cleanup
- process_destroy() fully resets PCB to PROC_STATE_UNUSED
- scheduler_ready_queue_add() skips terminated/unused processes
- elfload re-adds process to ready queue before switching
- testyield, runproc, elfload, proclist all working reliably
- Verified multiple consecutive runs without crashes or TSS errors

Tags: v0.4.6, elfload, scheduler, process, tss, user-mode"


Project dons-os x86_64 User-Mode Shell.txt
Project: dons-os x86_64 User-Mode Shell Implementation
Context

I'm working on an educational x86_64 OS project called dons-os (https://github.com/DonaldMoran/dons-os-x86_64). 
The project has a fully working boot chain (16-bit → 32-bit → 64-bit long mode) with a higher-half kernel, interrupts, 
memory management, and a cooperative scheduler.

Current Status:

    ✅ Full boot chain working
    ✅ Higher-half kernel at 0xFFFFFFFF80100000
    ✅ Interrupts (IDT, PIC, PIT, keyboard)
    ✅ PMM, VMM with recursive paging, HHDM
    ✅ Heap allocator (kmalloc/kfree)
    ✅ Ring 3 support (GDT, TSS, user segments)
    ✅ System calls (SYS_WRITE, SYS_EXIT)
    ✅ ELF loader that loads and runs user programs in Ring 3
    ✅ Process infrastructure (PCB, process creation)
    ✅ Cooperative scheduler (v0.4.5) with process_yield()

The Goal:

    Create a user-mode shell that runs in Ring 3
    Use cooperative scheduler (not preemptive yet)
    Preserve existing elfload functionality

    Later upgrade to preemptive scheduler

The Challenge:
Previous attempts to implement both preemptive scheduler and user-mode shell simultaneously failed. The current plan 
is to implement the user-mode shell first using the cooperative scheduler, then upgrade to preemptive.
Current Understanding of ELF Loader Flow

I believe the current elfload process works like this:
text

kmain.c:handle_command("elfload")
    ↓
elf.c:elf_load(test_program)
    ↓
elf.c:ring3_enter(entry, stack)
    ↓
ring3.c:ring3_enter()
    ↓
ring3_entry.S:jump_to_user_mode()
    ↓
[IRETQ to Ring 3]
    ↓
test_program.asm:_start
    ↓
SYS_WRITE (syscall #1)
    ↓
user_syscall_entry.asm:user_syscall_entry
    ↓
user_syscall.c:syscall_dispatch()
    ↓
[Should return to user program]
    ↓
SYS_EXIT (syscall #2)
    ↓
user_syscall_entry.asm:user_syscall_entry
    ↓
user_syscall.c:syscall_dispatch()
    ↓
[Should jump to kmain_shell_loop]

However, I suspect the actual implementation may differ from my notes.
What I Need From You

To verify the current state and proceed with the implementation, I will provide you with any files you wish:

You will need to ask me for files as we work, so that you understand them best, here is the current project tree:

├── 04_kernel_64bit
│   ├── archive
│   │   ├── ring3.c_removed
│   │   └── ring3_entry.S
│   ├── context_switch.asm
│   ├── elf.c
│   ├── entry.asm
│   ├── gdt.c
│   ├── heap.c
│   ├── idt.c
│   ├── idt_load.asm
│   ├── include
│   │   ├── archive
│   │   │   └── ring3.h
│   │   ├── bootinfo.h
│   │   ├── debug.h
│   │   ├── elf.h
│   │   ├── gdt.h
│   │   ├── heap.h
│   │   ├── idt.h
│   │   ├── interrupts.h
│   │   ├── keyboard.h
│   │   ├── pmm.h
│   │   ├── process.h
│   │   ├── ring3.h_removed
│   │   ├── scheduler.h
│   │   ├── serial.h
│   │   ├── syscall.h
│   │   ├── tss.h
│   │   ├── user_msr.h
│   │   ├── user_space.h
│   │   ├── user_syscall.h
│   │   ├── vga.h
│   │   └── vmm.h
│   ├── interrupts.c
│   ├── isr.asm
│   ├── keyboard.c
│   ├── kmain.c
│   ├── linker.ld
│   ├── Makefile
│   ├── pmm.c
│   ├── process.c
│   ├── ring3.c
│   ├── ring3_entry.S
│   ├── scheduler.c
│   ├── serial.c
│   ├── string.c
│   ├── syscall.c
│   ├── test_program.asm
│   ├── test_syscall.c
│   ├── tss.c
│   ├── user_linker.ld
│   ├── user_syscall.c
│   ├── user_syscall_entry.asm
│   ├── vga.c
│   └── vmm.c
├── 05_boot_kernel64
│   ├── boot.asm
│   ├── BOOTCHAIN.md
│   ├── boot.lst
│   ├── Makefile
│   └── stage2.asm
├── ai_request.txt
├── build.log
├── capture.txt
├── docs
│   └── PCB_offsets.md
├── elfload_first.txt
├── kvm_debug.log
├── kvm_serial.log
├── LICENSE
├── Makefile
├── OSDev_Checklist.md
├── qemu_kvm_debug.log
├── qemu.log
├── README.md
├── ROADMAP.md
├── run
└── serial.log

Recent work includes:
- Fixed scheduler infinite loop causing processes to run twice
- Fixed process cleanup with proper resource freeing
- Fixed TSS corruption by resetting stack before shell return
- Fixed SYS_EXIT return path using jmp instead of call
- Added KERNEL_BASE check to distinguish kernel vs user processes
- Fixed IRETQ selectors to use 0x2B/0x33 (original working values)
- Only allocate user stack for user-space processes
- process_exit() now calls process_destroy() for proper cleanup
- process_destroy() fully resets PCB to PROC_STATE_UNUSED
- scheduler_ready_queue_add() skips terminated/unused processes
- elfload re-adds process to ready queue before switching
- testyield, runproc, elfload, proclist all working reliably
- Verified multiple consecutive runs without crashes or TSS errors 
- Created a usershell in c (and one in assembly) neither fully working
  I prefer to keep working with the c one. It seems to run but I do not
  yet see the prompt on the vga screen and if I press keyboard keys
  I see no indication in the serial terminal that is being recognized.
  
Goal: Review the c shell implementation and fix it 

Additional info:
noneya@fedora:~/code/dons-os-x86_64$ cd 04_kernel_64bit
readelf -h user_shell.elf
readelf -l user_shell.elf
ELF Header:
  Magic:   7f 45 4c 46 02 01 01 00 00 00 00 00 00 00 00 00 
  Class:                             ELF64
  Data:                              2's complement, little endian
  Version:                           1 (current)
  OS/ABI:                            UNIX - System V
  ABI Version:                       0
  Type:                              EXEC (Executable file)
  Machine:                           Advanced Micro Devices X86-64
  Version:                           0x1
  Entry point address:               0x8000000000
  Start of program headers:          64 (bytes into file)
  Start of section headers:          7360 (bytes into file)
  Flags:                             0x0
  Size of this header:               64 (bytes)
  Size of program headers:           56 (bytes)
  Number of program headers:         3
  Size of section headers:           64 (bytes)
  Number of section headers:         9
  Section header string table index: 7

Elf file type is EXEC (Executable file)
Entry point 0x8000000000
There are 3 program headers, starting at offset 64

Program Headers:
  Type           Offset             VirtAddr           PhysAddr
                 FileSiz            MemSiz              Flags  Align
  LOAD           0x0000000000001000 0x0000008000000000 0x0000008000000000
                 0x000000000000080e 0x000000000000080e  R E    0x1000
  LOAD           0x000000000000180e 0x000000800000080e 0x000000800000080e
                 0x0000000000000183 0x0000000000000183  R      0x1000
  GNU_STACK      0x0000000000000000 0x0000000000000000 0x0000000000000000
                 0x0000000000000000 0x0000000000000000  RW     0x0

 Section to Segment mapping:
  Segment Sections...
   00     .ltext 
   01     .lrodata.str1.1 .lrodata.str1.16 
   02     
noneya@fedora:~/code/dons-os-x86_64/04_kernel_64bit$

Also here is the current session log, I ran usershell from the kernel shell and it appears to 
start, I see the message on the vga screen:
=== User Shell === << From kmain.c
Starting the user shell...<< From kmain.c
User shell size: 7936 byes << From kmain.c but also correct as found also in user_shell_data.c
Loading and running user shell... << From kmain.c
ELF loaded << From elf.c

And here is the serial output of th entire build and session. In the user shell I tried keying 
help and pressing enter:

make -C 01_boot_16bit clean
make[1]: Entering directory '/home/noneya/code/dons-os-x86_64/01_boot_16bit'
rm -f boot.bin stage2.bin hdd.img
make[1]: Leaving directory '/home/noneya/code/dons-os-x86_64/01_boot_16bit'
make -C 02_boot_32bit clean
make[1]: Entering directory '/home/noneya/code/dons-os-x86_64/02_boot_32bit'
rm -f boot.bin stage2.bin hdd.img
make[1]: Leaving directory '/home/noneya/code/dons-os-x86_64/02_boot_32bit'
make -C 03_boot_64bit clean
make[1]: Entering directory '/home/noneya/code/dons-os-x86_64/03_boot_64bit'
rm -f boot.bin stage2.bin kernel.bin hdd.img
make[1]: Leaving directory '/home/noneya/code/dons-os-x86_64/03_boot_64bit'
make -C 04_kernel_64bit clean
make[1]: Entering directory '/home/noneya/code/dons-os-x86_64/04_kernel_64bit'
rm -f *.o *.bin *.elf test_program test_program.bin test_program_data.c
rm -f user_shell.elf user_shell_data.c
rm -f user_shell.bin
make[1]: Leaving directory '/home/noneya/code/dons-os-x86_64/04_kernel_64bit'
make -C 05_boot_kernel64 clean
make[1]: Entering directory '/home/noneya/code/dons-os-x86_64/05_boot_kernel64'
rm -f serial.log qemu.log qemu_debug.log
rm -f boot.bin stage2.bin hdd.img
make[1]: Leaving directory '/home/noneya/code/dons-os-x86_64/05_boot_kernel64'
make -C 01_boot_16bit
make[1]: Entering directory '/home/noneya/code/dons-os-x86_64/01_boot_16bit'
nasm -f bin boot.asm -o boot.bin
nasm -f bin stage2.asm -o stage2.bin
rm -f hdd.img
qemu-img create -f raw hdd.img 10M
Formatting 'hdd.img', fmt=raw size=10485760
dd if=boot.bin   of=hdd.img conv=notrunc
dd if=stage2.bin of=hdd.img bs=512 seek=1 conv=notrunc
make[1]: Leaving directory '/home/noneya/code/dons-os-x86_64/01_boot_16bit'
make -C 02_boot_32bit
make[1]: Entering directory '/home/noneya/code/dons-os-x86_64/02_boot_32bit'
nasm -f bin boot.asm -o boot.bin
nasm -f bin stage2.asm -o stage2.bin
rm -f hdd.img
qemu-img create -f raw hdd.img 10M
Formatting 'hdd.img', fmt=raw size=10485760
dd if=boot.bin   of=hdd.img conv=notrunc
dd if=stage2.bin of=hdd.img bs=512 seek=1 conv=notrunc
make[1]: Leaving directory '/home/noneya/code/dons-os-x86_64/02_boot_32bit'
make -C 03_boot_64bit
make[1]: Entering directory '/home/noneya/code/dons-os-x86_64/03_boot_64bit'
nasm -f bin boot.asm -o boot.bin
nasm -f bin stage2.asm -o stage2.bin
nasm -f bin kernel.asm -o kernel.bin
rm -f hdd.img
qemu-img create -f raw hdd.img 10M
Formatting 'hdd.img', fmt=raw size=10485760
dd if=boot.bin    of=hdd.img conv=notrunc
dd if=stage2.bin  of=hdd.img bs=512 seek=1  conv=notrunc
dd if=kernel.bin  of=hdd.img bs=512 seek=64 conv=notrunc
make[1]: Leaving directory '/home/noneya/code/dons-os-x86_64/03_boot_64bit'
make -C 04_kernel_64bit
make[1]: Entering directory '/home/noneya/code/dons-os-x86_64/04_kernel_64bit'
nasm -f elf64 entry.asm -o entry.o
nasm -f elf64 ring3_entry.S -o ring3_entry.o
clang -target x86_64-unknown-elf -ffreestanding -O2 -Wall -Wextra -mcmodel=kernel -fno-pic -mno-red-zone -mno-sse -mno-sse2 -mno-avx -mno-mmx -Iinclude -c kmain.c -o kmain.o
clang -target x86_64-unknown-elf -ffreestanding -O2 -Wall -Wextra -mcmodel=kernel -fno-pic -mno-red-zone -mno-sse -mno-sse2 -mno-avx -mno-mmx -Iinclude -c pmm.c -o pmm.o
clang -target x86_64-unknown-elf -ffreestanding -O2 -Wall -Wextra -mcmodel=kernel -fno-pic -mno-red-zone -mno-sse -mno-sse2 -mno-avx -mno-mmx -Iinclude -c vmm.c -o vmm.o
clang -target x86_64-unknown-elf -ffreestanding -O2 -Wall -Wextra -mcmodel=kernel -fno-pic -mno-red-zone -mno-sse -mno-sse2 -mno-avx -mno-mmx -Iinclude -c heap.c -o heap.o
clang -target x86_64-unknown-elf -ffreestanding -O2 -Wall -Wextra -mcmodel=kernel -fno-pic -mno-red-zone -mno-sse -mno-sse2 -mno-avx -mno-mmx -Iinclude -c idt.c -o idt.o
clang -target x86_64-unknown-elf -ffreestanding -O2 -Wall -Wextra -mcmodel=kernel -fno-pic -mno-red-zone -mno-sse -mno-sse2 -mno-avx -mno-mmx -Iinclude -c interrupts.c -o interrupts.o
nasm -f elf64 idt_load.asm -o idt_load.o
nasm -f elf64 isr.asm -o isr.o
clang -target x86_64-unknown-elf -ffreestanding -O2 -Wall -Wextra -mcmodel=kernel -fno-pic -mno-red-zone -mno-sse -mno-sse2 -mno-avx -mno-mmx -Iinclude -I. -c vga.c -o vga.o
clang -target x86_64-unknown-elf -ffreestanding -O2 -Wall -Wextra -mcmodel=kernel -fno-pic -mno-red-zone -mno-sse -mno-sse2 -mno-avx -mno-mmx -Iinclude -c keyboard.c -o keyboard.o
clang -target x86_64-unknown-elf -ffreestanding -O2 -Wall -Wextra -mcmodel=kernel -fno-pic -mno-red-zone -mno-sse -mno-sse2 -mno-avx -mno-mmx -Iinclude -c serial.c -o serial.o
clang -target x86_64-unknown-elf -ffreestanding -O2 -Wall -Wextra -mcmodel=kernel -fno-pic -mno-red-zone -mno-sse -mno-sse2 -mno-avx -mno-mmx -Iinclude -c tss.c -o tss.o
clang -target x86_64-unknown-elf -ffreestanding -O2 -Wall -Wextra -mcmodel=kernel -fno-pic -mno-red-zone -mno-sse -mno-sse2 -mno-avx -mno-mmx -Iinclude -c gdt.c -o gdt.o
clang -target x86_64-unknown-elf -ffreestanding -O2 -Wall -Wextra -mcmodel=kernel -fno-pic -mno-red-zone -mno-sse -mno-sse2 -mno-avx -mno-mmx -Iinclude -I. -c syscall.c -o syscall.o
clang -target x86_64-unknown-elf -ffreestanding -O2 -Wall -Wextra -mcmodel=kernel -fno-pic -mno-red-zone -mno-sse -mno-sse2 -mno-avx -mno-mmx -Iinclude -I. -c elf.c -o elf.o
nasm -f elf64 test_program.asm -o test_program.o
ld.lld -T user_linker.ld -o test_program test_program.o
xxd -i test_program > test_program_data.c
clang -target x86_64-unknown-elf -ffreestanding -O2 -Wall -Wextra -mcmodel=kernel -fno-pic -mno-red-zone -mno-sse -mno-sse2 -mno-avx -mno-mmx -Iinclude -c test_program_data.c -o test_program_data.o
clang -target x86_64-unknown-elf -ffreestanding -O2 -Wall -Wextra -mcmodel=kernel -fno-pic -mno-red-zone -mno-sse -mno-sse2 -mno-avx -mno-mmx -Iinclude -I. -c user_syscall.c -o user_syscall.o
nasm -f elf64 user_syscall_entry.asm -o user_syscall_entry.o
clang -target x86_64-unknown-elf -ffreestanding -O2 -Wall -Wextra -mcmodel=kernel -fno-pic -mno-red-zone -mno-sse -mno-sse2 -mno-avx -mno-mmx -Iinclude -c process.c -o process.o
clang -target x86_64-unknown-elf -ffreestanding -O2 -Wall -Wextra -mcmodel=kernel -fno-pic -mno-red-zone -mno-sse -mno-sse2 -mno-avx -mno-mmx -Iinclude -c string.c -o string.o
clang -target x86_64-unknown-elf -ffreestanding -O2 -Wall -Wextra -mcmodel=kernel -fno-pic -mno-red-zone -mno-sse -mno-sse2 -mno-avx -mno-mmx -Iinclude -c scheduler.c -o scheduler.o
nasm -f elf64 context_switch.asm -o context_switch.o
clang -target x86_64-unknown-elf -ffreestanding -O2 -Wall -Wextra -mcmodel=kernel -fno-pic -mno-red-zone -mno-sse -mno-sse2 -mno-avx -mno-mmx -mcmodel=large -Iinclude -c user_shell.c -o user_shell.o
clang -target x86_64-unknown-elf -ffreestanding -O2 -Wall -Wextra -mcmodel=kernel -fno-pic -mno-red-zone -mno-sse -mno-sse2 -mno-avx -mno-mmx -mcmodel=large -Iinclude -c userlib.c -o userlib.o
ld.lld -T user_linker.ld -o user_shell.elf user_shell.o userlib.o
xxd -i user_shell.elf > user_shell_data.c
clang -target x86_64-unknown-elf -ffreestanding -O2 -Wall -Wextra -mcmodel=kernel -fno-pic -mno-red-zone -mno-sse -mno-sse2 -mno-avx -mno-mmx -Iinclude -c user_shell_data.c -o user_shell_data.o
ld.lld -nostdlib -T linker.ld -o kernel.elf entry.o ring3_entry.o kmain.o pmm.o vmm.o heap.o idt.o interrupts.o idt_load.o isr.o vga.o keyboard.o serial.o tss.o gdt.o syscall.o elf.o test_program_data.o user_syscall.o user_syscall_entry.o process.o string.o scheduler.o context_switch.o user_shell_data.o
objcopy -O binary kernel.elf kernel.bin
make[1]: Leaving directory '/home/noneya/code/dons-os-x86_64/04_kernel_64bit'
make -C 05_boot_kernel64
make[1]: Entering directory '/home/noneya/code/dons-os-x86_64/05_boot_kernel64'
nasm -f bin boot.asm -o boot.bin
nasm -f bin stage2.asm -o stage2.bin
rm -f hdd.img
qemu-img create -f raw hdd.img 20M
Formatting 'hdd.img', fmt=raw size=20971520
dd if=boot.bin of=hdd.img conv=notrunc
dd if=stage2.bin of=hdd.img bs=512 seek=1 conv=notrunc
dd if=../04_kernel_64bit/kernel.bin of=hdd.img bs=512 seek=64 conv=notrunc
make[1]: Leaving directory '/home/noneya/code/dons-os-x86_64/05_boot_kernel64'
make -C 05_boot_kernel64 run
make[1]: Entering directory '/home/noneya/code/dons-os-x86_64/05_boot_kernel64'
qemu-system-x86_64 -drive file=hdd.img,format=raw -serial stdio -display sdl -monitor none
serial_init
BootInfo validated successfully
Memory map entries: 7
Kernel physical: 0x100000 - 0x110000
PML4 physical: 0x11000
idt_init
pit_init
pm_init
PMM: Total pages: 32640
PMM: Free pages: 32464
vmm_init
VMM: Initializing...
VMM: Detected 127 MB usable RAM
VMM: Highest physical address: 0x7FE0000
VMM: Mapping physical memory into HHDM...
VMM: HHDM mapping complete.
VMM: CR3 = 0x11000
VMM: NX support available
VMM: Initialization complete
heap_init
HEAP: Initializing...
VMM: Allocated PDPT at 0x7FDE000
VMM: Allocated PD at 0x7FDD000
VMM: Allocated PT at 0x7FDC000
HEAP: Initialized at 0xFFFF900000000000
scheduler_init
SCHEDULER: Initializing...
SCHEDULER: Initialization complete
process_init
PROCESS: Initializing...
PROCESS: Created process 1 (idle) entry=0x0 cr3=0x11000 kernel_stack=0xFFFFFFFF8011F100
PROCESS: Initialization complete. 1 processes ready.
GDT: Old user code at 0x30 = 0xAFF2000000FFFF
GDT: Old user data at 0x28 = 0xAFFA000000FFFF
GDT: New user data at 0x28 = 0xCFF2000000FFFF
GDT: New user code at 0x30 = 0xAFFA000000FFFF
GDT: User segments fixed for Ring 3

=== GDT DEBUG ===
GDT Base: 0x10125 Limit: 0x47
GDT+0x0 (selector 0x0): 0x0 [Not Present]
GDT+0x8 (selector 0x8): 0xCF9A000000FFFF [Code DPL=0]
GDT+0x10 (selector 0x10): 0xCF93000000FFFF [Data DPL=0]
GDT+0x18 (selector 0x18): 0xAF9A000000FFFF [Code DPL=0 64-bit]
GDT+0x20 (selector 0x20): 0xAF93000000FFFF [Data DPL=0 64-bit]
GDT+0x28 (selector 0x28): 0xCFF2000000FFFF [Data DPL=3 USER]
GDT+0x30 (selector 0x30): 0xAFFA000000FFFF [Code DPL=3 64-bit USER]
GDT+0x38 (selector 0x38): 0x0 [Not Present]
GDT+0x40 (selector 0x40): 0x0 [Not Present]
=== END GDT DEBUG ===

=== GDT FOCUSED DUMP ===
Kernel code (0x08): 0xCF9A000000FFFF
Kernel data (0x10): 0xCF93000000FFFF
User data   (0x28): 0xCFF2000000FFFF
User code   (0x30): 0xAFFA000000FFFF
========================

tss_init
=== TSS INIT START ===
TSS: vmm_get_phys(0x5000) = 0x5000
TSS: probing 0x5000...
TSS: read ok
TSS: write ok
TSS: tss pointer = 0x5000
TSS: sizeof(tss_t) = 0x68
TSS: TSS_TOTAL_SIZE = 0x2069
TSS: iomap pointer = 0x5068
TSS: zeroing range [0x5000 .. 0x7069)
TSS: zeroed 0x400 bytes
TSS: zeroed 0x800 bytes
TSS: zeroed 0xC00 bytes
TSS: zeroed 0x1000 bytes
TSS: zeroed 0x1400 bytes
TSS: zeroed 0x1800 bytes
TSS: zeroed 0x1C00 bytes
TSS: zeroed 0x2000 bytes
TSS: Zeroing loop completed
TSS: I/O Permission Bitmap terminator set at 0x7068
TSS: Setting rsp0...
TSS: rsp0 = 0xFFFFFFFF801187E0
TSS: Setting iopb_base...
TSS: iopb_base = 0x68
TSS: Calling gdt_set_tss...
GDT: Setting TSS in bootloader GDT...
GDT: Bootloader GDT at 0x10125 (limit: 0x47)
GDT: TSS descriptor at GDT+0x38
GDT: TSS base=0x5000 size=0x2068
GDT: TSS descriptor low=0x890050002068
GDT: TSS descriptor high=0x0
GDT: Reading back TSS descriptor...
  Low: 0x890050002068
  High: 0x0
GDT: TSS descriptor set successfully
TSS: gdt_set_tss completed
TSS: Loading TR with selector 0x38...
TSS: about to execute ltr 0x38
TSS: ltr completed successfully
TSS: TR loaded
TSS: TR register = 0x38

=== GDT DEBUG ===
GDT Base: 0x10125 Limit: 0x47
GDT+0x0 (selector 0x0): 0x0 [Not Present]
GDT+0x8 (selector 0x8): 0xCF9A000000FFFF [Code DPL=0]
GDT+0x10 (selector 0x10): 0xCF93000000FFFF [Data DPL=0]
GDT+0x18 (selector 0x18): 0xAF9A000000FFFF [Code DPL=0 64-bit]
GDT+0x20 (selector 0x20): 0xAF93000000FFFF [Data DPL=0 64-bit]
GDT+0x28 (selector 0x28): 0xCFF2000000FFFF [Data DPL=3 USER]
GDT+0x30 (selector 0x30): 0xAFFA000000FFFF [Code DPL=3 64-bit USER]
GDT+0x38 (selector 0x38): 0x8B0050002068 [System DPL=0]
GDT+0x40 (selector 0x40): 0x0 [Not Present]
=== END GDT DEBUG ===

TSS: TR loaded correctly!
=== TSS INIT COMPLETE ===
Testing I/O permission...
I/O port 0x64 readable, value=0x1C
Initializing **RING** 0 syscalls...
SYSCALL init done
RING 0 Done.
Initializing **RING** 3 syscalls...
Initializing **RING** 3 syscalls...
SYSCALL init done
LSTAR = 0xFFFFFFFF801068DE
STAR = 0x20001800000000
RING 3 Done.
LSTAR = 0xFFFFFFFF801068DE
Done.
DonsDOS v0.4.6
Type 'help'
> USER_SHELL: Starting user shell
USER_SHELL: Size = 7936 bytes
USER_SHELL: Calling elf_load()
ELF: Loading
ELF: Entry=0x8000000000
ELF: Program headers=3
VMM: Allocated PDPT at 0x7EDB000
VMM: Allocated PD at 0x7EDA000
VMM: Allocated PT at 0x7ED9000
PROCESS: Created process 2 (elf_prog) entry=0x8000000000 cr3=0x11000 kernel_stack=0xFFFFFFFF80121100 user_stack_top=0x7FFFFFE00FF0
ELF: Scanning program headers
ELF: Header 0 type=1
ELF: LOAD vaddr=0x8000000000 memsz=0x80E filesz=0x80E offset=0x1000
ELF: start_page=0x8000000000 end_page=0x8000001000 num_pages=1
ELF: Mapping new page virt=0x8000000000 phys=0x7ED8000
VMM: Allocated PDPT at 0x7ED7000
VMM: Allocated PD at 0x7ED6000
VMM: Allocated PT at 0x7ED5000
ELF: Header 1 type=1
ELF: LOAD vaddr=0x800000080E memsz=0x183 filesz=0x183 offset=0x180E
ELF: start_page=0x8000000000 end_page=0x8000001000 num_pages=1
ELF: Page already mapped at 0x8000000000 phys=0x7ED8000 - remapping with user flags
ELF: Header 2 type=1685382481
ELF: Copying filesz=0x80E to 0x8000000000 via HHDM (paged)
ELF: Verified data at 0x8000000000: 55 89 41 41 41 41 53 81 88 0 48 A5 0 80 0 49 
ELF: Copying filesz=0x183 to 0x800000080E via HHDM (paged)
ELF: Verified data at 0x800000080E: 55 6B 6F 6E 63 6D 61 64 20 20 76 72 69 6E 2D 53 
ELF: Process created successfully
ELF: Starting process via scheduler
SCHEDULER: Switching from PID 1 (idle) to PID 2 (elf_prog)
sys_write: fd=1 buf=0x80000008A5 count=0
sys_write: copying 0 bytes from user
sys_write: data=''
sys_write: fd=1 buf=0x80000008B6 count=0
sys_write: copying 0 bytes from user
sys_write: data=''
sys_write: fd=1 buf=0x80000008DE count=0
sys_write: copying 0 bytes from user
sys_write: data=''
sys_read: fd=0 buf=0x7FFFFFE00F17 count=1
make[1]: Leaving directory '/home/noneya/code/dons-os-x86_64/05_boot_kernel64'




