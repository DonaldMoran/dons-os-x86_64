# =======================================================================
# DonsDOS Newlib User Shell Application Makefile
# Optimized to enforce free-standing loop primitives under -O2 bounds
# =======================================================================

CC = /opt/cross/bin/x86_64-elf-gcc
LD = /opt/cross/bin/x86_64-elf-gcc

# FIXED: Appended -fno-builtin-memset to prevent GCC from implicitly 
# vectorizing your custom loop into privileged SSE operations.
CFLAGS = -Iinclude -Iapps/include -O2 -Wall -Wextra -fno-pic -mno-red-zone \
         -mno-sse -mno-sse2 -mno-avx -mno-mmx -mcmodel=large -fno-common -g \
         -fno-stack-protector -Wno-missing-field-initializers -fno-builtin-memset

LDFLAGS = -T ../../user_newlib_linker.ld -nostdlib -mcmodel=large -Wl,--no-warn-rwx-segments -Llib

SRCS = arc2/crt0.S arc2/syscalls.c arc2/reent.c apps/user_shell.c
OBJS = build/arc2/crt0.o build/arc2/syscalls.o build/arc2/reent.o build/apps/user_shell.o

all: prep build/user_shell.elf

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
	xxd -i build/user_shell.elf > ../../user_shell_data.c

clean:
	rm -rf build ../../user_shell_data.c
