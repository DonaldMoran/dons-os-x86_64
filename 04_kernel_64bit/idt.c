#include "idt.h"

#define IDT_SIZE 256

struct idt_entry idt[IDT_SIZE];
struct idt_ptr   idt_descriptor;

extern void isr0_stub(void);
extern void isr1_stub(void);
extern void isr13_stub(void);

extern void irq0_stub(void);
extern void irq1_stub(void);

extern void pic_remap(void);
extern void idt_load(void);

static void set_idt_entry(int vec, uint64_t handler) {
    idt[vec].offset_low  = handler & 0xFFFF;
    idt[vec].selector    = 0x18;
    idt[vec].ist         = 0;
    idt[vec].type_attr   = 0x8E;
    idt[vec].offset_mid  = (handler >> 16) & 0xFFFF;
    idt[vec].offset_high = (handler >> 32) & 0xFFFFFFFF;
    idt[vec].zero        = 0;
}

void idt_init(void) {
    set_idt_entry(0, (uint64_t)isr0_stub);
    set_idt_entry(1, (uint64_t)isr1_stub);
    set_idt_entry(13, (uint64_t)isr13_stub);   // <-- GPF handler

    pic_remap();

    set_idt_entry(32, (uint64_t)irq0_stub);
    set_idt_entry(33, (uint64_t)irq1_stub);

    idt_descriptor.limit = sizeof(idt) - 1;
    idt_descriptor.base  = (uint64_t)&idt;

    idt_load();
}
