#!/bin/bash
# Migration script for Picolibc integration
# Run from: /path/to/dons-os-x86_64/
# Usage: ./migrate-to-picolibc.sh

set -e  # Exit on error

echo "=== Dons OS - Picolibc Migration ==="
echo "Starting migration..."
echo "Current directory: $(pwd)"

# ----------------------------------------------------------------------
# 1. BACKUP CURRENT USER SHELL
# ----------------------------------------------------------------------
echo ""
echo "[1/7] Backing up current user shell..."

if [ -f "04_kernel_64bit/user_shell.c" ]; then
    cp 04_kernel_64bit/user_shell.c 04_kernel_64bit/user_shell.c.old
    echo "  ✓ Created backup: 04_kernel_64bit/user_shell.c.old"
else
    echo "  ⚠ Warning: 04_kernel_64bit/user_shell.c not found"
fi

if [ -f "04_kernel_64bit/user_shell_data.c" ]; then
    cp 04_kernel_64bit/user_shell_data.c 04_kernel_64bit/user_shell_data.c.old
    echo "  ✓ Created backup: 04_kernel_64bit/user_shell_data.c.old"
fi

# ----------------------------------------------------------------------
# 2. CREATE USERLAND DIRECTORY STRUCTURE
# ----------------------------------------------------------------------
echo ""
echo "[2/7] Creating userland directory structure..."

mkdir -p 04_kernel_64bit/userland/picolibc
mkdir -p 04_kernel_64bit/userland/apps
mkdir -p 04_kernel_64bit/userland/build

echo "  ✓ Created userland directories"

# ----------------------------------------------------------------------
# 3. CREATE PICOBIBC STUB FILES
# ----------------------------------------------------------------------
echo ""
echo "[3/7] Creating Picolibc stub files..."

# crt0.c
cat > 04_kernel_64bit/userland/picolibc/crt0.c << 'EOF'
// crt0.c - Startup code for Picolibc
extern int main(void);
extern void _exit(int status);

void _start(void) {
    int status = main();
    _exit(status);
}
EOF
echo "  ✓ Created crt0.c"

# syscalls.c - Using YOUR syscall numbers (SYS_WRITE=1, SYS_EXIT=2, SYS_READ=3)
cat > 04_kernel_64bit/userland/picolibc/syscalls.c << 'EOF'
// syscalls.c - Picolibc syscall stubs using YOUR kernel's syscall numbers
#include <stdint.h>
#include <stddef.h>
#include <sys/stat.h>
#include <errno.h>

// Your syscall numbers (from syscall.h)
#define SYS_WRITE 1
#define SYS_EXIT  2
#define SYS_READ  3

// Syscall wrapper (matches your kernel's calling convention)
static inline long syscall(long num, long arg1, long arg2, long arg3) {
    long ret;
    __asm__ volatile (
        "syscall"
        : "=a"(ret)
        : "a"(num), "D"(arg1), "S"(arg2), "d"(arg3)
        : "rcx", "r11", "memory"
    );
    return ret;
}

// Picolibc required stubs
int _write(int file, char *ptr, int len) {
    return (int)syscall(SYS_WRITE, file, (long)ptr, len);
}

void _exit(int status) {
    syscall(SYS_EXIT, status, 0, 0);
    while(1) __asm__("hlt");
}

// Heap management - uses your user memory layout (starts at 0x8000000000)
void *_sbrk(ptrdiff_t incr) {
    extern char _end;  // Defined in user_linker.ld
    static char *heap_end = &_end;
    char *prev_heap_end = heap_end;
    
    // Your user space starts at 0x8000000000, heap limit at 0x8001000000 (16MB)
    // This matches your user_linker.ld base address
    char *heap_limit = (char*)0x8001000000ULL;
    
    if (heap_end + incr > heap_limit) {
        errno = ENOMEM;
        return (void*)-1;
    }
    
    heap_end += incr;
    return (void*)prev_heap_end;
}

