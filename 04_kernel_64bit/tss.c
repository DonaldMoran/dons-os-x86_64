#include <stddef.h>
#include "include/tss.h"
#include "include/gdt.h"
#include "include/vga.h"
#include "include/serial.h"
#include "include/vmm.h"

static tss_t* tss = (tss_t*)TSS_PHYS_ADDR;
static uint8_t* iomap = (uint8_t*)(TSS_PHYS_ADDR + sizeof(tss_t));

uint64_t kernel_stack[4096] __attribute__((aligned(16)));
uint64_t *kernel_stack_top = &kernel_stack[4096];


void tss_init(void) {
    // Zero the entire TSS region (TSS + I/O Permission Bitmap)
    uint8_t* buffer_ptr = (uint8_t*)tss;
    for (size_t i = 0; i < TSS_TOTAL_SIZE; i++) {
        buffer_ptr[i] = 0;
    }

    // I/O Permission Bitmap terminator: all bits set at the end of the
    // bitmap means every port is permitted.
    iomap[TSS_IOMAP_SIZE] = 0xFF;

    // RSP0 = top of the kernel stack (used on ring-3 -> ring-0 transitions
    // and by the scheduler when switching to a user task)
    tss->rsp0 = (uint64_t)&kernel_stack[4096];

    // I/O bitmap base = end of the TSS structure
    tss->iopb_base = sizeof(tss_t);

    // Point the GDT TSS descriptor at our TSS
    gdt_set_tss((uint64_t)tss, TSS_TOTAL_SIZE - 1);

    // Load TR with selector 0x38
    __asm__ volatile (
        "cli\n\t"
        "ltr %%ax\n\t"
        "sti"
        :
        : "a"(0x38)
        : "memory"
    );

    // Verify TR loaded correctly
    uint16_t tr;
    __asm__ volatile ("str %%ax" : "=a"(tr) : : "memory");

    if (tr == 0x38) {
        serial_print("TSS: TR loaded correctly (0x38)\n");
    } else {
        serial_print("TSS: ERROR - TR register is 0x");
        serial_print_hex(tr);
        serial_print(" (expected 0x38)\n");
    }
}


/* Called from the timer preempt path. Keep as a real function call (no
   inlining) so the surrounding frame layout stays exactly what the
   assembly stub expects. See timer_preempt_handler in interrupts.c for
   the full explanation of this boundary. */
void __attribute__((noinline))
tss_set_kernel_stack(uint64_t stack) {
    tss->rsp0 = stack;
}
