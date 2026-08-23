CC      := clang
LD      := ld.lld
AS      := nasm
OBJCOPY := objcopy
XXD     := xxd

# Clean CFLAGS - only what's necessary
CFLAGS  := -target x86_64-unknown-elf -ffreestanding -O2 -Wall -Wextra -mcmodel=kernel -fno-pic -mno-red-zone -mno-sse -mno-sse2 -mno-avx -mno-mmx
LDFLAGS := -nostdlib

.PHONY: all clean

all: kernel.bin

# ============================================================
# KERNEL OBJECTS - Add user_shell_data.o here
# ============================================================
OBJS = entry.o ring3_entry.o kmain.o pmm.o vmm.o heap.o idt.o interrupts.o \
	idt_load.o isr.o vga.o keyboard.o serial.o tss.o gdt.o \
	syscall.o elf.o test_program_data.o \
	user_syscall.o user_syscall_entry.o process.o string.o scheduler.o \
	context_switch.o user_shell_data.o

kernel.elf: $(OBJS)
	$(LD) $(LDFLAGS) -T linker.ld -o kernel.elf $(OBJS)

kernel.bin: kernel.elf
	$(OBJCOPY) -O binary kernel.elf kernel.bin

# ============================================================
# TEST PROGRAM (existing)
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
# USER SHELL (C)
# ============================================================

#~ # User library
#~ userlib.o: userlib.c include/userlib.h
#~ 	$(CC) $(CFLAGS) -mcmodel=large -fPIC -Iinclude -c userlib.c -o userlib.o

#~ # User shell object
#~ user_shell.o: user_shell.c include/userlib.h
#~ 	$(CC) $(CFLAGS) -mcmodel=large -fPIC -Iinclude -c user_shell.c -o user_shell.o

# User library
userlib.o: userlib.c include/userlib.h
	$(CC) $(CFLAGS) -mcmodel=large -Iinclude -c userlib.c -o userlib.o

# User shell object
user_shell.o: user_shell.c include/userlib.h
	$(CC) $(CFLAGS) -mcmodel=large -Iinclude -c user_shell.c -o user_shell.o

# User shell ELF (embed the ELF file directly, NOT the binary)
user_shell.elf: user_shell.o userlib.o
	$(LD) -T user_linker.ld -o user_shell.elf user_shell.o userlib.o

# Generate the embedded data file from the user shell ELF
user_shell_data.c: user_shell.elf
	$(XXD) -i user_shell.elf > user_shell_data.c

# Compile the embedded data
user_shell_data.o: user_shell_data.c
	$(CC) $(CFLAGS) -Iinclude -c user_shell_data.c -o user_shell_data.o

# ============================================================
# USER SHELL (Assembly - no compiler/linker issues)
# ============================================================

#~ # User shell object (assembly)
#~ user_shell.o: user_shell.asm
#~ 	$(AS) -f elf64 user_shell.asm -o user_shell.o

#~ # User shell ELF
#~ user_shell.elf: user_shell.o
#~ 	$(LD) -T user_linker.ld -o user_shell.elf user_shell.o

#~ # Generate the embedded data file from the user shell ELF
#~ user_shell_data.c: user_shell.elf
#~ 	$(XXD) -i user_shell.elf > user_shell_data.c

#~ # Compile the embedded data
#~ user_shell_data.o: user_shell_data.c
#~ 	$(CC) $(CFLAGS) -Iinclude -c user_shell_data.c -o user_shell_data.o

# ============================================================
# ASSEMBLY RULES (existing)
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
# C COMPILATION RULES (existing)
# ============================================================
%.o: %.c
	$(CC) $(CFLAGS) -Iinclude -c $< -o $@

# Special rules for files with extra includes
vga.o: vga.c
	$(CC) $(CFLAGS) -Iinclude -I. -c vga.c -o vga.o

syscall.o: syscall.c
	$(CC) $(CFLAGS) -Iinclude -I. -c syscall.c -o syscall.o

elf.o: elf.c
	$(CC) $(CFLAGS) -Iinclude -I. -c elf.c -o elf.o

user_syscall.o: user_syscall.c
	$(CC) $(CFLAGS) -Iinclude -I. -c user_syscall.c -o user_syscall.o

# ============================================================
# CLEAN (updated)
# ============================================================
clean:
	rm -f *.o *.bin *.elf test_program test_program.bin test_program_data.c
	rm -f user_shell.elf user_shell_data.c
	rm -f user_shell.bin
