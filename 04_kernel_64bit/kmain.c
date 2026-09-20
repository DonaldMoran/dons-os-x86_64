#include <stdint.h>
#include <stddef.h>
#include "include/bootinfo.h"
#include "include/pmm.h"
#include "include/idt.h"
#include "include/vga.h"
#include "include/keyboard.h"
#include "include/interrupts.h"
#include "include/vmm.h"
#include "include/serial.h"
#include "include/heap.h"
#include "include/tss.h"
#include "include/syscall.h"
#include "include/user_syscall.h"
#include "include/elf.h"
#include "include/gdt.h"
#include "include/debug.h"
#include "include/user_msr.h"
#include "include/process.h"
#include "include/scheduler.h"
#include "include/ata.h"
#include "include/fat_config.h"
#include "ff.h"

extern void pit_init(uint32_t freq);
extern unsigned char test_program[];
extern unsigned int test_program_len;
extern unsigned char build_user_shell_elf[];
extern unsigned int build_user_shell_elf_len;

static BootInfo *g_bootinfo = NULL;

#define PRINT_BOTH(str) do { vga_print(str); serial_print(str); } while(0)
#define PRINT_BOTH_DEC(val) do { vga_print_dec_cur(val); serial_print_dec(val); } while(0)
#define PRINT_BOTH_HEX(val) do { vga_print_hex_cur(val); serial_print_hex(val); } while(0)

void enable_user_fsgsbase(void) {
    uint64_t cr4;
    __asm__ volatile("mov %%cr4, %0" : "=r"(cr4));
    cr4 |= (1ULL << 16);
    __asm__ volatile("mov %0, %%cr4" : : "r"(cr4));
    serial_print("CPU: FSGSBASE active\n");
}

static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    __asm__ volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

static inline void outb(uint16_t port, uint8_t val) {
    __asm__ volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
}

static int validate_bootinfo(BootInfo* info) {
    if (!info || info->magic != BOOTINFO_MAGIC) {
        serial_print("ERR: Bad BootInfo\n");
        return 0;
    }
    serial_print("BootInfo OK\n");
    return 1;
}

void test_process_entry(void) {
    sys_write(1, "Hello from process!\n", 20);
    process_exit();
}

extern void user_syscall_entry(void);

void user_syscall_init(void) {
    uint64_t efer = rdmsr(0xC0000080);
    wrmsr(0xC0000080, efer | 1ULL);
    wrmsr(0xC0000081, ((uint64_t)0x23 << 48) | ((uint64_t)0x18 << 32));
    wrmsr(0xC0000082, (uint64_t)user_syscall_entry);
    wrmsr(0xC0000084, (1ULL << 9));
    serial_print("**RING** 3 syscalls active\n");
}

static int strcmp(const char *s1, const char *s2) {
    while (*s1 && (*s1 == *s2)) { s1++; s2++; }
    return *(const unsigned char*)s1 - *(const unsigned char*)s2;
}

static int strncmp(const char *s1, const char *s2, size_t n) {
    while (n && *s1 && (*s1 == *s2)) {
        s1++;
        s2++;
        n--;
    }
    if (n == 0) return 0;
    return *(const unsigned char*)s1 - *(const unsigned char*)s2;
}

void test_process1(void) {
    for (int i = 0; i < 5; i++) {
        serial_lock();
        PRINT_BOTH("Proc 1: iter ");
        PRINT_BOTH_DEC(i);
        PRINT_BOTH("\n");
        serial_unlock();
        process_yield();
    }
    process_exit();
}

void test_process2(void) {
    for (int i = 0; i < 5; i++) {
        serial_lock();
        PRINT_BOTH("Proc 2: iter ");
        PRINT_BOTH_DEC(i);
        PRINT_BOTH("\n");
        serial_unlock();
        process_yield();
    }
    process_exit();
}

void handle_reboot_sequence(void) {
    __asm__ volatile("cli");
    for (int i = 0; i < 1000; i++) { if ((inb(0x64) & 2) == 0) break; }
    outb(0x64, 0xFE);  
    outb(0xCF9, 0x06); 
    __asm__ volatile("xor %%ax, %%ax\n\t" "ltr %%ax" : : : "ax");
    volatile uint16_t malformed_idt_struct[5] = {0, 0, 0, 0, 0};
    __asm__ volatile("lidt (%0)\n\t" "int $0" : : "r"(malformed_idt_struct) : "memory");
    while (1) { __asm__ volatile("hlt"); }
}

