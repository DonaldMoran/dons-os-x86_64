CC      := clang
LD      := ld.lld
AS      := nasm
OBJCOPY := objcopy

CFLAGS  := -target x86_64-unknown-elf -ffreestanding -O2 -Wall -Wextra -mcmodel=kernel -fno-pic -mno-red-zone
LDFLAGS := -nostdlib

.PHONY: all clean

all: kernel.bin

# ---------------------------------------------------------
# Object files
# ---------------------------------------------------------
OBJS = entry.o kmain.o pmm.o idt.o interrupts.o idt_load.o isr.o vga.o keyboard.o vmm.o serial.o heap.o

# ---------------------------------------------------------
# Kernel build
# ---------------------------------------------------------
kernel.elf: $(OBJS)
	$(LD) $(LDFLAGS) -T linker.ld -o kernel.elf $(OBJS)

kernel.bin: kernel.elf
	$(OBJCOPY) -O binary kernel.elf kernel.bin

# ---------------------------------------------------------
# Assembly sources
# ---------------------------------------------------------
%.o: %.asm
	$(AS) -f elf64 $< -o $@

# Specific assembly files (if they need special flags)
idt_load.o: idt_load.asm
	$(AS) -f elf64 idt_load.asm -o idt_load.o

isr.o: isr.asm
	$(AS) -f elf64 isr.asm -o isr.o

entry.o: entry.asm
	$(AS) -f elf64 entry.asm -o entry.o

# ---------------------------------------------------------
# C sources (pattern rule)
# ---------------------------------------------------------
%.o: %.c
	$(CC) $(CFLAGS) -Iinclude -c $< -o $@

# Special case for vga.c (needs -I. as well)
vga.o: vga.c
	$(CC) $(CFLAGS) -Iinclude -I. -c vga.c -o vga.o

# ---------------------------------------------------------
# Clean
# ---------------------------------------------------------
clean:
	rm -f *.o kernel.elf kernel.bin
