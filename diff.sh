# Files that are different:

A=/home/noneya/code/archive/dons-os-x86_64/04_kernel_64bit
B=/home/noneya/code/archive/no_terminal/04_kernel_64bit

cd "$A"
find . -type f \( -name '*.c' -o -name '*.S' -o -name '*.asm' \) -print0 |
while IFS= read -r -d '' f; do
  if [ ! -e "$B/$f" ]; then
    echo "ONLY IN NEW: ${f#./}"
  elif ! cmp -s "$A/$f" "$B/$f"; then
    echo "DIFFERS: ${f#./}"
  fi
done


# Show the differences

A=/home/noneya/code/dons-os-x86_64/04_kernel_64bit
B=/home/noneya/code/archive/dons-os-x86_64/04_kernel_64bit

cd "$A"
find . -type f \( -name '*.c' -o -name '*.S' -o -name '*.asm' \) -print0 |
while IFS= read -r -d '' f; do
  if [ ! -e "$B/$f" ]; then
    echo "=== ONLY IN NEW: ${f#./} ==="
  elif ! cmp -s "$A/$f" "$B/$f"; then
    echo "=== ${f#./} ==="
    diff -u "$A/$f" "$B/$f"
    echo
  fi
done
