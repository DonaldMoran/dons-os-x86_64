SESSION HANDOFF — DonsDOS After Debug Cleanup
Context for the new session

You are picking up a 64-bit x86_64 hobby OS called DonsDOS immediately after a major debugging + cleanup session. The system is stable, working, and tagged. This handoff documents everything needed to resume productively without re-deriving the state.
Project layout

    Kernel: 04_kernel_64bit/

    Userland (newlib): 04_kernel_64bit/userland/newlib/

    Boot chain: 01_boot_16bit/, 02_boot_32bit/, 03_boot_64bit/, 05_boot_kernel64/

    Kernel builds with clang (-mcmodel=kernel -fno-pic -mno-red-zone -mno-sse -mno-sse2 -mno-avx -mno-mmx)

    Userland builds with gcc (-mcmodel=large -mno-red-zone -mno-sse ...), newlib built --disable-tls --disable-newlib-tls --enable-newlib-reent-small

Git state at handoff
text

20260912H  Debug cleanup complete; stable interactive shell       <- HEAD
20260912F  Priority-1 debug cleanup complete: interrupts, user_syscall, tss, gdt
20260912E  interrupts: timer_preempt_handler noinline fix; 25/25 stable
20260912D  Scheduler idle-requeue fix; interactive shell stable
20260912C  Newlib reentrancy fully working in ring 3
20260912B  restorepoint: 20260912B
20260912A  restorepoint: 20260912A
20260911A  Milestone: user shell runs continuously
20260910A  Milestone: printf works in userland

Branch dev is 9 commits ahead of origin/dev and has not been pushed.

Note: there is no 20260912G tag. The priority-2 cleanup was committed under 20260912H. This is intentional, not a mistake.
What is working (verified)

    Boot to interactive kernel shell — clean, minimal serial log (~15 lines)

    Preemptive multitasking — timer-driven, round-robin, quantum = 2 ticks (20ms)

    Ring 3 userland — usershell command loads an embedded ELF, runs it in ring 3

    Newlib reentrancy — _impure_ptr correct, __sinit runs, printf/setvbuf/stdio all work

    Syscalls — sys_write (real: safe_copy_from_user + serial + vga), sys_read (sti;hlt), sys_brk, sys_exit, sys_arch_set_fs

    Interactive user shell — banner, menu, read(0,...), responds to 1 and 2, clean exit

    All kernel shell commands work: help, clear, version, info, mem, reboot, pmmtest, test, vmmtest, serialtest, heapstat, maptest, testrec, heaptest, nxtest, syscall, elfload, proclist, vmmclone, proccreate, runproc, schstat, testyield, usershell

What was fixed this session (do not re-fix)
Bug 1 — user_syscall_entry.asm clobbering callee-saved registers

The original prologue did mov r12, rcx and mov r13, r11 before saving the user's r12/r13. Every syscall silently corrupted those two registers. Downstream effect: main's cached function pointers (%r12 = print_hex_label) became stale after each syscall, causing wild jumps to RIP=0x2, RIP=0xA, and CR2=-32 faults inside newlib.

Fix: push the original rbx, rbp, r12–r15 FIRST, before anything is clobbered. Then use rbx as the syscall-number scratch (which is legal because rbx was already saved). Also save user RIP/RFLAGS explicitly in dedicated stack slots so sysret can restore them.
Bug 2 — context_switch.asm saving a non-timer-compatible frame

When saving prev, the original code did mov [rdi+0x108], rsp (raw RSP) and treated [rsp] as RIP. That frame is not what irq0_stub expects after a timer-driven switch. irq0_stub pops 15 GPRs and does iretq from next->rsp, so next->rsp must point at a full iretq frame.

Fix: context_switch now fabricates a proper iretq frame (SS, RSP, RFLAGS, CS, RIP) followed by the 15 GPR slots, so every save produces a layout irq0_stub can resume.
Bug 3 — scheduler_switch_to re-adding idle to the ready queue

