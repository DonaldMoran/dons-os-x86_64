CC      := clang
LD      := ld.lld
AS      := nasm
OBJCOPY := objcopy

CFLAGS  := -target x86_64-unknown-elf -ffreestanding -O2 -Wall -Wextra -mcmodel=kernel -fno-pic -mno-red-zone
LDFLAGS := -nostdlib

.PHONY: all clean

all: kernel.bin

# ---------------------------------------------------------
# Add ALL kernel object files here
# ---------------------------------------------------------
OBJS = entry.o kmain.o pmm.o idt.o interrupts.o idt_load.o isr.o vga.o keyboard.o vmm.o serial.o

kernel.elf: $(OBJS)
	$(LD) $(LDFLAGS) -T linker.ld -o kernel.elf $(OBJS)

kernel.bin: kernel.elf
	$(OBJCOPY) -O binary kernel.elf kernel.bin

vga.o: vga.c
	$(CC) $(CFLAGS) -Iinclude -I. -c vga.c -o vga.o

keyboard.o: keyboard.c
	$(CC) $(CFLAGS) -Iinclude -c keyboard.c -o keyboard.o


# ---------------------------------------------------------
# Assembly sources
# ---------------------------------------------------------
entry.o: entry.asm
	$(AS) -f elf64 entry.asm -o entry.o

idt_load.o: idt_load.asm
	$(AS) -f elf64 idt_load.asm -o idt_load.o

isr.o: isr.asm
	$(AS) -f elf64 isr.asm -o isr.o

# ---------------------------------------------------------
# C sources
# ---------------------------------------------------------
kmain.o: kmain.c
	$(CC) $(CFLAGS) -Iinclude -c kmain.c -o kmain.o

pmm.o: pmm.c
	$(CC) $(CFLAGS) -Iinclude -c pmm.c -o pmm.o

idt.o: idt.c
	$(CC) $(CFLAGS) -Iinclude -c idt.c -o idt.o

interrupts.o: interrupts.c
	$(CC) $(CFLAGS) -Iinclude -c interrupts.c -o interrupts.o

clean:
	rm -f *.o kernel.elf kernel.bin

vmm.o: vmm.c
	$(CC) $(CFLAGS) -Iinclude -c vmm.c -o vmm.o

serial.o: serial.c
	$(CC) $(CFLAGS) -Iinclude -c serial.c -o serial.o
