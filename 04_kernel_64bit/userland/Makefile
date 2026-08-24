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

# Picolibc stub files - including float_stubs.c
PICOLIBC_SRCS := $(PICOLIBC_DIR)/crt0.c \
                 $(PICOLIBC_DIR)/syscalls.c \
                 $(PICOLIBC_DIR)/stdio_init.c \
                 $(PICOLIBC_DIR)/float_stubs.c

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
	@echo "=== Userland build complete ==="

# Create build directory
$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

# Compile Picolibc stubs
$(BUILD_DIR)/%.o: $(PICOLIBC_DIR)/%.c | $(BUILD_DIR)
	@echo "Compiling $<"
	$(CC) $(CFLAGS) -c $< -o $@

# Compile applications
$(BUILD_DIR)/%.o: $(APPS_DIR)/%.c | $(BUILD_DIR)
	@echo "Compiling $<"
	$(CC) $(CFLAGS) -c $< -o $@

# Link user shell with -Wl,--allow-multiple-definition
$(TARGET): $(ALL_OBJS)
	@echo "Linking $@"
	$(CC) $(CFLAGS) $(LDFLAGS) -T ../user_linker.ld -o $@ $(ALL_OBJS) -lc -lgcc -Wl,--allow-multiple-definition
	@echo "=== User shell ELF info ==="
	@readelf -l $@ | head -20 || true
	@echo "Size: $$(stat -c%s $@ 2>/dev/null || stat -f%z $@) bytes"

# Generate C data file
$(DATA_OUT): $(TARGET)
	@echo "Generating $(DATA_OUT)"
	$(XXD) -i $< > $(DATA_OUT)
	@echo "Generated $(DATA_OUT)"
	@echo "=== Variable names in $(DATA_OUT) ==="
	@grep -E "^(unsigned char|unsigned int)" $(DATA_OUT) || true

# Clean
clean:
	rm -rf $(BUILD_DIR) $(DATA_OUT)
