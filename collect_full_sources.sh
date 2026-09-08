#!/bin/bash
# Collect all source files from 04_kernel_64bit and 05_boot_kernel64,
# plus the capture.txt log from 04_kernel_64bit.
# Excludes: build/, newlib/, *.md, user_shell_data.c, and all *.txt except capture.txt.

OUTPUT="kernel_full_sources.txt"
> "$OUTPUT"

# Directories to scan
DIRS="04_kernel_64bit 05_boot_kernel64"

# Helper: append a file with its name
append_file() {
    local file="$1"
    echo "===== FILE: $file =====" >> "$OUTPUT"
    cat "$file" >> "$OUTPUT"
    echo -e "\n\n" >> "$OUTPUT"
}

# --- 1. Collect all code/source files from the two directories ---
for dir in $DIRS; do
    if [ ! -d "$dir" ]; then
        echo "Warning: directory '$dir' not found" >&2
        continue
    fi
    find "$dir" -type f \( \
        -name "*.c" -o \
        -name "*.h" -o \
        -name "*.S" -o \
        -name "*.asm" -o \
        -name "*.ld" -o \
        -name "Makefile" -o \
        -name "*.mk" \) \
        ! -path "*/build/*" \
        ! -path "*/newlib/*" \
        ! -name "user_shell_data.c" \
        | sort | while read -r file; do
            append_file "$file"
        done
done

# --- 2. Specifically add capture.txt from 04_kernel_64bit (if present) ---
CAPTURE="04_kernel_64bit/capture.txt"
if [ -f "$CAPTURE" ]; then
    echo "===== FILE: $CAPTURE (last run log) =====" >> "$OUTPUT"
    cat "$CAPTURE" >> "$OUTPUT"
    echo -e "\n\n" >> "$OUTPUT"
else
    echo "Warning: $CAPTURE not found" >&2
fi

echo "All sources and capture log collected in $OUTPUT"