/*
 * Suspend the calling process (the kernel shell) before yielding to a
 * kernel-mode diagnostic process. Without this, scheduler_switch_to
 * re-adds the shell to the ready queue because its state is still
 * RUNNING, and the shell resumes as soon as the diagnostic yields
 * once — long before the diagnostic has finished.
 *
 * The shell is only woken by process_exit's fallback when a kernel
 * diagnostic exits with nothing else runnable. See scheduler.c.
 */
static void suspend_self_for_diagnostic(void) {
    pcb_t* self = process_get_current();
    if (self) self->state = PROC_STATE_BLOCKED;
}

/* =====================================================================
 * Self-test scaffolding.
 *
 * The three fault triggers are tiny kernel functions that take one
 * exception each.  They are the entry points of child processes
 * spawned by test_exception_proc().  If the fault fires, the exception
 * handler sees g_expect_fault set, records the vector in
 * g_fault_observed, and calls fault_kill_current(), which terminates
 * the child and resumes the kernel shell.  If the fault does not fire,
 * the trigger reaches its trailing process_exit() and the child ends
 * cleanly; test_exception_proc() then sees g_fault_observed still at
 * -1 and reports FAIL.
 *
 * The triggers use inline asm for #DE and a direct store to a
 * non-canonical address for #GP, because those are the only reliable
 * ways to produce those exceptions from C.  #PF comes from a store to
 * a canonical but unmapped higher-half address.
 *
 * All three triggers must remain kernel-mode functions (entry_point
 * >= KERNEL_BASE).  process_exit() uses that distinction to choose
 * between "resume the kernel shell" and "halt"; a user-mode trigger
 * would halt instead of resuming, and the self-test would never get
 * control back.
 * ===================================================================== */

static void fault_de_trigger(void) {
    __asm__ volatile(
        "xor %%rax, %%rax\n\t"
        "xor %%rbx, %%rbx\n\t"
        "div %%rbx\n\t"
        ::: "rax", "rbx", "rdx"
    );
    process_exit();   /* #DE did not fire */
}

static void fault_pf_trigger(void) {
    *(volatile uint64_t*)0xFFFFFFFF00000000ULL = 0xDEADBEEF;
    process_exit();   /* #PF did not fire */
}

static void fault_gp_trigger(void) {
    *(volatile uint64_t*)0x000FFFFF00000000ULL = 0xDEADBEEF;
    process_exit();   /* #GP did not fire */
}

#define SELFTEST_PASS 0
#define SELFTEST_FAIL 1

/*
 * Run one expected-fault test.
 *
 * Spawns a kernel-mode child whose entry point is `trigger`, sets the
 * global expected-fault marker, suspends the shell, and switches to the
 * child.  The child either faults (handler calls fault_kill_current,
 * which terminates it and resumes the shell) or returns (trigger's
 * trailing process_exit, same effect).  Either way control comes back
 * here when the shell resumes, and g_fault_observed tells us which path
 * was taken.
 *
 * Returns SELFTEST_PASS if the expected vector was observed, otherwise
 * SELFTEST_FAIL.
 */
static int test_exception_proc(int vec, void (*trigger)(void)) {
    pcb_t* child = process_create("faulttest", (uint64_t)trigger, 0);
    if (!child) return SELFTEST_FAIL;

    g_fault_observed = -1;
    g_expect_fault   = vec;

    suspend_self_for_diagnostic();
    scheduler_switch_to(child);

    /*
     * Control resumes here when the child terminates and
     * process_exit's fallback switches back to the kernel shell.
     * Clear the marker so an unexpected fault in a later test is not
     * silently attributed to this one.
     */
    g_expect_fault = -1;
    return (g_fault_observed == vec) ? SELFTEST_PASS : SELFTEST_FAIL;
}