scheduler_switch_to unconditionally re-added prev (the outgoing process) to the ready queue. When prev was idle, which was already on the queue from process_init, the second add created a self-cycle (idle->next = idle), orphaning every other task from the queue. Symptom: the shell ran once, then was never rescheduled — timer kept firing, dots kept appearing, but nothing ever came back.

Fix: skip the re-add when prev->pid == 1. This matches the convention the timer path already uses (if (current->pid != 1) scheduler_ready_queue_add(current)).
Architectural fragility — the noinline fix

timer_preempt_handler straddles an implicit ABI boundary with irq0_stub: irq0_stub pushes a 15-GPR frame, calls the handler with rdi = rsp, and uses the return value in rax as the new rsp. For this to work, the function must be a real function call. When diagnostics were removed and the function became small enough, clang inlined it at -O2 and the frame layout between irq0_stub's pushes and the returned rsp no longer matched. Symptom: intermittent user-mode #PF at CR2 0x7FF3xxxxxx inside newlib.

Fix: __attribute__((noinline)) on timer_preempt_handler (and, defensively, on tss_set_kernel_stack). Both have explanatory comments in the source.

Long-term fix (not done): switch to a per-process kernel stack on syscall entry. See "Next major work" below.
Debug cleanup

Eleven files had per-operation, per-page, per-allocation, or per-switch logging removed. Boot log went from ~150 lines to ~15. Net diff: −706 / +144 lines. All error paths, on-demand diagnostics (shell commands), and one-line subsystem confirmations were kept. Full details in the 20260912H commit message.
Things to be aware of

    noinline on timer_preempt_handler is load-bearing. If it's ever removed and the function shrinks below clang's inlining threshold, the intermittent user-mode fault will return. The symptom is a #PF with CR2=0x7FF3xxxxxx and RIP inside newlib's _vfprintf_r. First thing to check if that resurfaces.

    The syscall path runs on the user stack. Not ideal but works today. Every kernel function called from syscall_dispatch runs on user memory. This is why we needed the noinline workaround for the timer boundary — if the timer fires during a syscall, it lands on the user stack and the frame layout gets complicated.

    sys_read uses sti; hlt. Blocks the CPU in the kernel until a key arrives. Not the "correct" design (a proper version marks the process BLOCKED and yields), but functional.

    kmain.c currently has (or should have, after the last commit): serial_print("Kernel: entering shell loop.\n"); — if it still prints loop\n.\n with a stray . on the next line, that's a one-line cosmetic fix.

Files that matter most for the next piece of work

If the next session's goal involves the syscall path or the timer path (see "Next major work"), have these ready:

    04_kernel_64bit/user_syscall_entry.asm — the syscall entry stub

    04_kernel_64bit/interrupts.c — timer_preempt_handler

    04_kernel_64bit/isr.asm — irq0_stub

    04_kernel_64bit/context_switch.asm — context switch on cooperative paths

    04_kernel_64bit/process.c — process_create, kernel_idle_loop, PCB layout

    04_kernel_64bit/include/process.h — pcb_t definition

    04_kernel_64bit/tss.c, tss.h — TSS/RSP0

    04_kernel_64bit/user_syscall.c — syscall handlers, safe_copy_to_user / safe_copy_from_user

Next major work — options, in priority order
A. Kernel-stack-on-syscall-entry refactor (highest value)

Goal: all kernel code runs on a kernel stack, not the user stack.

Approach:

    Give each process a dedicated syscall stack (or reuse kernel_stack_top).

    In user_syscall_entry, immediately save user RSP and set RSP = current process's kernel stack top.

    Do all pushes/syscall_dispatch/pops on the kernel stack.

    On return, restore user RSP and sysret.

Requires: a global like g_syscall_stack_top updated by scheduler_switch_to and timer_preempt_handler whenever current changes. Careful thought about nesting — a timer interrupt during a syscall must not overwrite the syscall's stack frame. Standard solution: separate stack for interrupts (TSS.RSP0) vs syscalls (dedicated per-process or per-CPU stack). Or use TSS.IST1 for the timer.

