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
#include "include/user_space.h"
#include "ff.h"

/* Defined in kmain.c — reboots the machine via keyboard controller + ACPI reset port. */
extern void handle_reboot_sequence(void);

#define WRITE_CHUNK 256
static char g_write_bounce[WRITE_CHUNK];

/* Maximum path length accepted from user space.  Matches the LFN
 * buffer budget: "0:/" + FF_MAX_LFN (255) + NUL, rounded up. */
#define USER_PATH_MAX 300

/* Maximum number of program headers we accept in an ELF. */
#define EXEC_MAX_PHDRS 16

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

static int copy_user_string(char* dst, size_t dst_cap, const char* user_src) {
    if (dst_cap == 0) return -1;
    size_t i = 0;
    while (i < dst_cap - 1) {
        char c;
        if (safe_copy_from_user(&c, user_src + i, 1) != 0) return -1;
        dst[i++] = c;
        if (c == '\0') return 0;
    }
    dst[dst_cap - 1] = '\0';
    return 0;
}

// ============================================================
// FILE TABLE HELPERS
// ============================================================
static void close_all_files(pcb_t* proc) {
    if (!proc) return;
    for (int i = 3; i < MAX_PROCESS_FILES; i++) {
        if (proc->file_table[i]) {
            f_close((FIL*)proc->file_table[i]);
            kfree(proc->file_table[i]);
            proc->file_table[i] = NULL;
        }
    }
}