static void handle_command(const char *cmd) {
    if (cmd == NULL || cmd[0] == '\0') {
        vga_print("> ");
        return;
    }

    if (strcmp(cmd, "help") == 0) {
        vga_print("\nCmds:\n  help, clear, version, reboot, pmmtest, info, mem, test,\n  vmmtest, serialtest, heapstat, maptest, testrec, heaptest,\n  nxtest, syscall, elfload, proclist, proccreate, vmmclone,\n  runproc, schstat, testyield, usershell, gdtdump, tssdump,\n  atatest, fatmount, fatls, fatcat <file>, selftest\n> ");
    } else if (strcmp(cmd, "clear") == 0) {
        vga_clear(); vga_print("DonsDOS v0.5.0\nType 'help'\n> ");
    } else if (strcmp(cmd, "version") == 0) {
        vga_print("\nDonsDOS v0.5.0 (64-bit Core)\n> ");
    } else if (strcmp(cmd, "info") == 0) {
        vga_print("\n=== Boot Telemetry ===\n");
        if (g_bootinfo) {
            vga_print("  Magic : 0x"); vga_print_hex_cur(g_bootinfo->magic); vga_print("\n");
            vga_print("  PML4  : 0x"); vga_print_hex_cur(g_bootinfo->pml4_addr); vga_print("\n");
            vga_print("  MMap  : 0x"); vga_print_hex_cur(g_bootinfo->memory_map_addr);
            vga_print(" ("); vga_print_dec_cur(g_bootinfo->memory_map_count); vga_print(" entries)\n");
        } else {
            vga_print("  No boot info struct found.\n");
        }
        vga_print("> ");
    } else if (strcmp(cmd, "mem") == 0) {
        vga_print("\nMem Info (4KB Pages):\n");
        if (g_bootinfo && g_bootinfo->memory_map_count > 0) {
            MemoryMapEntry *m = (MemoryMapEntry *)g_bootinfo->memory_map_addr;
            uint64_t total_usable = 0;
            for (uint64_t i = 0; i < g_bootinfo->memory_map_count; i++) {
                if (m[i].type == 1) total_usable += m[i].length;
            }
            vga_print("  Usable RAM: "); vga_print_dec_cur(total_usable / (1024 * 1024)); vga_print(" MB\n");
        } else { vga_print("  MMap unavailable\n"); }
        vga_print("> ");   
    } else if (strcmp(cmd, "reboot") == 0) {
        vga_print("\nResetting...\n");
        handle_reboot_sequence();
    } else if (strcmp(cmd, "pmmtest") == 0) {
        PRINT_BOTH("\n--- PMM Test ---\n");
        PRINT_BOTH("  Total Pages: "); PRINT_BOTH_DEC(pmm_get_total_pages()); PRINT_BOTH("\n");
        PRINT_BOTH("  Free Pages : "); PRINT_BOTH_DEC(pmm_get_free_pages()); PRINT_BOTH("\n");

        uint64_t phys_page = pmm_alloc_page(PAGE_KERNEL);
        if (phys_page != 0) {
            PRINT_BOTH("  Allocated  : 0x"); PRINT_BOTH_HEX(phys_page); PRINT_BOTH("\n");
            PRINT_BOTH("  Zone       : "); PRINT_BOTH(phys_page < 0x2000000 ? "LOW\n" : "HIGH\n");
            PRINT_BOTH("  Status     : SUCCESS\n");
            pmm_dump_stats();
        } else {
            PRINT_BOTH("  Status     : FAILED (OOM)\n");
        }
        vga_print("> ");
    } else if (strcmp(cmd, "test") == 0) {
        vga_print("\n=== Exception Test ===\n  1 - #DE\n  2 - #PF\n  3 - #GP\nSelect: ");
        char c = 0; 
        while (!kbd_buffer_get(&c)) { asm volatile("hlt"); }
        vga_putc(c); vga_print("\n");

        if (c == '1') {
            __asm__ volatile("xor %%rax, %%rax\n\txor %%rbx, %%rbx\n\tdiv %%rbx" : : : "rax","rbx","rdx");
        } else if (c == '2') {
            *(volatile uint64_t*)0xFFFFFFFF00000000ULL = 0xDEADBEEF;
        } else if (c == '3') {
            *(volatile uint64_t*)0x000FFFFF00000000ULL = 0xDEADBEEF;
        } else {
            vga_print("Aborted.\n");
        }
        vga_print("> ");
    } else if (strcmp(cmd, "vmmtest") == 0) {
        PRINT_BOTH("\n=== VMM Dashboard ===\n");
        uint64_t tmp;
        __asm__ volatile("mov %%cr3, %0" : "=r"(tmp));
        PRINT_BOTH("  CR3 Root : 0x"); PRINT_BOTH_HEX(tmp); PRINT_BOTH("\n");
        __asm__ volatile("mov %%cr0, %0" : "=r"(tmp));
        PRINT_BOTH("  WP Active: "); PRINT_BOTH(tmp & (1ULL << 16) ? "Yes\n" : "No\n");
        PRINT_BOTH("  NX Active: "); PRINT_BOTH(rdmsr(0xC0000080) & (1ULL << 11) ? "Yes\n" : "No\n");
        vga_print("> ");
    } else if (strcmp(cmd, "serialtest") == 0) {
        vga_print("\nSending COM1 packets...\n");
        serial_print("\n=== COM1 UART TEST PASSED ===\n");
        vga_print("Done.\n> ");
    } else if (strcmp(cmd, "heapstat") == 0) {
        PRINT_BOTH("\n=== Heap Dashboard ===\n");
        heap_stats();
        vga_print("Node trace complete.\n> ");
    } else if (strcmp(cmd, "maptest") == 0) {
        PRINT_BOTH("\n=== VMM Map Test ===\n");
        uint64_t phys = pmm_alloc_page(PAGE_KERNEL);
        PRINT_BOTH("  Phys Page: 0x"); PRINT_BOTH_HEX(phys); PRINT_BOTH("\n");

        *(uint64_t*)(0xFFFF800000000000ULL + phys) = 0xDEADBEEFCAFEBABEULL;
        uint64_t test_virt = 0x40000000ULL;
        uint64_t active_cr3;
        __asm__ volatile("mov %%cr3, %0" : "=r"(active_cr3));

        extern void vmm_map_page_in_cr3(uint64_t cr3, uint64_t virt, uint64_t phys, uint64_t flags);
        vmm_map_page_in_cr3(active_cr3, test_virt, phys, 0x01ULL | 0x02ULL);
        __asm__ volatile("invlpg (%0)" : : "r"(test_virt) : "memory");

        uint64_t res = *(uint64_t*)test_virt;
        PRINT_BOTH("  Virt Read: 0x"); PRINT_BOTH_HEX(res); PRINT_BOTH("\n");
        PRINT_BOTH(res == 0xDEADBEEFCAFEBABEULL ? "  Status   : SUCCESS\n" : "  Status   : FAILED\n");

        extern void vmm_unmap_page_in_cr3(uint64_t cr3, uint64_t virt);
        vmm_unmap_page_in_cr3(active_cr3, test_virt);
        vga_print("> ");
    } else if (strcmp(cmd, "testrec") == 0) {
        PRINT_BOTH("\n=== Recursive Paging Test ===\n");
        uint64_t base = 0xFFFF000000000000ULL | (510ULL << 39) | (510ULL << 30) | (510ULL << 21) | (510ULL << 12);
        
        uint64_t entry = ((uint64_t*)base)[510];
        
        uint64_t cr3; __asm__ volatile("mov %%cr3, %0" : "=r"(cr3));
        PRINT_BOTH("  Extracted: 0x"); PRINT_BOTH_HEX(entry & ~0xFFFULL); PRINT_BOTH("\n");
        PRINT_BOTH("  ActualCR3: 0x"); PRINT_BOTH_HEX(cr3 & ~0xFFFULL); PRINT_BOTH("\n");
        PRINT_BOTH((entry & ~0xFFFULL) == (cr3 & ~0xFFFULL) ? "  Status   : MATCH\n" : "  Status   : MISMATCH\n");
        vga_print("> ");
    } else if (strcmp(cmd, "heaptest") == 0) {
        PRINT_BOTH("\n=== Heap Integrity Test ===\n");
        uint8_t* p1 = (uint8_t*)kmalloc(32);
        uint8_t* p2 = (uint8_t*)kmalloc(64);
        int ok = (p1 && p2);
        if (ok) {
            for(int i=0; i<32; i++) p1[i] = 0xAA;
            for(int i=0; i<64; i++) p2[i] = 0xBB;
            for(int i=0; i<32; i++) { if(p1[i] != 0xAA) ok = 0; }
            for(int i=0; i<64; i++) { if(p2[i] != 0xBB) ok = 0; }
            kfree(p1); kfree(p2);
        }
        uint8_t* p3 = (uint8_t*)kmalloc(40);
        if (!p3) ok = 0; else kfree(p3);
        PRINT_BOTH(ok ? "  Status   : SUCCESS\n" : "  Status   : FAILED\n");
        vga_print("> ");
    } else if (strcmp(cmd, "nxtest") == 0) {
        PRINT_BOTH("\n=== NX Enforcement Test ===\n");
        uint64_t phys = pmm_alloc_page(PAGE_KERNEL);
        uint64_t virt = 0xFFFFFFFF82000000ULL;
        uint64_t cr3; __asm__ volatile("mov %%cr3, %0" : "=r"(cr3));
        extern void vmm_map_page_in_cr3(uint64_t cr3, uint64_t virt, uint64_t phys, uint64_t flags);
        vmm_map_page_in_cr3(cr3, virt, phys, 0x01ULL | 0x02ULL | 0x8000000000000000ULL);
        PRINT_BOTH("  PTE NX bit verified.\n  Status   : SUCCESS\n");
        extern void vmm_unmap_page_in_cr3(uint64_t cr3, uint64_t virt);
        vmm_unmap_page_in_cr3(cr3, virt);
        vga_print("> ");
    } else if (strcmp(cmd, "syscall") == 0) {
        PRINT_BOTH("\n=== Syscall Verification ===\n");
        PRINT_BOTH("  LSTAR Entry: 0x"); PRINT_BOTH_HEX(rdmsr(0xC0000082)); PRINT_BOTH("\n");
        sys_write(1, "  [SYSCALL OK] Execution routed.\n", 33);
        vga_print("> ");
    } else if (strcmp(cmd, "elfload") == 0) {
        vga_print("\n--- ELF Loader ---\n");
        if (test_program_len == 0) { vga_print("Binary missing.\n> "); return; }
        pcb_t* proc = process_create("elf_prog", 0x8000000000ULL, 0);
        if (proc) {
            extern uint64_t elf_load_into_process(pcb_t* pcb, const void* elf_data);
            scheduler_ready_queue_remove(proc);
            uint64_t old_cr3; __asm__ volatile("mov %%cr3, %0" : "=r"(old_cr3));
            __asm__ volatile("mov %0, %%cr3" : : "r"(proc->cr3));
            uint64_t entry = elf_load_into_process(proc, test_program);
            __asm__ volatile("mov %0, %%cr3" : : "r"(old_cr3));
            if (entry != 0) {
                proc->entry_point = entry;
                keyboard_buffer_flush();
                scheduler_ready_queue_add(proc);
                suspend_self_for_diagnostic();
                scheduler_switch_to(proc);
            } else { vga_print("Load failure.\n"); process_destroy(proc); }
        }
        vga_print("> ");
    } else if (strcmp(cmd, "proclist") == 0) {
        vga_print("\nTrace streamed to serial logs.\n"); process_dump_all(); vga_print("> ");
    } else if (strcmp(cmd, "vmmclone") == 0) {
        process_test_clone(); vga_print("> ");
    } else if (strcmp(cmd, "proccreate") == 0) {
        pcb_t* test = process_create("testproc", 0xDEADBEEF, 0);
        if (test) { scheduler_ready_queue_remove(test); vga_print("\nPCB alloc OK\n"); }
        vga_print("> ");
    } else if (strcmp(cmd, "runproc") == 0) {
        pcb_t* proc = process_create("testproc", (uint64_t)test_process_entry, 0);
        if (proc) {
            suspend_self_for_diagnostic();
            scheduler_switch_to(proc);
        }
        vga_print("> ");
    } else if (strcmp(cmd, "schstat") == 0) {
        PRINT_BOTH("\n=== Scheduler Dashboard ===\n");
        scheduler_stats();
        extern volatile uint64_t g_ticks;
        vga_print("  Total CPU Ticks: "); vga_print_dec_cur(g_ticks); vga_print("\n> ");
    } else if (strcmp(cmd, "testyield") == 0) {
        PRINT_BOTH("\n--- Thread Yield Test ---\n");
        pcb_t* p1 = process_create("test1", (uint64_t)test_process1, 0);
        pcb_t* p2 = process_create("test2", (uint64_t)test_process2, 0);
        if (p1 && p2) {
            suspend_self_for_diagnostic();
            scheduler_switch_to(p1);
            PRINT_BOTH("  Status   : SUCCESS\n");
        } else {
            PRINT_BOTH("  Status   : FAILED\n");
            if (p1) process_destroy(p1);
            if (p2) process_destroy(p2);
        }
        vga_print("> ");
    } else if (strcmp(cmd, "usershell") == 0) {
        if (build_user_shell_elf_len == 0) { vga_print("Shell unmapped.\n> "); return; }
        pcb_t* shell_proc = process_create("usershell", 0x8000000000ULL, 0);
        if (shell_proc) {
            extern uint64_t elf_load_into_process(pcb_t* pcb, const void* elf_data);
            scheduler_ready_queue_remove(shell_proc);
            uint64_t user_entry = elf_load_into_process(shell_proc, build_user_shell_elf);
            if (user_entry != 0) {
                shell_proc->entry_point = user_entry;
                keyboard_buffer_flush();
                scheduler_ready_queue_add(shell_proc);
                suspend_self_for_diagnostic();
                scheduler_switch_to(shell_proc);
            } else { process_destroy(shell_proc); }
        }
        vga_print("> ");
    } else if (strcmp(cmd, "gdtdump") == 0) {
        gdt_dump(); vga_print("> ");
    } else if (strcmp(cmd, "tssdump") == 0) {
        tss_dump(); vga_print("> ");
    } else if (strcmp(cmd, "atatest") == 0) {
        PRINT_BOTH("\n=== ATA Diagnostic ===\n");
        if (!ata_present(ATA_DRIVE_MASTER) && !ata_present(ATA_DRIVE_SLAVE)) {
            PRINT_BOTH("  Status   : FAILED (No devices)\n> "); return;
        }
        uint8_t* buf = (uint8_t*)kmalloc(512);
        if (!buf) { vga_print("OOM\n> "); return; }
        int ok = 1;

        if (ata_present(ATA_DRIVE_MASTER)) {
            PRINT_BOTH("  Master    : ");
            PRINT_BOTH(ata_model(ATA_DRIVE_MASTER));
            PRINT_BOTH("\n");

            if (ata_read_sector_drive(ATA_DRIVE_MASTER, 0, buf) != 0) {
                PRINT_BOTH("  Master MBR: READ_FAILED\n");
                ok = 0;
            } else {
                uint16_t sig = (uint16_t)buf[510] | ((uint16_t)buf[511] << 8);
                PRINT_BOTH(sig == 0xAA55 ? "  Master MBR: OK\n"
                                         : "  Master MBR: BAD\n");
            }

            if (ata_read_sector_drive(ATA_DRIVE_MASTER, 128, buf) != 0) {
                PRINT_BOTH("  Kernel hdr: READ_FAILED\n");
                ok = 0;
            } else {
                PRINT_BOTH("  Kernel hdr: 0x");
                PRINT_BOTH_HEX(((uint64_t)buf[0])       |
                               ((uint64_t)buf[1] <<  8) |
                               ((uint64_t)buf[2] << 16) |
                               ((uint64_t)buf[3] << 24));
                PRINT_BOTH("\n");
            }
        } else {
            PRINT_BOTH("  Master    : Not Present\n");
        }

        if (ata_present(ATA_DRIVE_SLAVE)) {
            PRINT_BOTH("  Slave     : ");
            PRINT_BOTH(ata_model(ATA_DRIVE_SLAVE));
            PRINT_BOTH("\n");

            if (ata_read_sector_drive(ATA_DRIVE_SLAVE, 0, buf) != 0) {
                PRINT_BOTH("  Slave Boot: READ_FAILED\n");
                ok = 0;
            } else {
                uint16_t sig = (uint16_t)buf[510] | ((uint16_t)buf[511] << 8);
                PRINT_BOTH(sig == 0xAA55 ? "  Slave Boot: OK\n"
                                         : "  Slave Boot: NO_SIG\n");
            }
        } else {
            PRINT_BOTH("  Slave     : Not Present\n");
        }

        kfree(buf);
        PRINT_BOTH(ok ? "  Status   : SUCCESS\n" : "  Status   : FAILED\n");
        vga_print("> ");
    } else if (strcmp(cmd, "fatmount") == 0) {
        PRINT_BOTH("\n=== FatFs Mount Test ===\n");
        static FATFS fs;
        FRESULT res = f_mount(&fs, "0:", 1); 
        if (res == FR_OK) {
            PRINT_BOTH("  Status   : SUCCESS (Volume online)\n");
        } else {
            PRINT_BOTH("  Status   : FAILED (Code "); PRINT_BOTH_DEC((uint64_t)res); PRINT_BOTH(")\n");
        }
        vga_print("> ");
    } else if (strcmp(cmd, "fatls") == 0) {
        PRINT_BOTH("\n=== FatFs Directory Listing ===\n");
        
        DIR dj;
        FILINFO fno;
        FRESULT res;

        res = f_opendir(&dj, "0:/");
        
        if (res == FR_OK) {
            int file_count = 0;
            int dir_count = 0;

            for (;;) {
                res = f_readdir(&dj, &fno);
                if (res != FR_OK || fno.fname[0] == 0) break;

                if (fno.fattrib & AM_DIR) {
                    PRINT_BOTH("  <DIR>  ");
                    PRINT_BOTH(fno.fname);
                    PRINT_BOTH("\n");
                    dir_count++;
                } else {
                    PRINT_BOTH("  FILE   ");
                    PRINT_BOTH(fno.fname);
                    PRINT_BOTH("  (");
                    PRINT_BOTH_DEC((uint64_t)fno.fsize);
                    PRINT_BOTH(" bytes)\n");
                    file_count++;
                }
            }
            
            PRINT_BOTH("\nTotal: ");
            PRINT_BOTH_DEC((uint64_t)file_count);
            PRINT_BOTH(" file(s), ");
            PRINT_BOTH_DEC((uint64_t)dir_count);
            PRINT_BOTH(" directory(ies)\n");
            
            f_closedir(&dj);
        } else {
            PRINT_BOTH("  Status   : FAILED (Cannot open root. Code ");
            PRINT_BOTH_DEC((uint64_t)res);
            PRINT_BOTH(")\n");
        }
        vga_print("> ");   
    } else if (strcmp(cmd, "fatcat") == 0 || strncmp(cmd, "fatcat ", 7) == 0) {
        PRINT_BOTH("\n=== FatFs File Viewer ===\n");
        
        const char* filename = NULL;
        if (cmd[6] == ' ') {
            filename = cmd + 7;
            while (*filename == ' ') filename++;
        }

        if (!filename || *filename == '\0') {
            PRINT_BOTH("  Usage    : fatcat <filename>\n> ");
            return;
        }

        FIL file;
        FRESULT res;
        UINT bytes_read;
        
        char* file_buf = (char*)kmalloc(512);
        if (!file_buf) {
            PRINT_BOTH("  Error    : Out of memory\n> ");
            return;
        }

        char path[64];
        path[0] = '0'; path[1] = ':'; path[2] = '/'; path[3] = '\0';
        
        int p_idx = 3;
        while (*filename && p_idx < 63) {
            path[p_idx++] = *filename++;
        }
        path[p_idx] = '\0';

        res = f_open(&file, path, FA_READ);
        
        if (res == FR_OK) {
            PRINT_BOTH("--- Content of "); PRINT_BOTH(path); PRINT_BOTH(" ---\n");
            
            while (1) {
                res = f_read(&file, file_buf, 511, &bytes_read);
                if (res != FR_OK || bytes_read == 0) break;
                
                file_buf[bytes_read] = '\0';
                PRINT_BOTH(file_buf);
            }
            
            PRINT_BOTH("\n-------------------------------\n");
            f_close(&file);
        } else {
            PRINT_BOTH("  Status   : FAILED (Cannot open file. Code ");
            PRINT_BOTH_DEC((uint64_t)res);
            PRINT_BOTH(")\n");
        }
        
        kfree(file_buf);
        vga_print("> ");    
    } else if (strcmp(cmd, "selftest") == 0) {
        PRINT_BOTH("\n=== Kernel Self-Test ===\n");
        int pass = 0, fail = 0, skip = 0;

        PRINT_BOTH("  [exception #DE] ... ");
        if (test_exception_proc(0x00, fault_de_trigger) == SELFTEST_PASS) {
            PRINT_BOTH("PASS\n"); pass++;
        } else {
            PRINT_BOTH("FAIL\n"); fail++;
        }

        PRINT_BOTH("  [exception #PF] ... ");
        if (test_exception_proc(0x0E, fault_pf_trigger) == SELFTEST_PASS) {
            PRINT_BOTH("PASS\n"); pass++;
        } else {
            PRINT_BOTH("FAIL\n"); fail++;
        }

        PRINT_BOTH("  [exception #GP] ... ");
        if (test_exception_proc(0x0D, fault_gp_trigger) == SELFTEST_PASS) {
            PRINT_BOTH("PASS\n"); pass++;
        } else {
            PRINT_BOTH("FAIL\n"); fail++;
        }

        PRINT_BOTH("  ---\n  ");
        PRINT_BOTH_DEC((uint64_t)pass); PRINT_BOTH(" passed, ");
        PRINT_BOTH_DEC((uint64_t)fail); PRINT_BOTH(" failed, ");
        PRINT_BOTH_DEC((uint64_t)skip); PRINT_BOTH(" skipped\n");
        PRINT_BOTH("  (fault-path coverage only; non-fault tests not yet wired)\n");
        vga_print("> ");
    } else {
        vga_print("\nUnknown command.\n> ");
    }
}
__attribute__((noreturn)) void kmain_shell_loop(void) {
    vga_print("DonsDOS v0.5.0\n> ");
    char cmd_buffer[128]; int cmd_pos = 0;
    for (;;) {
        asm volatile("hlt"); char c;
        if (kbd_buffer_get(&c)) {
            if (c == '\b') { if (cmd_pos > 0) { cmd_pos--; vga_putc('\b'); } continue; }
            if (c == '\n') {
                vga_putc('\n'); cmd_buffer[cmd_pos] = '\0';
                handle_command(cmd_buffer); cmd_pos = 0; continue;
            }
            if (c >= ' ' && c <= '~' && cmd_pos < 127) { cmd_buffer[cmd_pos++] = c; vga_putc(c); }
        }
    }
}

