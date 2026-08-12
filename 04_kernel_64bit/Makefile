CC      := clang
LD      := ld.lld
AS      := nasm
OBJCOPY := objcopy

CFLAGS  := -target x86_64-unknown-elf -ffreestanding -O2 -Wall -Wextra -mcmodel=kernel -fno-pic -mno-red-zone
LDFLAGS := -nostdlib

.PHONY: all clean

all: kernel.bin

OBJS = entry.o ring3_entry.o kmain.o pmm.o vmm.o heap.o idt.o interrupts.o \
       idt_load.o isr.o vga.o keyboard.o serial.o tss.o gdt.o ring3.o \
       syscall.o syscall_init.o

kernel.elf: $(OBJS)
	$(LD) $(LDFLAGS) -T linker.ld -o kernel.elf $(OBJS)

kernel.bin: kernel.elf
	$(OBJCOPY) -O binary kernel.elf kernel.bin

idt_load.o: idt_load.asm
	$(AS) -f elf64 idt_load.asm -o idt_load.o

isr.o: isr.asm
	$(AS) -f elf64 isr.asm -o isr.o

entry.o: entry.asm
	$(AS) -f elf64 entry.asm -o entry.o

ring3_entry.o: ring3_entry.S
	$(AS) -f elf64 ring3_entry.S -o ring3_entry.o

syscall_init.o: syscall_init.asm
	$(AS) -f elf64 syscall_init.asm -o syscall_init.o

%.o: %.c
	$(CC) $(CFLAGS) -Iinclude -c $< -o $@

# Specific rules for C files that need include path
vga.o: vga.c
	$(CC) $(CFLAGS) -Iinclude -I. -c vga.c -o vga.o

syscall.o: syscall.c
	$(CC) $(CFLAGS) -Iinclude -I. -c syscall.c -o syscall.o

clean:
	rm -f *.o kernel.elf kernel.bin
