CC      := clang
LD      := ld.lld
AS      := nasm
OBJCOPY := objcopy
XXD     := xxd

# Userland directory - only newlib now
USERLAND_DIR := userland/newlib

# Disk layout configuration. Selects which drive FatFs talks to and
# which image layout the boot chain expects.
#
#   FAT_CONFIG=dual   (default) hdd.img master + fat.img slave
#   FAT_CONFIG=single            hdd.img only, FAT partition at LBA 2048
#
# See include/fat_config.h for the layout descriptions.
FAT_CONFIG ?= dual

ifeq ($(FAT_CONFIG),single)
FAT_CFLAGS := -DFAT_CONFIG_SINGLE_DRIVE=1
else
FAT_CFLAGS := -DFAT_CONFIG_SINGLE_DRIVE=0
endif

# Kernel CFLAGS - Added -Ifatfs here so all files can parse ff.h natively
CFLAGS  := -target x86_64-unknown-elf -ffreestanding -O2 -Wall -Wextra \
	-mcmodel=kernel -fno-pic -mno-red-zone -mno-sse -mno-sse2 \
	-mno-avx -mno-mmx -Iinclude -Ifatfs $(FAT_CFLAGS)

# User-space CFLAGS (for kernel-side objects that need to embed user data)
USER_CFLAGS := -target x86_64-unknown-elf -ffreestanding -O0 -Wall -Wextra \
	-fno-pic -mno-red-zone -mno-sse -mno-sse2 -mno-avx -mno-mmx \
	-mcmodel=large -fno-builtin -fno-common -Iinclude

LDFLAGS := -nostdlib

# Two limits apply to kernel.bin:
#
# 1. KERNEL_STAGING_LIMIT — the ceiling imposed by stage2.asm's staging
#    layout. 128 KB staged at 0x80000 (PASS 1+2) plus 320 KB from
#    staging B at 0x20000..0x7FFFF (PASS 3..7), for 448 KB total.
#    Exceed this and stage2 silently truncates the tail and the kernel
#    boots to garbage. See stage2.asm.
#
# 2. KERNEL_DISK_LIMIT — the ceiling imposed by the disk layout. The
#    kernel starts at LBA 128; the FAT partition starts at LBA 2048.
#    The kernel region is therefore (2048 - 128) * 512 = 983040 bytes.
#    Exceed this and the kernel would be overwritten by the FAT volume.
#
# The staging limit is the binding one today at 448 KB. The disk limit
# (983 KB) becomes the binding one only if the staging architecture is
# changed again (Option 2 or Option 6 in the roadmap). Both are checked
# below. See MAINTENANCE.md item 1 for the full set of options.
KERNEL_STAGING_LIMIT := 458752
KERNEL_DISK_LIMIT    := 983040

.PHONY: all clean userland

# Build both kernel and userland
all: userland kernel.bin

# Build userland - now directly calls newlib Makefile
userland:
	$(MAKE) -C $(USERLAND_DIR)

# ============================================================
# KERNEL OBJECTS
# ============================================================
OBJS = entry.o ring3_entry.o kmain.o pmm.o vmm.o heap.o idt.o interrupts.o \
	idt_load.o isr.o vga.o keyboard.o serial.o tss.o gdt.o \
	elf.o test_program_data.o \
	user_syscall.o user_syscall_entry.o process.o string.o scheduler.o \
	context_switch.o user_shell_data.o ata.o \
	fatfs/ff.o fatfs/diskio.o

kernel.elf: $(OBJS)
	$(LD) $(LDFLAGS) -T linker.ld -o kernel.elf $(OBJS)

