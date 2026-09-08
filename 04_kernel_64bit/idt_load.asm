[bits 64]

global idt_load
extern idt_descriptor

idt_load:
    lidt [rel idt_descriptor]
    ret
