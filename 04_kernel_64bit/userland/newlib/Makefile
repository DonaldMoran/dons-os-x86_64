# =======================================================================
# DonsDOS Newlib User Shell Application Makefile
# Optimized to enforce free-standing loop primitives under -O2 bounds
# =======================================================================

CC = /opt/cross/bin/x86_64-elf-gcc
LD = /opt/cross/bin/x86_64-elf-gcc
STRIP = /opt/cross/bin/x86_64-elf-strip

CFLAGS = -Iinclude -Iapps/include -O2 -Wall -Wextra -fno-pic -mno-red-zone \
         -mno-sse -mno-sse2 -mno-avx -mno-mmx -mcmodel=large -fno-common -g \
         -fno-stack-protector -Wno-missing-field-initializers -fno-builtin-memset

LDFLAGS = -T ../../user_newlib_linker.ld -nostdlib -mcmodel=large -Wl,--no-warn-rwx-segments -Llib

SRCS = arc2/crt0.S arc2/syscalls.c arc2/reent.c apps/user_shell.c
OBJS = build/arc2/crt0.o build/arc2/syscalls.o build/arc2/reent.o build/apps/user_shell.o

all: prep build/user_shell.elf ../../user_shell_data.c

prep:
	@mkdir -p build/arc2 build/apps

build/arc2/crt0.o: arc2/crt0.S
	$(CC) $(CFLAGS) -c arc2/crt0.S -o build/arc2/crt0.o

build/arc2/syscalls.o: arc2/syscalls.c
	$(CC) $(CFLAGS) -c arc2/syscalls.c -o build/arc2/syscalls.o

build/arc2/reent.o: arc2/reent.c
	$(CC) $(CFLAGS) -c arc2/reent.c -o build/arc2/reent.o

build/apps/user_shell.o: apps/user_shell.c
	$(CC) $(CFLAGS) -c apps/user_shell.c -o build/apps/user_shell.o

build/user_shell.elf: $(OBJS)
	$(LD) $(LDFLAGS) -o build/user_shell.elf build/arc2/crt0.o build/arc2/syscalls.o build/arc2/reent.o build/apps/user_shell.o -lc

# Strip debug metadata (.debug_*, .comment, etc.) before embedding.
# We strip from the link output into a temp, then copy over the
# original name so xxd -i keeps emitting `build_user_shell_elf[]`,
# which is what kmain.c expects.
build/user_shell.elf.stripped: build/user_shell.elf
	$(STRIP) --strip-debug build/user_shell.elf -o build/user_shell.elf.stripped
	@echo "user_shell.elf          : $$(stat -c %s build/user_shell.elf) bytes"
	@echo "user_shell.elf.stripped : $$(stat -c %s build/user_shell.elf.stripped) bytes"

../../user_shell_data.c: build/user_shell.elf.stripped
	cp build/user_shell.elf.stripped build/user_shell.elf
	xxd -i build/user_shell.elf > ../../user_shell_data.c

clean:
	rm -rf build ../../user_shell_data.c