// Other required stubs
int _close(int file) { return -1; }
int _fstat(int file, struct stat *st) { 
    st->st_mode = S_IFCHR; 
    return 0; 
}
int _isatty(int file) { return 1; }
int _lseek(int file, int ptr, int dir) { return 0; }
int _read(int file, char *ptr, int len) { 
    return (int)syscall(SYS_READ, file, (long)ptr, len);
}
int _kill(int pid, int sig) { errno = EINVAL; return -1; }
int _getpid(void) { return 1; }
int _times(void *buf) { return -1; }
EOF
echo "  ✓ Created syscalls.c"

# stdio_init.c
cat > 04_kernel_64bit/userland/picolibc/stdio_init.c << 'EOF'
// stdio_init.c - Define stdout/stderr for Picolibc
#include <stdio.h>

extern int _write(int file, char *ptr, int len);

// Output for stdout
static int stdout_putc(char c, FILE *file) {
    (void) file;
    _write(1, &c, 1);
    return c;
}

// Output for stderr (same as stdout for now)
static int stderr_putc(char c, FILE *file) {
    (void) file;
    _write(2, &c, 1);
    return c;
}

// Input (will use your keyboard input via SYS_READ)
static int stdin_getc(FILE *file) {
    (void) file;
    char c;
    if (_read(0, &c, 1) > 0) {
        return (unsigned char)c;
    }
    return EOF;
}

// Define stdout, stdin, stderr
static FILE __stdout = FDEV_SETUP_STREAM(stdout_putc, NULL, NULL, _FDEV_SETUP_WRITE);
static FILE __stdin  = FDEV_SETUP_STREAM(NULL, stdin_getc, NULL, _FDEV_SETUP_READ);
static FILE __stderr = FDEV_SETUP_STREAM(stderr_putc, NULL, NULL, _FDEV_SETUP_WRITE);

FILE *const stdout = &__stdout;
FILE *const stdin = &__stdin;
FILE *const stderr = &__stderr;
EOF
echo "  ✓ Created stdio_init.c"

# ----------------------------------------------------------------------
# 4. CREATE USER SHELL WITH PICOLIBC
# ----------------------------------------------------------------------
echo ""
echo "[4/7] Creating new user shell (Picolibc version)..."

cat > 04_kernel_64bit/userland/apps/user_shell.c << 'EOF'
// user_shell.c - Ported to use Picolibc stdio
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static const char *help_text = 
    "User Shell v0.2 (Picolibc)\n"
    "Commands:\n"
    "  help    - Show this help\n"
    "  exit    - Exit shell\n"
    "  clear   - Clear screen\n"
    "  whoami  - Show current user\n"
    "  version - Show version\n"
    "  echo    - Echo text back\n"
    "  test    - Test stdio functions\n";

int main(void) {
    char cmd_buffer[128];
    
    // Print banner
    printf("%s", help_text);
    
    while (1) {
        printf("] ");
        fflush(stdout);
        
        // Read command
        if (fgets(cmd_buffer, sizeof(cmd_buffer), stdin) == NULL) {
            continue;
        }
        
        // Remove newline
        size_t len = strlen(cmd_buffer);
        if (len > 0 && cmd_buffer[len-1] == '\n') {
            cmd_buffer[len-1] = '\0';
        }
        
        // Parse command
        if (strcmp(cmd_buffer, "exit") == 0) {
            printf("Goodbye!\n");
            break;
        } else if (strcmp(cmd_buffer, "help") == 0) {
            printf("%s", help_text);
        } else if (strcmp(cmd_buffer, "clear") == 0) {
            printf("\n");
        } else if (strcmp(cmd_buffer, "whoami") == 0) {
            printf("user\n");
        } else if (strcmp(cmd_buffer, "version") == 0) {
            printf("v0.2 (Picolibc)\n");
        } else if (strncmp(cmd_buffer, "echo ", 5) == 0) {
            printf("%s\n", cmd_buffer + 5);
        } else if (strcmp(cmd_buffer, "test") == 0) {
            printf("Testing stdio...\n");
            printf("  sprintf test: %s\n", "works");
            printf("  Number: %d, Hex: %x, Octal: %o\n", 42, 42, 42);
            printf("  String length: %zu\n", strlen("hello"));
        } else if (strlen(cmd_buffer) > 0) {
            printf("Unknown command: '%s'\nType 'help' for available commands\n", cmd_buffer);
        }
    }
    
    return 0;
}
EOF
echo "  ✓ Created userland/apps/user_shell.c"

