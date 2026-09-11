#include "include/user_syscall.h"
#include <stdint.h>
#include "include/serial.h"
#include "include/vmm.h"
#include "include/keyboard.h"
#include "include/syscall.h"
#include "include/vga.h"
#include "include/process.h"
#include "include/pmm.h"
#include "include/scheduler.h"
#include "include/user_msr.h"
#include "include/elf.h"

// ============================================================
// SAFE COPY FUNCTIONS
// ============================================================
static int safe_copy_to_user(void* user_dest, const void* kernel_src, size_t count) {
    pcb_t* current = process_get_current();
    if (!current) return -1;

    uint64_t base = (uint64_t)user_dest;
    const uint8_t* src = (const uint8_t*)kernel_src;
    size_t copied = 0;
    while (copied < count) {
        uint64_t cur = base + copied;
        uint64_t phys_with_offset = vmm_get_phys_from_cr3(current->cr3, cur);
        if (!phys_with_offset) return -1;
        
        uint8_t* dst = (uint8_t*)(HHDM_START + phys_with_offset);
        size_t chunk = count - copied;
        uint64_t page_off = cur & 0xFFF;
        size_t page_rem = 4096 - page_off;
        if (chunk > page_rem) chunk = page_rem;
        
        for (size_t i = 0; i < chunk; i++) dst[i] = src[copied + i];
        copied += chunk;
    }
    return 0;
}

// ============================================================
// SYSCALL HANDLERS
// ============================================================
long sys_write(int fd, const void* buf, size_t count) {
    if (!buf || count == 0) return -1;

    if (fd == 1 || fd == 2) {
        pcb_t* current = process_get_current();
        if (!current) return -1;

        uint64_t user_ptr = (uint64_t)buf;
        size_t processed = 0;

        while (processed < count) {
            uint64_t cur_addr = user_ptr + processed;
            uint64_t phys = vmm_get_phys_from_cr3(current->cr3, cur_addr);
            if (!phys) return (processed > 0 ? processed : -1);

            uint8_t* raw_ram = (uint8_t*)(HHDM_START + phys);
            char c = (char)*raw_ram;
            vga_putc(c);
            serial_putc(c);
            processed++;
        }
        return (long)processed;
    }
    return 0;
}

//~ long sys_read(int fd, void* buf, size_t count) {
    //~ if (fd == 0) {
        //~ char c;
        //~ size_t bytes_read = 0;
        //~ uint8_t* dest_ptr = (uint8_t*)buf; // Explicit typecast for safe pointer arithmetic
        
        //~ while (bytes_read < count) {
            //~ // Re-enable interrupts to allow keyboard hardware IRQ1 to fill the buffer
            //~ __asm__ volatile("sti");

            //~ // =======================================================================
            //~ // CRITICAL CORE FIX: COOPERATIVE YIELDING
            //~ // =======================================================================
            //~ // Instead of executing a low-level 'hlt' instruction which stalls the CPU mid-transit
            //~ // inside Ring 0 and leaves context registers vulnerable to preemption clobbering,
            //~ // we cleanly yield the timeslice back to the scheduler Ready Queue.
            //~ while (!kbd_buffer_get(&c)) {
                //~ process_yield();
            //~ }

            //~ if (safe_copy_to_user(dest_ptr + bytes_read, &c, 1) == 0) {
                //~ bytes_read++;
            //~ } else {
                //~ return -1;
            //~ }
        //~ }
        //~ return bytes_read;
    //~ }
    //~ return 0;
//~ }
long sys_read(int fd, void* buf, size_t count) {
    if (fd == 0) {
        char c;
        size_t bytes_read = 0;
        uint8_t* dest_ptr = (uint8_t*)buf;

        while (bytes_read < count) {
            __asm__ volatile("sti");

            while (!kbd_buffer_get(&c)) {
                process_yield();
            }

            //~ serial_print("\n[READ] got '");
            //~ serial_putc(c);
            //~ serial_print("' (0x");
            //~ serial_print_hex((uint8_t)c);
            //~ serial_print(")\n");

            if (safe_copy_to_user(dest_ptr + bytes_read, &c, 1) == 0) {
                bytes_read++;
            } else {
                return -1;
            }
        }
        return bytes_read;
    }
    return 0;
}