Benefit: removes the noinline dependency, eliminates the class of frame-corruption bug we spent this session on, makes the syscall path look like Linux's.

Risk: moderate. Breakage would show up as syscall entry/exit faults, which are easy to spot.
B. malloc from userland

Wire up a proper user-facing heap that exercises sys_brk. The kernel side (sys_brk in user_syscall.c) is already implemented. The userland side (sbrk in arc2/syscalls.c) exists. What's needed:

    A malloc/free in userland that uses sbrk.

    A shell command that allocates, writes, reads back, frees.

    Confirmation that user pages get mapped/unmapped correctly.

Benefit: exercises the syscall path in a new way (write to newly-mapped user memory), a prerequisite for almost everything else.
C. sys_read blocking refactor

Change sys_read from sti; hlt (blocks the CPU in the kernel) to a proper blocking call:

    Mark current process BLOCKED.

    Yield to another task via scheduler_switch_to.

    Be woken by kbd_buffer_put (which would need to know which process is waiting for keyboard input).

Benefit: correct design, and pairs naturally with A. If you do A first, C is straightforward.
D. On-demand GDT/TSS dumps

If useful, add gdtdump and tssdump shell commands. The old dump functions are in git show 20260912C:04_kernel_64bit/gdt.c. Small.
E. More userland

More shell commands in userland/newlib/apps/user_shell.c; extend arc2/syscalls.c with open/close/stat and wire them to a simple in-kernel "device" surface.
F. Longer-term

Filesystem, networking, SMP, mmap, signals, wait queues, priorities. None urgent.
How to work with me effectively (based on this session)

Things that worked well:

    Paste the actual current source before asking for a patch. Stale source caused us to chase phantom bugs twice.

    Prefer real evidence over reasoning. The dots-heartbeat diagnostic is what finally located the queue corruption. When a fault appears, get the exact fault signature (CR2, RIP, CS, error code) and an nm/disassembly window around the RIP.

    Isolate one change at a time. Apply one file's cleanup, rebuild, run, verify, then move on. When we batched changes, we couldn't attribute faults.

    Roll back immediately when a change breaks something, and confirm the rollback is stable. Don't try to fix forward on a broken tree.

    Tag before experimenting. Restore points are cheap; undoing an ambiguous regression is expensive.

    The fault signature CR2=0x7FF3xxxxxx in user mode inside newlib is diagnostic — it means the timer/syscall frame boundary is broken again.

Boot log at handoff (for reference)
text

BootInfo validated successfully
TIMER[1] frame=... cur=NULL
PMM: init OK, free=32448 pages (126 MB)
VMM: init OK, 127 MB usable, max phys 0x7FE0000
HEAP: init OK, 1024 KB
PROCESS: init OK, 1 process(es)
GDT: User segments fixed for Ring 3
TSS: TR loaded correctly (0x38)
CPU: x86_64 FSGSBASE instructions successfully enabled for Ring 3.
SYSCALL init done
**RING** 3 syscalls Initialized
CPU: Native hardware SSE vector extensions safely enabled.
Kernel: entering shell loop
TIMER[2] ... cur=1
TIMER[3] ... cur=1

Then usershell produces:
text

========================================
   DonsDOS Native Newlib 4.x User Shell
========================================

Select option node:
  1. Print a message via printf
  2. Exit Runtime Environment

> 
[SUCCESS] printf works in ring 3!
> 
Exiting user shell environment...

First thing to do in the new session