# ----------------------------------------------------------------------
# 5. CREATE USERLAND MAKEFILE
# ----------------------------------------------------------------------
echo ""
echo "[5/7] Creating userland Makefile..."

cat > 04_kernel_64bit/userland/Makefile << 'EOF'
# Userland Makefile - Uses cross-compiler with Picolibc
CC      := /opt/cross/bin/x86_64-elf-gcc
LD      := /opt/cross/bin/x86_64-elf-ld
OBJCOPY := /opt/cross/bin/x86_64-elf-objcopy
XXD     := xxd

# Picolibc paths
PICOLIBC_INC := /opt/cross/include
PICOLIBC_LIB := /opt/cross/lib

# Compiler flags for userland
CFLAGS := -ffreestanding -nostdlib -static \
          -I$(PICOLIBC_INC) \
          -O2 -Wall -Wextra \
          -fno-pic -mno-red-zone -mno-sse -mno-sse2 \
          -mno-avx -mno-mmx -mcmodel=large \
          -fno-builtin -fno-common

LDFLAGS := -nostdlib -static \
           -L$(PICOLIBC_LIB)

# Source directories
PICOLIBC_DIR := picolibc
APPS_DIR     := apps
BUILD_DIR    := build

# Picolibc stub files
PICOLIBC_SRCS := $(PICOLIBC_DIR)/crt0.c \
                 $(PICOLIBC_DIR)/syscalls.c \
                 $(PICOLIBC_DIR)/stdio_init.c

# Application source files
APP_SRCS := $(APPS_DIR)/user_shell.c

# Object files (in build directory)
PICOLIBC_OBJS := $(patsubst $(PICOLIBC_DIR)/%.c,$(BUILD_DIR)/%.o,$(PICOLIBC_SRCS))
APP_OBJS     := $(patsubst $(APPS_DIR)/%.c,$(BUILD_DIR)/%.o,$(APP_SRCS))
ALL_OBJS     := $(PICOLIBC_OBJS) $(APP_OBJS)

# Targets
TARGET   := $(BUILD_DIR)/user_shell.elf
DATA_OUT := ../user_shell_data.c

.PHONY: all clean

all: $(TARGET) $(DATA_OUT)

# Create build directory
$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

# Compile Picolibc stubs
$(BUILD_DIR)/%.o: $(PICOLIBC_DIR)/%.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

# Compile applications
$(BUILD_DIR)/%.o: $(APPS_DIR)/%.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

# Link user shell
$(TARGET): $(ALL_OBJS)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $(ALL_OBJS) -lc -lgcc
	@echo "=== User shell ELF info ==="
	@file $(TARGET)
	@echo "Size: $$(stat -c%s $(TARGET) 2>/dev/null || stat -f%z $(TARGET)) bytes"

# Generate C data file for embedding
$(DATA_OUT): $(TARGET)
	$(XXD) -i $(TARGET) > $(DATA_OUT)
	@echo "Generated $(DATA_OUT)"

# Clean
clean:
	rm -rf $(BUILD_DIR) $(DATA_OUT)
EOF
echo "  ✓ Created userland/Makefile"

# ----------------------------------------------------------------------
# 6. UPDATE USER LINKER SCRIPT
# ----------------------------------------------------------------------
echo ""
echo "[6/7] Updating user_linker.ld..."

# Check if user_linker.ld already has _end
if grep -q "_end = .;" 04_kernel_64bit/user_linker.ld; then
    echo "  ✓ user_linker.ld already has _end symbol"