kernel.bin: kernel.elf
	$(OBJCOPY) -O binary kernel.elf kernel.bin
	@sz=$$(stat -c %s kernel.bin); \
	if [ $$sz -gt $(KERNEL_STAGING_LIMIT) ]; then \
		echo "============================================================"; \
		echo "ERROR: kernel.bin is $$sz bytes"; \
		echo "       staging limit is $(KERNEL_STAGING_LIMIT) bytes"; \
		echo "       overflow: $$((sz - $(KERNEL_STAGING_LIMIT))) bytes"; \
		echo ""; \
		echo "  stage2.asm can only stage 448 KB (128 KB at 0x80000 plus"; \
		echo "  320 KB at 0x20000..0x7FFFF). The kernel image will be"; \
		echo "  silently truncated and boot to garbage."; \
		echo ""; \
		echo "  Shrink the kernel, or restructure stage2.asm to stream"; \
		echo "  reads (see MAINTENANCE.md item 1)."; \
		echo "============================================================"; \
		exit 1; \
	fi; \
	if [ $$sz -gt $(KERNEL_DISK_LIMIT) ]; then \
		echo "============================================================"; \
		echo "ERROR: kernel.bin is $$sz bytes"; \
		echo "       disk layout limit is $(KERNEL_DISK_LIMIT) bytes"; \
		echo "       (kernel region is LBA 128..2047)"; \
		echo "       overflow: $$((sz - $(KERNEL_DISK_LIMIT))) bytes"; \
		echo ""; \
		echo "  The kernel would overlap the FAT partition at LBA 2048."; \
		echo "  Move the partition start in 05_boot_kernel64/Makefile"; \
		echo "  and update FAT_DISK_TOTAL_SECTORS in include/fat_config.h."; \
		echo "============================================================"; \
		exit 1; \
	fi; \
	echo "kernel.bin: $$sz bytes / staging $(KERNEL_STAGING_LIMIT) / disk $(KERNEL_DISK_LIMIT) (staging headroom: $$(($(KERNEL_STAGING_LIMIT) - sz)) bytes)"

# ============================================================
# TEST PROGRAM
# ============================================================
test_program_data.o: test_program_data.c
	$(CC) $(CFLAGS) -c test_program_data.c -o test_program_data.o

test_program_data.c: test_program
	$(XXD) -i test_program > test_program_data.c

test_program: test_program.o user_linker.ld
	$(LD) -T user_linker.ld -o test_program test_program.o

test_program.o: test_program.asm
	$(AS) -f elf64 test_program.asm -o test_program.o

# ============================================================
# USER SHELL (embedded ELF blob)
# ============================================================
user_shell_data.o: user_shell_data.c
	sed -i 's/unsigned char build_user_shell_elf/__attribute__((section(".userelf"))) unsigned char build_user_shell_elf/' user_shell_data.c
	sed -i 's/unsigned int build_user_shell_elf_len/const unsigned int build_user_shell_elf_len/' user_shell_data.c
	$(CC) $(USER_CFLAGS) -c user_shell_data.c -o user_shell_data.o

# ============================================================
# ASSEMBLY RULES
# ============================================================
context_switch.o: context_switch.asm
	$(AS) -f elf64 context_switch.asm -o context_switch.o

entry.o: entry.asm
	$(AS) -f elf64 entry.asm -o entry.o

ring3_entry.o: ring3_entry.S
	$(AS) -f elf64 ring3_entry.S -o ring3_entry.o

idt_load.o: idt_load.asm
	$(AS) -f elf64 idt_load.asm -o idt_load.o

isr.o: isr.asm
	$(AS) -f elf64 isr.asm -o isr.o

user_syscall_entry.o: user_syscall_entry.asm
	$(AS) -f elf64 user_syscall_entry.asm -o user_syscall_entry.o

# ============================================================
# C COMPILATION RULES
# ============================================================
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# Catch-all rule specifically compilation within the fatfs subdirectory
fatfs/%.o: fatfs/%.c
	$(CC) $(CFLAGS) -c $< -o $@

vga.o: vga.c
	$(CC) $(CFLAGS) -I. -c vga.c -o vga.o

syscall.o: syscall.c
	$(CC) $(CFLAGS) -I. -c syscall.c -o syscall.o

elf.o: elf.c
	$(CC) $(CFLAGS) -I. -c elf.c -o elf.o

user_syscall.o: user_syscall.c
	$(CC) $(CFLAGS) -I. -c user_syscall.c -o user_syscall.o

# ============================================================
# CLEAN
# ============================================================
clean:
	rm -f *.o *.bin *.elf test_program test_program.bin test_program_data.c
	rm -f user_shell.elf user_shell_data.c
	rm -f user_shell.bin
	rm -f fatfs/*.o
	$(MAKE) -C $(USERLAND_DIR) clean
