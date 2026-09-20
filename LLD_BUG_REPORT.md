# LLD 22.1.8 mis-link bug — tracking note

## What happened

Building the kernel with `-O2` for `fatfs/ff.c` produced a `kernel.elf`
in which two functions contained **truncated instructions**. The bytes
present in the standalone `.o` file were missing from the linked binary.
The CPU raised `#UD` at runtime when it reached one of them.

## Bug report

Filed against LLVM's `llvm-project` repository:

    https://github.com/llvm/llvm-project/issues/224938

Title: "LLD 22.1.8 produces truncated instructions in linked kernel
binary while standalone object is correct"

Date filed: <PASTE THE DATE HERE>

## Evidence

### Faulting address (from QEMU serial output)

    *** UNHANDLED INTERRUPT: vector 6 ***
      error_code : 0x0000000000000000
      rip        : 0xFFFFFFFF80110002
      cs         : 0x0000000000000018
      rflags     : 0x0000000000010213
      rsp        : 0xFFFFFFFF8008FF50
      ss         : 0x0000000000000020
      rax        : 0x0000000000000004
      rbx        : 0xFFFFFFFF801269C8
      rcx        : 0x0000000000000000
      rdx        : 0x0000000000000000
      rsi        : 0x0000000000000000
      rdi        : 0xFFFFFFFF801269C8

Vector 6 is `#UD` (invalid opcode). rbx and rdi both hold `&boot_fs`,
so the fault is in `check_fs`, just after `move_window` returns.

### Disassembly of the faulting kernel.elf

    ffffffff80110019:	0f b6 43 30          	movzbl 0x30(%rbx),%eax
    ffffffff8011001d:	05 18 ff ff ff       	add    $0xffffff18,%eax
    ffffffff80110022:	44 0f b7 b3 2e 02 00 	movzwl 0x22e(%rbx),%r14d
    ffffffff80110029:	00
    ffffffff8011002a:	83 f8 03             	cmp    $0x3,%eax
    ffffffff8011002d:	0f                   	.byte 0xf
    ffffffff8011002e:	87                   	.byte 0x87
    ffffffff8011002f:	80                   	.byte 0x80

The instruction at `0xFFFFFFFF8011002D` should be a 6-byte `ja rel32`.
Only the first 3 bytes are present.

A second truncated instruction appears in `move_window`:

    ffffffff8010e0f8:	89 c1                	mov    %eax,%ecx
    ffffffff8010e0fa:	b8 01 00 00 00       	mov    $0x1,%eax
    ffffffff8010e0ff:	85                   	.byte 0x85

`0x85` is the opcode for `test r/m32, r32`, truncated to a single byte.

### Disassembly of the standalone object (correct)

Compiled with the same flags into `/tmp/ff_from_source.o`:

        2d29:	0f b6 43 30          	movzbl 0x30(%rbx),%eax
        2d2d:	05 18 ff ff ff       	add    $0xffffff18,%eax
        2d32:	44 0f b7 b3 2e 02 00 	movzwl 0x22e(%rbx),%r14d
        2d39:	00
        2d3a:	83 f8 03             	cmp    $0x3,%eax
        2d3d:	0f 87 80 00 00 00    	ja     2dc3 <check_fs+0xc3>

All instructions complete and correctly encoded.

## Environment

- Clang:  22.1.8 (Fedora 22.1.8-4.fc44)
- LLD:    22.1.8
- Target: x86_64-unknown-elf
- Host:   Fedora 44, x86_64

## The exact commands

Compile (single object):

    clang -target x86_64-unknown-elf -ffreestanding -O2 -Wall -Wextra \
          -mcmodel=kernel -fno-pic -mno-red-zone -mno-sse -mno-sse2 \
          -mno-avx -mno-mmx -Iinclude -Ifatfs -DFAT_CONFIG_SINGLE_DRIVE=1 \
          -c fatfs/ff.c -o fatfs/ff.o

Link (full kernel):

    ld.lld -nostdlib -T linker.ld -o kernel.elf <all objects>

Preprocess (for the report's attachment):

    clang -target x86_64-unknown-elf -ffreestanding -O2 -mcmodel=kernel \
          -fno-pic -mno-red-zone -mno-sse -mno-sse2 -mno-avx -mno-mmx \
          -Iinclude -Ifatfs -DFAT_CONFIG_SINGLE_DRIVE=1 \
          -E fatfs/ff.c -o ~/ff.i

The preprocessed output (~56 KB) was attached to the bug report.

## Workaround currently in place

In `04_kernel_64bit/Makefile`:

    fatfs/ff.o: fatfs/ff.c
    	$(CC) $(CFLAGS) -O1 -c fatfs/ff.c -o fatfs/ff.o

This overrides the default `-O2` for just that one file. The kernel
builds, boots, mounts the FAT volume, and launches the user shell.

## If LLVM responds

They will probably ask for one or more of the following. The answers
are all recorded above or are easy to regenerate:

1. **A minimal reproducer.** We do not have one — the bug does not
   reproduce when `ff.c` is compiled standalone to an object file. It
   only appears in the linked binary. If asked, say so, and offer the
   full kernel source tree (which has been copied aside for exactly
   this purpose).

2. **The preprocessed source.** `~/ff.i` (or regenerate with the
   command above).

3. **The linked `kernel.elf`.** The one that reproduced the fault is
   the build *without* the `-O1` override for `fatfs/ff.o`.

4. **The full build log.** Show them the `ld.lld` invocation and the
   flags for each object.

5. **The linker script.** `04_kernel_64bit/linker.ld`.

6. **A `--reproduce` tarball.** LLD supports
   `ld.lld --reproduce=repro.tar` which captures every input for a
   self-contained reproduction. If they ask, re-link the faulting
   `kernel.elf` with that flag and upload the tarball.

## The one outstanding question

We never ran the diagnostic that would have distinguished
"linker bug" from "compiler bug" with certainty: relinking the same
objects with `ld.bfd` instead of `ld.lld`. If LLVM asks us to narrow
the fault further, that test is the next thing to do:

    # in 04_kernel_64bit/Makefile
    LD := ld.bfd

then `make clean && make FAT_CONFIG=single` and check
`/opt/cross/bin/x86_64-elf-objdump -d kernel.elf` around
`0xFFFFFFFF8011002D`.

If `ld.bfd` produces correct output and `ld.lld` does not, the bug is
in LLD and the report is correct as filed. If both produce the same
broken output, the bug is elsewhere (compiler, assembler, or the way
objects are being produced).

## Where the faulting build's source tree is

A copy of the project was made at the time of filing:

    <PASTE THE PATH TO THE COPY HERE>

That copy is in the faulting state (no `-O1` override in
`04_kernel_64bit/Makefile`), so re-running `make clean && make
FAT_CONFIG=single` there should reproduce the bug.

## Notes for future me

- The bug is not in our source. The same source compiles to correct
  object code. It only breaks when linked.
- The `-O1` override is a workaround, not a fix. The bug is still
  there; it just doesn't trigger for this file at `-O1`.
- If the workaround ever stops working (e.g., after a toolchain
  update or a source change to `ff.c`), the disassembly check is:
  `/opt/cross/bin/x86_64-elf-objdump -d kernel.elf | grep -B2 -A2 "\.byte"`
  If any `.byte` shows up in `.text`, the truncation is back.
- The Makefile does not track header dependencies
  (`%.o: %.c` with no `.h`). Every header change requires
  `make clean`. This is a separate issue and worth fixing when
  we have a moment.
