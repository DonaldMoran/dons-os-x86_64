AS := nasm

.PHONY: all clean run run-log run-debug run-verbose run-headless run-kvm run-telnet \
        run-single run-kvm-single watch-serial clean-logs hello-elf

all: boot.bin stage2.bin hdd.img

# ---------------------------------------------------------
# Disk layout constants (must match include/fat_config.h)
# ---------------------------------------------------------
SINGLE_DRIVE_LBA := 2048
DISK_SECTORS     := 40960

# Path to the standalone user program built by the userland Makefile.
HELLO_ELF := ../04_kernel_64bit/userland/newlib/apps/hello.elf

# ---------------------------------------------------------
# Build targets
# ---------------------------------------------------------
boot.bin: boot.asm
	$(AS) -f bin boot.asm -o boot.bin

stage2.bin: stage2.asm
	$(AS) -f bin stage2.asm -o stage2.bin

# Build the disk-loaded hello.elf. Delegates to the userland Makefile
# so this Makefile does not need to know how a user program is built.
hello-elf:
	$(MAKE) -C ../04_kernel_64bit/userland/newlib hello

# Dual-drive image: hdd.img (boot chain + kernel) + fat.img (slave).
hdd.img: boot.bin stage2.bin ../04_kernel_64bit/kernel.bin
	rm -f hdd.img
	qemu-img create -f raw hdd.img 20M
	dd if=boot.bin of=hdd.img conv=notrunc
	dd if=stage2.bin of=hdd.img bs=512 seek=1 conv=notrunc
	dd if=../04_kernel_64bit/kernel.bin of=hdd.img bs=512 seek=128 conv=notrunc

# Single-drive image: boot chain + kernel + FAT16 partition at LBA 2048.
# The test-files/ directory is copied by mcopy, and HELLO.ELF is copied
# from the userland build output.
hdd-single.img: boot.bin stage2.bin ../04_kernel_64bit/kernel.bin hello-elf
	rm -f hdd-single.img
	qemu-img create -f raw hdd-single.img 20M
	dd if=boot.bin of=hdd-single.img conv=notrunc
	dd if=stage2.bin of=hdd-single.img bs=512 seek=1 conv=notrunc
	dd if=../04_kernel_64bit/kernel.bin of=hdd-single.img bs=512 seek=128 conv=notrunc
	mkfs.vfat -F 16 -h $(SINGLE_DRIVE_LBA) -n DONSOS \
	    --offset=$(SINGLE_DRIVE_LBA) hdd-single.img
	@for f in ../test-files/*; do \
	    if [ -f "$$f" ]; then \
	        echo "mcopy $$f"; \
	        mcopy -i hdd-single.img@@$$(( $(SINGLE_DRIVE_LBA) * 512 )) "$$f" ::/ ; \
	    fi \
	done
	@if [ -f "$(HELLO_ELF)" ]; then \
	    echo "mcopy $(HELLO_ELF) -> ::/HELLO.ELF"; \
	    mcopy -i hdd-single.img@@$$(( $(SINGLE_DRIVE_LBA) * 512 )) \
	        "$(HELLO_ELF)" ::/HELLO.ELF ; \
	else \
	    echo "WARN: $(HELLO_ELF) not found; HELLO.ELF will be missing from the image"; \
	fi
	@echo "--- FAT partition contents (hdd-single.img) ---"
	@mdir -i hdd-single.img@@$$(( $(SINGLE_DRIVE_LBA) * 512 )) ::/ || true
	@echo "--- verifying FAT boot sector at LBA $(SINGLE_DRIVE_LBA) ---"
	@dd if=hdd-single.img bs=512 skip=$(SINGLE_DRIVE_LBA) count=1 2>/dev/null | \
	    xxd | head -2
	@echo "--- hidden_sectors field (BPB offset 0x1C), expect 0008 0000 ---"
	@dd if=hdd-single.img bs=1 skip=$$(( $(SINGLE_DRIVE_LBA) * 512 + 28 )) \
	    count=4 2>/dev/null | xxd

# ---------------------------------------------------------
# QEMU Run Targets
# ---------------------------------------------------------

run:
	qemu-system-x86_64 -cpu max \
		-drive file=hdd.img,format=raw,if=ide,index=0 \
		-drive file=fat.img,format=raw,if=ide,index=1 \
		-serial stdio -display sdl -monitor none

run-single: hdd-single.img
	qemu-system-x86_64 -cpu max \
		-drive file=hdd-single.img,format=raw,if=ide,index=0 \
		-serial stdio -display sdl -monitor none

run-log:
	qemu-system-x86_64 -cpu max -drive file=hdd.img,format=raw -serial file:serial.log -display sdl

run-debug:
	qemu-system-x86_64 -cpu max -drive file=hdd.img,format=raw -serial stdio -display sdl -s -S

run-verbose:
	qemu-system-x86_64 -cpu max -drive file=hdd.img,format=raw \
		-serial stdio \
		-display sdl \
		-d int,cpu_reset,guest_errors,page \
		-no-reboot -no-shutdown \
		-D qemu_debug.log

run-headless:
	qemu-system-x86_64 -cpu max -drive file=hdd.img,format=raw -serial stdio -nographic -monitor none

run-kvm:
	qemu-system-x86_64 -drive file=hdd.img,format=raw,if=ide,index=0 \
		-drive file=fat.img,format=raw,if=ide,index=1 \
		-serial stdio \
		-display sdl \
		-enable-kvm \
		-cpu host

run-kvm-single: hdd-single.img
	qemu-system-x86_64 -drive file=hdd-single.img,format=raw,if=ide,index=0 \
		-serial stdio \
		-display sdl \
		-enable-kvm \
		-cpu host

run-telnet:
	qemu-system-x86_64 -cpu max -drive file=hdd.img,format=raw -serial telnet::4444,server,nowait -display sdl

log:
	qemu-system-x86_64 -cpu max -drive file=hdd.img,format=raw \
		-d int,cpu_reset,guest_errors,unimp,page \
		-no-reboot -no-shutdown \
		-serial file:qemu.log \
		-D qemu_debug.log \
		-monitor stdio

debug:
	qemu-system-x86_64 -cpu max -drive file=hdd.img,format=raw \
		-s -S \
		-no-reboot -no-shutdown \
		-d int,cpu_reset,guest_errors \
		-serial file:qemu.log \
		-monitor stdio

# ---------------------------------------------------------
# Utility
# ---------------------------------------------------------

watch-serial:
	tail -f serial.log

clean-logs:
	rm -f serial.log qemu.log qemu_debug.log

# ---------------------------------------------------------
# Clean
# ---------------------------------------------------------
clean: clean-logs
	rm -f boot.bin stage2.bin hdd.img hdd-single.img
