#ifndef SERIAL_H
#define SERIAL_H

#include <stddef.h>
#include <stdint.h>

void serial_init(void);
void serial_putc(char c);
void serial_print(const char* str);
void serial_print_hex(uint64_t value);
void serial_print_dec(uint64_t value);
void serial_write(const char* buf, size_t count);

/*
 * Reentrant print lock.
 *
 * serial_lock() disables interrupts and increments a nesting counter.
 * serial_unlock() decrements it and re-enables interrupts only when the
 * counter returns to zero. Nested lock/unlock pairs are safe.
 *
 * Use this to make a multi-part message atomic against timer
 * preemption. Example:
 *
 *     serial_lock();
 *     serial_print("ATA: probe drive ");
 *     serial_print_dec(n);
 *     serial_print(": no device\n");
 *     serial_unlock();
 *
 * Without the lock, a timer tick between any two serial_print calls
 * writes its own output into the middle of the message.
 *
 * Limitations (see MAINTENANCE.md item 4d/4e):
 *   - Holds interrupts off for the duration of the message. At 115200
 *     baud a 40-char line takes ~3.5 ms, so a single message can delay
 *     the timer by that much. Fine for boot messages; not a design
 *     for high-frequency kernel printing.
 *   - Global, not per-console. With multiple users this would need to
 *     become a per-tty lock.
 *   - Not SMP-safe. cli/sti is meaningless on the other CPU.
 *
 * The correct long-term design is a ring buffer drained by a console
 * task, with per-console locks. This lock is a pragmatic fix for boot
 * interleaving and will be replaced when the tty subsystem exists.
 */
void serial_lock(void);
void serial_unlock(void);

#endif