Tell me (the new session's assistant):

    That you're continuing from 20260912H.

    Which of the next-major-work options (A–F) you want to tackle.

    Paste the relevant source files for that work.

If you want to start with A (kernel-stack-on-syscall-entry), paste: user_syscall_entry.asm, interrupts.c, isr.asm, process.c, include/process.h, tss.c, tss.h, scheduler.c, user_syscall.c.

If you want to start with B (malloc from userland), paste: userland/newlib/arc2/syscalls.c, userland/newlib/apps/user_shell.c, user_syscall.c (kernel side), and any userland/newlib/user_newlib_linker.ld if allocation addresses matter.











** **************************************************************************

I think i did A and now working on C not sure???

Handoff: DonsDOS — blocking reads and multi-shell support

Resume from tag 20260912I. The working tree is clean and checked out
at that tag. Do not assume anything from any prior session; this
document is the only context you need.

Repository layout

    Kernel: 04_kernel_64bit/
    Userland (newlib): 04_kernel_64bit/userland/newlib/
    Boot chain: 01_boot_16bit/, 02_boot_32bit/, 03_boot_64bit/,
                05_boot_kernel64/
    Kernel builds with clang (-mcmodel=kernel -fno-pic -mno-red-zone
    -mno-sse -mno-sse2 -mno-avx -mno-mmx)
    Userland builds with gcc (-mcmodel=large -mno-red-zone ...)

What the tag 20260912I contains

Boot to interactive kernel shell, preemptive multitasking, ring 3
userland via usershell, newlib reentrancy, all kernel shell commands,
clean usershell operation. sys_read blocks the CPU via sti; hlt when
the kbd buffer is empty. That is the limitation this session is here
to fix.

The requirement

DonsDOS must support multiple concurrent user shells, each running
as an ordinary user process. A shell blocked in read() waiting for
keyboard input must not prevent any other process from running, and
must not prevent idle from halting the CPU. When a key arrives, the
blocked shell wakes and consumes the byte. This must work for N
shells, not just one.

Falling out of that:
  - A shell can exit. When it does, it is terminated and its
    resources reclaimed. The machine keeps running because idle is
    always runnable.
  - Any user process, not just a shell, can block in read() and be
    resumed when input arrives.
  - The scheduler's choice of what to run next is unaffected by
    whether some process is blocked.

Recommended design

Do not build a separate cooperative context-switch primitive for
the block/wake path. That approach was attempted in a previous
session and failed in ways that were never fully explained (frame
layout mismatches, a same-privilege iretq behaving like a
cross-privilege one, and a timer path that resumed a cooperative
frame). Each fix revealed a new bug. The design has too many moving
parts.

Instead, use the timer path for everything. The key insight: the
timer path already does what a blocking read needs, if you let it
preempt kernel-mode processes.

1. sys_read blocks by marking the process BLOCKED and then yielding
   to the scheduler. Use the existing context_switch, not a new
   primitive. Pseudocode:

       while (bytes_read < count) {
           cli;
           if (kbd_buffer_get(&c)) {
               sti;
               copy c to user;
               bytes_read++;
               continue;
           }
           self = process_get_current();
           if (!self || self->pid == 1) { sti; return bytes_read; }
           self->waiting_on = WAIT_KBD;
           self->state = PROC_STATE_BLOCKED;
           sti;
           process_yield();   /* existing context_switch */
       }

   The catch: context_switch's save path fabricates a 20-qword
   frame from prev->rip and prev->rsp, and for a process that blocks
   inside a syscall those PCB fields are stale (they hold the last
   user-preemption values, not the current kernel RIP/RSP). So
   context_switch's save path needs to correctly record the current
   ring-0 kernel context when prev is being saved from ring 0.
   That is the one real change to context_switch.asm this design
   needs. Derive the layout from llvm-objdump, not from first
   principles; the previous session lost many rounds to guessing.

2. irq1_handler wakes the blocked process by moving it to READY
   and putting it on the ready queue. The timer picks it up on the
   next tick and resumes it through context_switch's .kernel_task
   path. Because the process was saved by context_switch's ring-0
   save path, its frame is an 18-qword ring-0 frame, and the
   timer's pop 15; iretq handles it correctly.

3. The timer must be able to preempt a kernel-mode current. Today,
   timer_preempt_handler declines to preempt a kernel-mode process
   other than idle:

       if ((frame->cs & 3) == 0) {
           if (current->pid != 1) {
               return stack_pointer;   /* prevents blocking from working */
           }
           ...
       }

   That branch is what makes sys_read's sti; hlt loop monopolize
   the scheduler. Remove that restriction (except for idle) and the
   scheduler runs normally while a process is blocked in a syscall.
   The process's kernel context is saved by the timer into
   current->r15..rsp, and it is resumed by the iretq like any other
   task. This is the single most important change.

4. The keyboard wake path does not need to switch. irq1_handler
   moves the waiter to READY and adds it to the ready queue. The
   timer picks it. Idle is the fallback if nothing else is READY.

Why this design

- Uses the existing context_switch for everything. No second
  primitive, no frame-kind ambiguity.
- Ring-0 frames are 18-qword frames, exactly what irq0_stub and
  context_switch.kernel_task already handle. No new frame shapes.
- The blocked process is off the ready queue, so it doesn't get
  scheduled. When woken, it's back on the queue, and the timer
  picks it. This is the "system still runs" property: idle always
  runs, other processes run, the blocked one sleeps.
- Multiple shells work because each is an ordinary user process
  that blocks independently. The keyboard buffer is shared;
  whichever shell wakes first consumes the byte.
- No iretq crosses a privilege boundary in the block/wake path.

What not to do

- Do not build a second context-switch primitive. The previous
  session tried context_switch_coop and context_save_and_switch_kernel
  and each introduced a new class of bug.
- Do not fabricate iretq frames from PCB slots for cooperative
  switches. The PCB slots are stale for ring-0 saves.
- Do not have kbd_wake_all perform the switch. Move the process
  to READY and let the timer do it.
- Do not use sti; hlt in sys_read. Mark BLOCKED and yield.

Files that matter most

    04_kernel_64bit/user_syscall_entry.asm   syscall entry (A2 refactor)
    04_kernel_64bit/user_syscall.c           sys_read, sys_write, ...
    04_kernel_64bit/interrupts.c             timer_preempt_handler,
                                             irq1_handler
    04_kernel_64bit/isr.asm                  irq0_stub, irq1_stub
    04_kernel_64bit/context_switch.asm       the switch primitive
    04_kernel_64bit/scheduler.c              scheduler_switch_to,
                                             process_yield, process_exit
    04_kernel_64bit/process.c                process_create,
                                             kernel_idle_loop, PCB layout
    04_kernel_64bit/include/process.h        pcb_t
    04_kernel_64bit/tss.c, tss.h             TSS.RSP0 and
                                             g_syscall_stack_top
    04_kernel_64bit/keyboard.c, keyboard.h   kbd_buffer_get/put

Verification

- Boot to kernel shell.
- usershell -> menu appears, shell blocks in read.
- Launch a second usershell. Both shells should have independent
  prompts.
- Type in one shell. The byte goes to whichever shell consumes it
  first. Both shells stay alive.
- Close both shells. Machine returns to idle, no hang.
- At no point should a blocked shell prevent the other shell or
  idle from running.

Ground rules that worked

- Paste the actual source before proposing a patch. Don't reason
  from memory.
- When a fault appears, get the exact fault signature (CR2, RIP,
  CS, error code), the address-to-symbol mapping
  (llvm-addr2line -e kernel.elf -f -C 0xADDR), and the
  disassembly window around the faulting RIP.
- Change one thing at a time. Build, run, verify.
- Tag before experimenting. Roll back immediately when something
  breaks. Don't fix forward on a broken tree.
- If a #GP at an iretq appears, dump the frame contents at the
  address the timer is about to consume, and compare against the
  expected layout. That single diagnostic ended several rounds of
  guessing in the previous session.


---

git reset --hard 20260913A
git clean -fdX
