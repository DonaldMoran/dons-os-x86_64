CC := /opt/cross/bin/x86_64-elf-gcc

CFLAGS := -ffreestanding -Iapps/include -O2 -Wall -Wextra \
          -fno-pic -mno-red-zone -mno-sse -mno-sse2 -mno-avx -mno-mmx \
          -mcmodel=large -fno-builtin -fno-common -g \
          -fno-stack-protector -Wno-missing-field-initializers

LDFLAGS := -T ../../user_linker.ld -nostdlib -nostartfiles \
           -mcmodel=large -Wl,--no-warn-rwx-segments

BUILD_DIR := build
ARC2_DIR := build/arc2
APPS_DIR := build/apps

ALL_OBJS := $(ARC2_DIR)/crt0_c.o \
            $(APPS_DIR)/user_shell.o

TARGET := build/user_shell.elf

.PHONY: all clean

all: $(TARGET)
	xxd -i $(TARGET) > ../../user_shell_data.c

$(TARGET): $(ALL_OBJS)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $(ALL_OBJS)

$(ARC2_DIR)/crt0_c.o: arc2/crt0_c.c
	@mkdir -p $(ARC2_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(APPS_DIR)/user_shell.o: apps/user_shell.c
	@mkdir -p $(APPS_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -rf $(BUILD_DIR) ../../user_shell_data.c
