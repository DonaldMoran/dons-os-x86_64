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
#include "include/heap.h"
#include "ff.h"

#define WRITE_CHUNK 256
static char g_write_bounce[WRITE_CHUNK];

// ============================================================
// SAFE COPY OPERATIONS
// ============================================================
static int safe_copy_from_user(void* kernel_dest, const void* user_src, size_t count) {
    pcb_t* current = process_get_current();
    if (!current) return -1;

    uint64_t base = (uint64_t)user_src;
    uint8_t* dst = (uint8_t*)kernel_dest;
    size_t copied = 0;
    while (copied < count) {
        uint64_t cur = base + copied;
        uint64_t phys_with_offset = vmm_get_phys_from_cr3(current->cr3, cur);
        if (!phys_with_offset) return -1;

        const uint8_t* src = (const uint8_t*)(HHDM_START + phys_with_offset);
        size_t chunk = count - copied;
        uint64_t page_off = cur & 0xFFF;
        size_t page_rem = 4096 - page_off;
        if (chunk > page_rem) chunk = page_rem;

        for (size_t i = 0; i < chunk; i++) dst[copied + i] = src[i];
        copied += chunk;
    }
    return 0;
}

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
// EXTENSION DESCRIPTOR SYSCALLS
// ============================================================
long sys_open(const char* path, int flags) {
    pcb_t* self = process_get_current();
    if (!self || !path) return -1;

    char local_path[128];
    if (safe_copy_from_user(local_path, path, 127) != 0) return -1;
    local_path[127] = '\0';

    int fd = -1;
    for (int i = 3; i < MAX_PROCESS_FILES; i++) {
        if (self->file_table[i] == NULL) {
            fd = i;
            break;
        }
    }
    if (fd == -1) return -1;

    BYTE mode = FA_READ;
    if (flags == 1) mode = FA_WRITE;
    else if (flags == 2) mode = FA_READ | FA_WRITE;
    if (flags & 0x0200) mode |= FA_CREATE_ALWAYS;

    FIL* file_obj = (FIL*)kmalloc(sizeof(FIL));
    if (!file_obj) return -1;

    if (f_open(file_obj, local_path, mode) != FR_OK) {
        kfree(file_obj);
        return -1;
    }

    self->file_table[fd] = file_obj;
    return fd;
}

long sys_close(int fd) {
    pcb_t* self = process_get_current();
    if (!self || fd < 3 || fd >= MAX_PROCESS_FILES || !self->file_table[fd]) return -1;

    FIL* file_obj = (FIL*)self->file_table[fd];
    f_close(file_obj);
    kfree(file_obj);
    self->file_table[fd] = NULL;
    return 0;
}
// ============================================================
// CORE IO REDIRECTION PIPES
// ============================================================
long sys_write(int fd, const void* buf, size_t count) {
    if (!buf || count == 0) return 0;
    pcb_t* self = process_get_current();
    if (!self) return -1;

    if (fd == 1 || fd == 2) {
        size_t remaining = count;
        const uint8_t* user_ptr = (const uint8_t*)buf;
        while (remaining > 0) {
            size_t chunk = remaining > WRITE_CHUNK ? WRITE_CHUNK : remaining;
            if (safe_copy_from_user(g_write_bounce, user_ptr, chunk) != 0) return -1;
            for (size_t i = 0; i < chunk; i++) {
                char c = g_write_bounce[i];
                serial_putc(c); vga_putc(c);
            }
            user_ptr += chunk; remaining -= chunk;
        }
        return (long)count;
    }

    if (fd >= 3 && fd < MAX_PROCESS_FILES && self->file_table[fd]) {
        FIL* file_obj = (FIL*)self->file_table[fd];
        char* bounce = (char*)kmalloc(512);
        if (!bounce) return -1;

        size_t total_written = 0;
        while (total_written < count) {
            size_t chunk = (count - total_written) > 512 ? 512 : (count - total_written);
            if (safe_copy_from_user(bounce, (const uint8_t*)buf + total_written, chunk) != 0) {
                kfree(bounce); return -1;
            }
            UINT written;
            if (f_write(file_obj, bounce, chunk, &written) != FR_OK) {
                kfree(bounce); return -1;
            }
            total_written += written;
            if (written < chunk) break;
        }
        kfree(bounce);
        return (long)total_written;
    }

    return (long)count;
}
//~ long sys_write(int fd, const void* buf, size_t count) {
    //~ if (!buf || count == 0) return 0;
    //~ pcb_t* self = process_get_current();
    //~ if (!self) return -1;

    //~ if (fd == 1 || fd == 2) {
        //~ size_t remaining = count;
        //~ const uint8_t* user_ptr = (const uint8_t*)buf;
        //~ while (remaining > 0) {
            //~ size_t chunk = remaining > WRITE_CHUNK ? WRITE_CHUNK : remaining;
            //~ if (safe_copy_from_user(g_write_bounce, user_ptr, chunk) != 0) return -1;
            //~ for (size_t i = 0; i < chunk; i++) {
                //~ char c = g_write_bounce[i];
                //~ serial_putc(c); vga_putc(c);
            //~ }
            //~ user_ptr += chunk; remaining -= chunk;
        //~ }
        //~ return (long)count;
    //~ }

    //~ /* File writes not supported while FF_FS_READONLY is 1. */
    //~ return -1;