// ============================================================
// EXTENSION DESCRIPTOR SYSCALLS
// ============================================================
long sys_open(const char* path, int flags) {
    pcb_t* self = process_get_current();
    if (!self || !path) return -1;

    char local_path[USER_PATH_MAX];
    if (copy_user_string(local_path, sizeof(local_path), path) != 0) return -1;

    int fd = -1;
    for (int i = 3; i < MAX_PROCESS_FILES; i++) {
        if (self->file_table[i] == NULL) {
            fd = i;
            break;
        }
    }
    if (fd == -1) return -1;

    BYTE mode = 0;

    switch (flags & 0x3) {
        case 0:  mode |= FA_READ;             break;  /* O_RDONLY */
        case 1:  mode |= FA_WRITE;            break;  /* O_WRONLY */
        case 2:  mode |= FA_READ | FA_WRITE;  break;  /* O_RDWR   */
        default: return -1;
    }

    if (flags & 0x0400) {                              /* O_TRUNC */
        mode |= FA_CREATE_ALWAYS;
    } else if (flags & 0x0200) {                       /* O_CREAT */
        if (flags & 0x0800) mode |= FA_CREATE_NEW;     /* O_CREAT|O_EXCL */
        else                mode |= FA_OPEN_ALWAYS;
    } else {
        mode |= FA_OPEN_EXISTING;
    }

    if (flags & 0x0008) mode |= FA_OPEN_APPEND;        /* O_APPEND */

    FIL* file_obj = (FIL*)kmalloc(sizeof(FIL));
    if (!file_obj) return -1;

    FRESULT r = f_open(file_obj, local_path, mode);
    if (r != FR_OK) {
        serial_print("sys_open: f_open FAIL path=");
        serial_print(local_path);
        serial_print(" flags=0x"); serial_print_hex((uint64_t)flags);
        serial_print(" mode=0x");  serial_print_hex((uint64_t)mode);
        serial_print(" r=");       serial_print_dec(r);
        serial_print("\n");
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

long sys_unlink(const char* path) {
    pcb_t* self = process_get_current();
    if (!self || !path) return -1;

    char local_path[USER_PATH_MAX];
    if (copy_user_string(local_path, sizeof(local_path), path) != 0) return -1;

    FRESULT r = f_unlink(local_path);
    if (r != FR_OK) {
        serial_print("sys_unlink: f_unlink FAIL path=");
        serial_print(local_path);
        serial_print(" r="); serial_print_dec(r);
        serial_print("\n");
        return -1;
    }
    return 0;
}

// ============================================================
// SYS_EXEC — spawn a process from an ELF on the FAT volume
// ============================================================
long sys_exec(const char* user_path) {
    pcb_t* self = process_get_current();
    if (!self || !user_path) return -1;

    char path[USER_PATH_MAX];
    if (copy_user_string(path, sizeof(path), user_path) != 0) {
        serial_print("sys_exec: bad path pointer\n");
        return -1;
    }

    FIL file;
    FRESULT fr = f_open(&file, path, FA_READ | FA_OPEN_EXISTING);
    if (fr != FR_OK) {
        serial_print("sys_exec: f_open(");
        serial_print(path);
        serial_print(") -> ");
        serial_print_dec(fr);
        serial_print("\n");
        return -1;
    }

    Elf64_Ehdr ehdr;
    UINT got = 0;
    fr = f_read(&file, &ehdr, sizeof(ehdr), &got);
    if (fr != FR_OK || got != sizeof(ehdr)) {
        serial_print("sys_exec: short read of ELF header\n");
        f_close(&file);
        return -1;
    }

    if (ehdr.e_ident[0] != ELF_MAGIC0 || ehdr.e_ident[1] != ELF_MAGIC1 ||
        ehdr.e_ident[2] != ELF_MAGIC2 || ehdr.e_ident[3] != ELF_MAGIC3) {
        serial_print("sys_exec: not an ELF file\n");
        f_close(&file);
        return -1;
    }
    if (ehdr.e_ident[4] != 2) { serial_print("sys_exec: not ELFCLASS64\n"); f_close(&file); return -1; }
    if (ehdr.e_ident[5] != 1) { serial_print("sys_exec: not little-endian\n"); f_close(&file); return -1; }
    if (ehdr.e_type != 2)     { serial_print("sys_exec: not ET_EXEC\n"); f_close(&file); return -1; }
    if (ehdr.e_machine != 62) { serial_print("sys_exec: not x86-64\n"); f_close(&file); return -1; }
    if (ehdr.e_phentsize != sizeof(Elf64_Phdr) ||
        ehdr.e_phnum == 0 || ehdr.e_phnum > EXEC_MAX_PHDRS) {
        serial_print("sys_exec: bad program header table\n");
        f_close(&file);
        return -1;
    }

    Elf64_Phdr phdrs[EXEC_MAX_PHDRS];
    if (ehdr.e_phoff > 0xFFFFFFFFULL) {
        serial_print("sys_exec: e_phoff out of range\n");
        f_close(&file);
        return -1;
    }
    fr = f_lseek(&file, (FSIZE_t)ehdr.e_phoff);
    if (fr != FR_OK) { serial_print("sys_exec: lseek to phdrs failed\n"); f_close(&file); return -1; }
    UINT phbytes = (UINT)(ehdr.e_phnum * sizeof(Elf64_Phdr));
    fr = f_read(&file, phdrs, phbytes, &got);
    if (fr != FR_OK || got != phbytes) {
        serial_print("sys_exec: short read of phdrs\n");
        f_close(&file);
        return -1;
    }

    FSIZE_t file_size = f_size(&file);
    if (file_size == 0 || file_size > 4ULL * 1024 * 1024) {
        serial_print("sys_exec: file size out of range\n");
        f_close(&file);
        return -1;
    }
    uint8_t* elf_buf = (uint8_t*)kmalloc((size_t)file_size);
    if (!elf_buf) {
        serial_print("sys_exec: kmalloc failed for ");
        serial_print_dec((uint64_t)file_size);
        serial_print(" bytes\n");
        f_close(&file);
        return -1;
    }
    fr = f_lseek(&file, 0);
    if (fr != FR_OK) { serial_print("sys_exec: rewind failed\n"); kfree(elf_buf); f_close(&file); return -1; }
    UINT total = 0;
    while (total < file_size) {
        UINT br = 0;
        UINT want = (UINT)(file_size - total);
        if (want > 4096) want = 4096;
        fr = f_read(&file, elf_buf + total, want, &br);
        if (fr != FR_OK) {
            serial_print("sys_exec: read failed at offset ");
            serial_print_dec(total);
            serial_print("\n");
            kfree(elf_buf); f_close(&file);
            return -1;
        }
        if (br == 0) break;
        total += br;
    }
    f_close(&file);
    if (total != file_size) {
        serial_print("sys_exec: short read of file body\n");
        kfree(elf_buf);
        return -1;
    }

    char proc_name[PROC_NAME_LEN];
    {
        const char* base = path;
        for (const char* p = path; *p; p++) {
            if (*p == '/' || *p == ':') base = p + 1;
        }
        int i = 0;
        while (base[i] && i < PROC_NAME_LEN - 1) {
            proc_name[i] = base[i];
            i++;
        }
        proc_name[i] = '\0';
        if (i == 0) {
            const char* fallback = "exec";
            for (i = 0; fallback[i] && i < PROC_NAME_LEN - 1; i++)
                proc_name[i] = fallback[i];
            proc_name[i] = '\0';
        }
    }

    pcb_t* child = process_create(proc_name, USER_CODE_BASE, 0);
    if (!child) {
        serial_print("sys_exec: process_create failed\n");
        kfree(elf_buf);
        return -1;
    }
    scheduler_ready_queue_remove(child);

    uint64_t entry = elf_load_into_process(child, elf_buf);
    kfree(elf_buf);

    if (entry == 0) {
        serial_print("sys_exec: elf_load_into_process failed\n");
        process_destroy(child);
        return -1;
    }

    child->entry_point = entry;
    child->rip = entry;

    uint64_t* frame = (uint64_t*)child->rsp;
    frame[15] = entry;

    child->parent_pid = self->pid;

    keyboard_buffer_flush();
    scheduler_ready_queue_add(child);

    serial_print("sys_exec: spawned pid=");
    serial_print_dec(child->pid);
    serial_print(" entry=0x");
    serial_print_hex(entry);
    serial_print(" (");
    serial_print(proc_name);
    serial_print(")\n");

    return (long)child->pid;
}

// ============================================================
// SYS_WAITPID
// ============================================================
long sys_waitpid(long pid, int* user_status, int options) {
    pcb_t* self = process_get_current();
    if (!self) return -1;

    uint64_t target = (pid <= 0) ? (uint64_t)-1 : (uint64_t)pid;

    for (;;) {
        pcb_t* zombie = NULL;
        pcb_t* live   = NULL;

        for (uint64_t child_pid = 1; child_pid < 1000; child_pid++) {
            pcb_t* c = process_find_by_pid(child_pid);
            if (!c) continue;
            if (c->parent_pid != self->pid) continue;
            if (target != (uint64_t)-1 && c->pid != target) continue;

            if (c->state == PROC_STATE_ZOMBIE) {
                zombie = c;
                break;
            }
            if (c->state != PROC_STATE_UNUSED) {
                live = c;
            }
        }

        if (zombie) {
            long reaped = (long)zombie->pid;
            int status = zombie->exit_status;
            process_reclaim(zombie);
            if (user_status) {
                if (safe_copy_to_user(user_status, &status, sizeof(status)) != 0) {
                    return -1;
                }
            }
            return reaped;
        }

        if (!live) {
            return -1;
        }

        if (options & WNOHANG) {
            return 0;
        }

        self->state = PROC_STATE_BLOCKED;
        self->block_kind = BLOCK_KIND_WAITPID;
        self->wait_pid = target;
        process_yield();
    }
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
                kfree(bounce);
                return (total_written > 0) ? (long)total_written : -1;
            }
            UINT written;
            if (f_write(file_obj, bounce, chunk, &written) != FR_OK) {
                kfree(bounce);
                return (total_written > 0) ? (long)total_written : -1;
            }
            total_written += written;
            if (written < chunk) break;
        }
        kfree(bounce);
        return (long)total_written;
    }

    return -1;
}

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
            self->block_kind = BLOCK_KIND_NONE;
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

            uint64_t map_flags = PT_PRESENT | PT_WRITE | PT_USER;
            vmm_map_page_in_cr3(current->cr3, virt, phys, map_flags);
            elf_add_page_to_pcb(current, phys);
        }
    }
    current->brk_virt = new_brk;
    return (void*)new_brk;
}

long sys_getpid(void) {
    pcb_t* current = process_get_current();
    if (!current) return 1;
    return (long)current->pid;
}

void sys_exit(int status) {
    pcb_t* self = process_get_current();
    if (self) {
        self->exit_status = status;
    }
    close_all_files(self);
    process_exit();
}

void sys_arch_set_fs(void* base) {
    uint64_t addr = (uint64_t)base;
    wrmsr(0xC0000100, addr);
}

static void kernel_do_reboot(void) {
    serial_print("[REBOOT] closing file handles before reset\n");
    close_all_files(process_get_current());
    handle_reboot_sequence();
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
        case 7:  return (uint64_t)sys_unlink((const char*)arg0);
        case 8:  return (uint64_t)sys_exec((const char*)arg0);
        case 9:  return (uint64_t)sys_waitpid((long)arg0, (int*)arg1, (int)arg2);
        case 10: return (uint64_t)sys_brk((long)arg0);
        case 20: return (uint64_t)sys_getpid();
        case 25: kernel_do_reboot(); return 0;
        case 5:  sys_arch_set_fs((void*)arg0); return 0;
        default:
            serial_print("Unknown syscall: ");
            serial_print_dec(num); serial_print("\n");
            return -1;
    }
}