void* sys_brk(long inc) {
    pcb_t* current = process_get_current();
    if (!current) return (void*)-1;

    static uint64_t heap_base = 0;
    if (heap_base == 0) heap_base = 0x8000200000ULL;
    
    uint64_t old_brk = current->brk_virt;
    if (old_brk == 0) {
        current->brk_virt = heap_base;
        old_brk = heap_base;
    }
    if (inc == 0) return (void*)current->brk_virt;
    
    uint64_t new_brk = old_brk + inc;
    if (inc < 0 && new_brk < heap_base) return (void*)-1;
    
    uint64_t old_page = (old_brk + 0xFFF) & ~0xFFFULL;
    uint64_t new_page = (new_brk + 0xFFF) & ~0xFFFULL;
    
    if (new_page > old_page) {
        for (uint64_t virt = old_page; virt < new_page; virt += 4096) {
            uint64_t phys = pmm_alloc_page_for_elf();
            if (!phys) return (void*)-1;
            
            void* hhdm = (void*)(HHDM_START + phys);
            for (uint64_t j = 0; j < 4096 / 8; j++) ((uint64_t*)hhdm)[j] = 0ULL;
            
            uint64_t map_flags = PT_PRESENT | PT_WRITE | PT_USER | 0x18ULL;
            vmm_map_page_in_cr3(current->cr3, virt, phys, map_flags);
            elf_add_page_to_pcb(current, phys);
        }
    }
    current->brk_virt = new_brk;
    return (void*)new_brk;
}

void sys_exit(int status) {
    (void)status;

    pcb_t* current = process_get_current();

    serial_print("\n[EXIT] sys_exit, pid=");
    serial_print_dec(current ? current->pid : 0);
    serial_print(", state=");
    serial_print_dec(current ? current->state : 0);
    serial_print("\n");

    if (current && current->pid != 1) {
        current->state = PROC_STATE_TERMINATED;
        extern void scheduler_ready_queue_remove(pcb_t* pcb);
        scheduler_ready_queue_remove(current);
        serial_print("[EXIT] marked TERMINATED\n");
    }

    /* Do NOT call process_yield or context_switch. The timer preempt
     * handler checks for state == TERMINATED and returns immediately,
     * so this halt loop is safe and the CPU simply idles forever. */
    serial_print("[EXIT] halting in kernel loop\n");
    while (1) __asm__ volatile("hlt");
}

void sys_arch_set_fs(void* base) {
    uint64_t addr = (uint64_t)base;
    wrmsr(0xC0000100, addr);
}

// ============================================================
// DISPATCHER
// ============================================================
uint64_t syscall_dispatch(uint64_t num,
                          uint64_t arg0, uint64_t arg1, uint64_t arg2,
                          uint64_t arg3, uint64_t arg4, uint64_t arg5) {
    (void)arg3; (void)arg4; (void)arg5;
    switch (num) {
        case 1: return (uint64_t)sys_write((int)arg0, (const void*)arg1, (size_t)arg2);
        case 3: return (uint64_t)sys_read((int)arg0, (void*)arg1, (size_t)arg2);
        case 10: return (uint64_t)sys_brk((long)arg0);
        case 2:  sys_exit((int)arg0); return 0;
        case 5:  sys_arch_set_fs((void*)arg0); return 0;
        default:
            serial_print("Unknown syscall: ");
            serial_print_dec(num);
            serial_print("\n");
            return -1;
    }
}

void syscall_init(void) {
    extern void syscall_init_asm(void);
    syscall_init_asm();
    serial_print("SYSCALL init done\n");
}
