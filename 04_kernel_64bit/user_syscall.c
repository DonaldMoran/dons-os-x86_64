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

static int safe_copy_from_user(void* kernel_dest, const void* user_src, size_t count) {
    uint64_t base = (uint64_t)user_src;
    uint8_t* dst = (uint8_t*)kernel_dest;
    size_t copied = 0;
    while (copied < count) {
        uint64_t cur = base + copied;
        uint64_t phys_with_offset = vmm_get_phys(cur);
        if (!phys_with_offset) {
            serial_print("safe_copy_from_user: unmapped user address 0x");
            serial_print_hex(cur);
            serial_print("\n");
            return -1;
        }
        uint8_t* src = (uint8_t*)(HHDM_START + phys_with_offset);
        size_t chunk;
        {
            uint64_t page_off = cur & 0xFFF;
            size_t page_rem = 4096 - page_off;
            chunk = count - copied;
            if (chunk > page_rem) chunk = page_rem;
        }
        for (size_t i = 0; i < chunk; i++)
            dst[copied + i] = src[i];
        copied += chunk;
    }
    return 0;
}

static int safe_copy_to_user(void* user_dest, const void* kernel_src, size_t count) {
    uint64_t base = (uint64_t)user_dest;
    const uint8_t* src = (const uint8_t*)kernel_src;
    size_t copied = 0;
    while (copied < count) {
        uint64_t cur = base + copied;
        uint64_t phys_with_offset = vmm_get_phys(cur);
        if (!phys_with_offset) {
            serial_print("safe_copy_to_user: unmapped user address 0x");
            serial_print_hex(cur);
            serial_print("\n");
            return -1;
        }
        uint8_t* dst = (uint8_t*)(HHDM_START + phys_with_offset);
        size_t chunk;
        {
            uint64_t page_off = cur & 0xFFF;
            size_t page_rem = 4096 - page_off;
            chunk = count - copied;
            if (chunk > page_rem) chunk = page_rem;
        }
        for (size_t i = 0; i < chunk; i++)
            dst[i] = src[copied + i];
        copied += chunk;
    }
    return 0;
}

// ============================================================
// SYSCALL HANDLERS
// ============================================================

long sys_write(int fd, const void* buf, size_t count) {
    if (fd == 1) {
        #define MAX_SYS_WRITE 4096
        char kernel_buf[MAX_SYS_WRITE + 1];
        size_t to_copy = (count > MAX_SYS_WRITE ? MAX_SYS_WRITE : count);
        if (safe_copy_from_user(kernel_buf, buf, to_copy) == 0) {
            kernel_buf[to_copy] = '\0';
            vga_write(kernel_buf, to_copy);
            serial_write(kernel_buf, to_copy);
            return to_copy;
        } else {
            return -1;
        }
    }
    return 0;
}

long sys_read(int fd, void* buf, size_t count) {
    if (fd == 0) {
        char c;
        size_t bytes_read = 0;
        while (bytes_read < count) {
            // Enable interrupts and wait for key
            __asm__ volatile("sti");
            while (!kbd_buffer_get(&c)) {
                __asm__ volatile("hlt");
            }
            // Do NOT echo here – user-space handles echo
            if (safe_copy_to_user(buf + bytes_read, &c, 1) == 0)
                bytes_read++;
            else
                return -1;
        }
        return bytes_read;
    }
    return 0;
}

void* sys_brk(long inc) {
    pcb_t* current = process_get_current();
    if (!current) return (void*)-1;

    // Anchor user heap securely inside the verified 0x8000xxxxxx segment space
    static uint64_t heap_base = 0;
    if (heap_base == 0) {
        heap_base = 0x8000200000ULL;
    }
    
    uint64_t old_brk = current->brk_virt;
    if (old_brk == 0) {
        current->brk_virt = heap_base;
        old_brk = heap_base;
    }
    
    // If user space is just querying the current break position
    if (inc == 0) {
        return (void*)current->brk_virt;
    }
    
    uint64_t new_brk = old_brk + inc;
    if (inc < 0 && new_brk < heap_base) {
        return (void*)-1;
    }
    
    uint64_t old_page = (old_brk + 0xFFF) & ~0xFFFULL;
    uint64_t new_page = (new_brk + 0xFFF) & ~0xFFFULL;
    
    if (new_page > old_page) {
        for (uint64_t virt = old_page; virt < new_page; virt += 4096) {
            uint64_t phys = pmm_alloc_page_for_elf();
            if (!phys) return (void*)-1;
            
            // Clean physical page memory in kernel space via HHDM before exposing it
            void* hhdm = (void*)(HHDM_START + phys);
            for (uint64_t j = 0; j < 4096 / 8; j++) {
                ((uint64_t*)hhdm)[j] = 0ULL;
            }
            
            // FIX: Map pages explicitly within the active process's unique CR3 space
            // instead of calling vmm_map_page on the kernel's root table.
            uint64_t map_flags = PT_PRESENT | PT_WRITE | PT_USER;
            
            // Invoke the process-aware virtual memory mapper
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
    if (current && current->pid != 1) {
        serial_print("SYSCALL: Terminating process PID ");
        serial_print_dec(current->pid);
        serial_print(" cleanly.\n");
        
        current->state = PROC_STATE_TERMINATED;
        
        extern void scheduler_ready_queue_remove(pcb_t* pcb);
        scheduler_ready_queue_remove(current);
    }
    
    // Organsied scheduler context switch to the next ready task
    process_yield();

    while (1) {
        __asm__ volatile("hlt");
    }
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
        case SYS_WRITE: return (uint64_t)sys_write((int)arg0, (const void*)arg1, (size_t)arg2);
        case SYS_READ:  return (uint64_t)sys_read((int)arg0, (void*)arg1, (size_t)arg2);
        case SYS_BRK:   return (uint64_t)sys_brk((long)arg0);
        case SYS_EXIT:  sys_exit((int)arg0); return 0;
        case SYS_ARCH_SET_FS: sys_arch_set_fs((void*)arg0); return 0;
        
        case SYS_PROCLIST: 
            vga_print("\n"); // Clear a clean line on the display
            extern void process_dump_all(void);
            process_dump_all(); // Invoke your kernel-space process viewer
            return 0;
        // FIXED INTEGRATION: Route system call number 105 down to the unified reboot sequence
        case SYS_REBOOT:
            vga_print("\n"); // Clear a clean line on the display
            extern void handle_reboot_sequence(void);
            handle_reboot_sequence();
            return 0;
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