else
    # Backup the original
    cp 04_kernel_64bit/user_linker.ld 04_kernel_64bit/user_linker.ld.old
    echo "  ✓ Backed up user_linker.ld to user_linker.ld.old"
    
    # Add _end to the BSS section
    cat > 04_kernel_64bit/user_linker.ld << 'EOF'
OUTPUT_FORMAT(elf64-x86-64)
ENTRY(_start)

SECTIONS
{
    /* User virtual base address */
    . = 0x8000000000;

    /* Code */
    .text : ALIGN(16)
    {
        *(.text*)
    }

    /* Read-only data (strings, consts, etc.) */
    .rodata : ALIGN(16)
    {
        *(.rodata*)
        *(.rodata.str1.1*)
        *(.rodata.str1.8*)
    }

    /* Exception frames (needed for clang) */
    .eh_frame : ALIGN(16)
    {
        *(.eh_frame*)
    }

    /* Writable data */
    .data : ALIGN(16)
    {
        *(.data*)
    }

    /* Zero‑fill section */
    .bss : ALIGN(16)
    {
        *(.bss*)
        *(COMMON)
        _end = .;  /* IMPORTANT: For sbrk to know where heap starts */
    }
}
EOF
    echo "  ✓ Added _end symbol to user_linker.ld"
fi

# ----------------------------------------------------------------------
# 7. UPDATE MAIN KERNEL MAKEFILE
# ----------------------------------------------------------------------
echo ""
echo "[7/7] Updating kernel Makefile..."

# Check if userland targets already exist
if grep -q "USERLAND_DIR" 04_kernel_64bit/Makefile; then
    echo "  ✓ Makefile already has userland targets"
else
    # Backup the original
    cp 04_kernel_64bit/Makefile 04_kernel_64bit/Makefile.old
    echo "  ✓ Backed up Makefile to Makefile.old"
    
    # Create new Makefile with userland integration
    cat > 04_kernel_64bit/Makefile << 'EOF'
CC      := clang
LD      := ld.lld
AS      := nasm
OBJCOPY := objcopy
XXD     := xxd

# Userland directory
USERLAND_DIR := userland

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

# Build userland
userland:
	$(MAKE) -C $(USERLAND_DIR)

# ============================================================
# KERNEL OBJECTS
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
# USER SHELL (NEW - generated by userland Makefile)
# ============================================================
# user_shell_data.c is now generated by userland/Makefile
# We just need to compile it
user_shell_data.o: user_shell_data.c
	$(CC) $(CFLAGS) -Iinclude -c user_shell_data.c -o user_shell_data.o

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
# CLEAN
# ============================================================
clean:
	rm -f *.o *.bin *.elf test_program test_program.bin test_program_data.c
	rm -f user_shell.elf user_shell_data.c
	rm -f user_shell.bin
	$(MAKE) -C $(USERLAND_DIR) clean
EOF
    echo "  ✓ Updated Makefile with userland integration"
fi

# ----------------------------------------------------------------------
# COMPLETE
# ----------------------------------------------------------------------
echo ""
echo "========================================"
echo "Migration Complete!"
echo "========================================"
echo ""
echo "Summary:"
echo "  ✓ Backed up old shell to 04_kernel_64bit/user_shell.c.old"
echo "  ✓ Created userland directory with Picolibc stubs"
echo "  ✓ Created new user shell using Picolibc"
echo "  ✓ Updated user_linker.ld with _end symbol"
echo "  ✓ Updated kernel Makefile"
echo ""
echo "Next Steps:"
echo "  1. Build the new system:"
echo "     cd 04_kernel_64bit && make clean && make all"
echo ""
echo "  2. Test in QEMU:"
echo "     cd ../05_boot_kernel64 && make all && make run"
echo ""
echo "  3. If something fails, you can revert:"
echo "     cd 04_kernel_64bit"
echo "     cp user_shell.c.old user_shell.c"
echo "     cp user_shell_data.c.old user_shell_data.c"
echo "     cp Makefile.old Makefile"
echo "     cp user_linker.ld.old user_linker.ld"
echo ""
echo "  4. Check the new shell output for 'User Shell v0.2 (Picolibc)'"
echo ""
