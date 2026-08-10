AS := nasm

.PHONY: all clean run run-log run-debug run-verbose run-headless run-kvm run-telnet watch-serial clean-logs

all: boot.bin stage2.bin hdd.img

# ---------------------------------------------------------
# Build targets
# ---------------------------------------------------------
boot.bin: boot.asm
	$(AS) -f bin boot.asm -o boot.bin

stage2.bin: stage2.asm
	$(AS) -f bin stage2.asm -o stage2.bin

hdd.img: boot.bin stage2.bin ../04_kernel_64bit/kernel.bin
	rm -f hdd.img
	qemu-img create -f raw hdd.img 20M
	dd if=boot.bin of=hdd.img conv=notrunc
	dd if=stage2.bin of=hdd.img bs=512 seek=1 conv=notrunc
	dd if=../04_kernel_64bit/kernel.bin of=hdd.img bs=512 seek=64 conv=notrunc

# ---------------------------------------------------------
# QEMU Run Targets
# ---------------------------------------------------------

# Default: VGA + Serial in terminal (best for development)
run:
	qemu-system-x86_64 -drive file=hdd.img,format=raw -serial stdio -display sdl -monitor none

# Serial output to file (view later with 'cat serial.log')
run-log:
	qemu-system-x86_64 -drive file=hdd.img,format=raw -serial file:serial.log -display sdl

# Debug mode with GDB server (port 1234)
# Connect with: gdb -ex "target remote localhost:1234" kernel.elf
run-debug:
	qemu-system-x86_64 -drive file=hdd.img,format=raw -serial stdio -display sdl -s -S

# Full debug with logging
run-verbose:
	qemu-system-x86_64 -drive file=hdd.img,format=raw \
		-serial stdio \
		-display sdl \
		-d int,cpu_reset,guest_errors,page \
		-no-reboot -no-shutdown \
		-D qemu_debug.log

# No graphics, serial to stdio (headless - good for servers)
run-headless:
	qemu-system-x86_64 -drive file=hdd.img,format=raw -serial stdio -nographic -monitor none

# With KVM acceleration (faster, if available)
run-kvm:
	qemu-system-x86_64 -drive file=hdd.img,format=raw -serial stdio -display sdl -enable-kvm -cpu host

# With telnet serial (connect with 'telnet localhost 4444')
run-telnet:
	qemu-system-x86_64 -drive file=hdd.img,format=raw -serial telnet::4444,server,nowait -display sdl

# Legacy debug targets (kept for compatibility)
log:
	qemu-system-x86_64 -drive file=hdd.img,format=raw \
		-d int,cpu_reset,guest_errors,unimp,page \
		-no-reboot -no-shutdown \
		-serial file:qemu.log \
		-D qemu_debug.log \
		-monitor stdio

debug:
	qemu-system-x86_64 -drive file=hdd.img,format=raw \
		-s -S \
		-no-reboot -no-shutdown \
		-d int,cpu_reset,guest_errors \
		-serial file:qemu.log \
		-monitor stdio

# ---------------------------------------------------------
# Utility targets
# ---------------------------------------------------------

# Watch serial log in real-time (run in another terminal)
watch-serial:
	tail -f serial.log

# Clean up log files
clean-logs:
	rm -f serial.log qemu.log qemu_debug.log

# ---------------------------------------------------------
# Clean
# ---------------------------------------------------------
clean: clean-logs
	rm -f boot.bin stage2.bin hdd.img
