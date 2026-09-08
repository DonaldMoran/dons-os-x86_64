CC      := clang
LD      := ld.lld
AS      := nasm
OBJCOPY := objcopy
XXD     := xxd

# Userland directory - only newlib now
USERLAND_DIR := userland/newlib

# Kernel CFLAGS
CFLAGS  := -target x86_64-unknown-elf -ffreestanding -O2 -Wall -Wextra \
	-mcmodel=kernel -fno-pic -mno-red-zone -mno-sse -mno-sse2 \
	-mno-avx -mno-mmx

# User-space CFLAGS (for kernel-side objects that need to embed user data)
USER_CFLAGS := -target x86_64-unknown-elf -ffreestanding -O0 -Wall -Wextra \
	-fno-pic -mno-red-zone -mno-sse -mno-sse2 -mno-avx -mno-mmx \
	-mcmodel=large -fno-builtin -fno-common -Iinclude

LDFLAGS := -nostdlib

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
	context_switch.o user_shell_data.o

kernel.elf: $(OBJS)
	$(LD) $(LDFLAGS) -T linker.ld -o kernel.elf $(OBJS)

kernel.bin: kernel.elf
	$(OBJCOPY) -O binary kernel.elf kernel.bin

# ============================================================
# TEST PROGRAM
# ============================================================
test_program_data.o: test_program_data.c
	$(CC) $(CFLAGS) -Iinclude -c test_program_data.c -o test_program_data.o

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
	sed -i 's/unsigned char build_user_shell_elf/__attribute__((section(".userelf"))) unsigned char build_user_shell_elf/' $<
	$(CC) $(CFLAGS) -Iinclude -c $< -o $@

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
	$(CC) $(CFLAGS) -Iinclude -c $< -o $@

vga.o: vga.c
	$(CC) $(CFLAGS) -Iinclude -I. -c vga.c -o vga.o

syscall.o: syscall.c
	$(CC) $(CFLAGS) -Iinclude -I. -c syscall.c -o syscall.o

elf.o: elf.c
	$(CC) $(CFLAGS) -Iinclude -I. -c elf.c -o elf.o

user_syscall.o: user_syscall.c
	$(CC) $(CFLAGS) -Iinclude -I. -c user_syscall.c -o user_syscall.o

# ============================================================
# CLEAN
# ============================================================
clean:
	rm -f *.o *.bin *.elf test_program test_program.bin test_program_data.c
	rm -f user_shell.elf user_shell_data.c
	rm -f user_shell.bin
	$(MAKE) -C $(USERLAND_DIR) clean