//~ }
long sys_read(int fd, void* buf, size_t count) {
    if (!buf || count == 0) return 0;
    pcb_t* self = process_get_current();
    if (!self) return 0;

    if (fd == 0) {
        char c; size_t bytes_read = 0; uint8_t* dest_ptr = (uint8_t*)buf;
        while (bytes_read < count) {
            __asm__ volatile("cli");
            if (kbd_buffer_get(&c)) {
                __asm__ volatile("sti");
                if (safe_copy_to_user(dest_ptr + bytes_read, &c, 1) == 0) bytes_read++;
                else return -1;
                continue;
            }
            if (self->pid == 1) { __asm__ volatile("sti"); __asm__ volatile("hlt"); continue; }
            self->state = PROC_STATE_BLOCKED;
            __asm__ volatile("sti"); __asm__ volatile("hlt");
        }
        return (long)bytes_read;
    }

    if (fd >= 3 && fd < MAX_PROCESS_FILES && self->file_table[fd]) {
        FIL* file_obj = (FIL*)self->file_table[fd];
        char* bounce = (char*)kmalloc(512);
        if (!bounce) return -1;

        size_t total_read = 0;
        while (total_read < count) {
            size_t chunk = (count - total_read) > 512 ? 512 : (count - total_read);
            UINT read_bytes;
            if (f_read(file_obj, bounce, chunk, &read_bytes) != FR_OK) {
                kfree(bounce); return -1;
            }
            if (read_bytes == 0) break;
            if (safe_copy_to_user((uint8_t*)buf + total_read, bounce, read_bytes) != 0) {
                kfree(bounce); return -1;
            }
            total_read += read_bytes;
            if (read_bytes < chunk) break;
        }
        kfree(bounce);
        return (long)total_read;
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
        current->brk_virt = heap_base; old_brk = heap_base;
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

            /* FIXED: Standard user memory permissions without the 0x18 caching bits */
            uint64_t map_flags = PT_PRESENT | PT_WRITE | PT_USER;
            vmm_map_page_in_cr3(current->cr3, virt, phys, map_flags);
            elf_add_page_to_pcb(current, phys);
        }
    }
    current->brk_virt = new_brk;
    return (void*)new_brk;
}

/* Find this section at the bottom of 04_kernel_64bit/user_syscall.c and replace it: */

long sys_getpid(void) {
    pcb_t* current = process_get_current();
    if (!current) return 1;
    return (long)current->pid;
}

void sys_exit(int status) {
    (void)status;
    pcb_t* self = process_get_current();
    if (self) {
        for (int i = 3; i < MAX_PROCESS_FILES; i++) {
            if (self->file_table[i]) {
                f_close((FIL*)self->file_table[i]);
                kfree(self->file_table[i]);
                self->file_table[i] = NULL;
            }
        }
    }
    process_exit();
}

void sys_arch_set_fs(void* base) {
    uint64_t addr = (uint64_t)base;
    wrmsr(0xC0000100, addr);
}

uint64_t syscall_dispatch(uint64_t num,
                          uint64_t arg0, uint64_t arg1, uint64_t arg2,
                          uint64_t arg3, uint64_t arg4, uint64_t arg5) {
    (void)arg3; (void)arg4; (void)arg5;
    switch (num) {
        case 1:  return (uint64_t)sys_write((int)arg0, (const void*)arg1, (size_t)arg2);
        case 2:  sys_exit((int)arg0); return 0;
        case 3:  return (uint64_t)sys_read((int)arg0, (void*)arg1, (size_t)arg2);
        case 4:  return (uint64_t)sys_open((const char*)arg0, (int)arg1);
        case 6:  return (uint64_t)sys_close((int)arg0);
        case 10: return (uint64_t)sys_brk((long)arg0);
        case 20: return (uint64_t)sys_getpid();                     /* NEW: Vector 20 */
        case 5:  sys_arch_set_fs((void*)arg0); return 0;
        default:
            serial_print("Unknown syscall: ");
            serial_print_dec(num); serial_print("\n");
            return -1;
    }
}