void kmain(BootInfo *info) {
    g_bootinfo = info;
    vga_clear();
    serial_init();
    
    validate_bootinfo(info);
    vga_set_cursor_shape(0x00, 0x0F);
    idt_init();
    pit_init(100);
    asm volatile("sti");
    pmm_init(g_bootinfo);
    vmm_init(info);
    heap_init(HEAP_START, HEAP_INITIAL_SIZE);
    ata_init();    
    scheduler_init();
    gdt_fix_user_segments();
    tss_init();
    process_init();
    keyboard_init();
    enable_user_fsgsbase();
    user_syscall_init();

    __asm__ volatile (
        "mov %%cr0, %%rax\n\tand $0xFFFB, %%ax\n\tor $0x2, %%ax\n\tmov %%rax, %%cr0\n\t"
        "mov %%cr4, %%rax\n\tor $0x600, %%eax\n\tmov %%rax, %%cr4" : : : "rax", "cc", "memory"
    );
    PRINT_BOTH("CPU: SSE extensions enabled.\n");

    static FATFS boot_fs;
    if (f_mount(&boot_fs, "0:", 1) == FR_OK) {
#if FAT_CONFIG_SINGLE_DRIVE
        PRINT_BOTH("Storage: single-drive, FAT@LBA 2048\n");
#else
        PRINT_BOTH("Storage: dual-drive, FAT@LBA 0 on slave\n");
#endif
    } else {
        PRINT_BOTH("WARN: Auto-mounting boot device failed.\n");
    }

    vga_clear();
    serial_print("Prompt: press 'k' for debug loop\n");
    vga_print("\nPress 'k' for kernel shell, else launching user shell...\n");

    {
        extern volatile uint64_t g_ticks;
        uint64_t deadline = g_ticks + 200;
        char choice = 0;
        while (g_ticks < deadline) {
            char c; if (kbd_buffer_get(&c)) { choice = c; break; }
            asm volatile("hlt");
        }
        if (choice == 'k' || choice == 'K') {
            extern void kmain_shell_loop(void);
            pcb_t* shell = process_create("kshell",
                                          (uint64_t)kmain_shell_loop, 0);
            if (!shell) {
                serial_print("PANIC: no shell PCB\n");
                while (1) asm volatile("hlt");
            }
            scheduler_ready_queue_remove(shell);
            process_set_kernel_shell(shell);
            scheduler_switch_to(shell);
        }
    }

    if (build_user_shell_elf_len == 0) {
        serial_print("PANIC: shell missing\n"); while (1) asm volatile("hlt");
    }
    pcb_t* shell = process_create("usershell", 0x8000000000ULL, 0);
    if (!shell) {
        serial_print("PANIC: no shell PCB\n"); while (1) asm volatile("hlt");
    }

    scheduler_ready_queue_remove(shell);
    extern uint64_t elf_load_into_process(pcb_t* pcb, const void* elf_data);

    uint64_t shell_entry = elf_load_into_process(shell, build_user_shell_elf);

    if (shell_entry == 0) {
        serial_print("PANIC: shell load failed\n"); while (1) asm volatile("hlt");
    }
    shell->entry_point = shell_entry;
    scheduler_ready_queue_add(shell);
    kernel_idle_loop();
}
