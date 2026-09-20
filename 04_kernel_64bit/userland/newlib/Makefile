# =======================================================================
# DonsDOS Newlib Userland Makefile
#
# Builds:
#   build/user_shell.elf    — embedded in the kernel image
#   apps/hello.elf          — standalone disk-loaded ELF
#
# The two targets differ only in what happens after linking:
#   - user_shell.elf is stripped and xxd -i'd into a C array.
#   - hello.elf is stripped and left as an ELF on disk.
# Both link against the same crt0, syscalls, reent, and libc.a.
# =======================================================================

CC = /opt/cross/bin/x86_64-elf-gcc
LD = /opt/cross/bin/x86_64-elf-gcc
STRIP = /opt/cross/bin/x86_64-elf-strip

CFLAGS = -Iinclude -Iapps/include -O2 -Wall -Wextra -fno-pic -mno-red-zone \
         -mno-sse -mno-sse2 -mno-avx -mno-mmx -mcmodel=large -fno-common -g \
         -fno-stack-protector -Wno-missing-field-initializers -fno-builtin-memset

LDFLAGS = -T ../../user_newlib_linker.ld -nostdlib -mcmodel=large -Wl,--no-warn-rwx-segments -Llib

SHARED = build/arc2/crt0.o build/arc2/syscalls.o build/arc2/reent.o

all: prep build/user_shell.elf ../../user_shell_data.c apps/hello.elf

prep:
	@mkdir -p build/arc2 build/apps

build/arc2/crt0.o: arc2/crt0.S
	$(CC) $(CFLAGS) -c arc2/crt0.S -o build/arc2/crt0.o

build/arc2/syscalls.o: arc2/syscalls.c
	$(CC) $(CFLAGS) -c arc2/syscalls.c -o build/arc2/syscalls.o

build/arc2/reent.o: arc2/reent.c
	$(CC) $(CFLAGS) -c arc2/reent.c -o build/arc2/reent.o

# ---- user_shell (embedded) ----
build/apps/user_shell.o: apps/user_shell.c
	$(CC) $(CFLAGS) -c apps/user_shell.c -o build/apps/user_shell.o

build/user_shell.elf: build/apps/user_shell.o $(SHARED)
	$(LD) $(LDFLAGS) -o build/user_shell.elf $(SHARED) build/apps/user_shell.o -lc

build/user_shell.elf.stripped: build/user_shell.elf
	$(STRIP) --strip-debug build/user_shell.elf -o build/user_shell.elf.stripped
	@echo "user_shell.elf          : $$(stat -c %s build/user_shell.elf) bytes"
	@echo "user_shell.elf.stripped : $$(stat -c %s build/user_shell.elf.stripped) bytes"

../../user_shell_data.c: build/user_shell.elf.stripped
	cp build/user_shell.elf.stripped build/user_shell.elf
	xxd -i build/user_shell.elf > ../../user_shell_data.c

# ---- hello (standalone, disk-loaded) ----
build/apps/hello.o: apps/hello.c
	$(CC) $(CFLAGS) -c apps/hello.c -o build/apps/hello.o

apps/hello.elf: build/apps/hello.o $(SHARED)
	$(LD) $(LDFLAGS) -o apps/hello.elf $(SHARED) build/apps/hello.o -lc
	$(STRIP) --strip-debug apps/hello.elf -o apps/hello.elf.tmp
	mv apps/hello.elf.tmp apps/hello.elf
	@echo "hello.elf               : $$(stat -c %s apps/hello.elf) bytes (disk-loaded)"

.PHONY: hello
hello: apps/hello.elf

clean:
	rm -rf build ../../user_shell_data.c apps/hello.elf apps/hello.elf.tmp
