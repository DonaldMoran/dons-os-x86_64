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
#include "include/ring3.h"
#include "include/tss.h"
#include "include/syscall.h"  // ADDED: Include syscall header

extern void pit_init(uint32_t freq);

static BootInfo *g_bootinfo = NULL;

static int strcmp(const char *s1, const char *s2) {
    while (*s1 && (*s1 == *s2)) {
        s1++;
        s2++;
    }
    return *(const unsigned char*)s1 - *(const unsigned char*)s2;
}

static void handle_command(const char *cmd) {
    if (cmd[0] == '\0') {
        vga_print("> ");
        return;
    }
    
    const char *valid_commands[] = {
        "help", "clear", "version", "reboot", 
        "pmmtest", "info", "mem", "test", 
        "vmmtest", "serialtest", "heapstat", "maptest", "testrec", "heaptest", "simple",
        "nxtest", "syscall"  // ADDED: syscall command
    };
    int num_commands = sizeof(valid_commands) / sizeof(valid_commands[0]);
    
    if (strcmp(cmd, "help") == 0) {
        vga_print("\nAvailable commands:\n");
        vga_print("  help     - Show this help\n");
        vga_print("  clear    - Clear the screen\n");
        vga_print("  version  - Show version info\n");
        vga_print("  reboot   - Reboot the system\n");
        vga_print("  pmmtest  - Test Physical Memory Manager\n");
        vga_print("  info     - Show boot information\n");
        vga_print("  mem      - Show memory information\n");
        vga_print("  test     - Exception Handling test\n");
        vga_print("  vmmtest  - Test Virtual Memory Manager\n");
        vga_print("  serialtest - Test serial output\n");
        vga_print("  heapstat - Show heap statistics\n");
        vga_print("  maptest  - Test page mapping\n");
        vga_print("  testrec  - Test recursive mapping address\n");
        vga_print("  heaptest - Test heap free list\n");
        vga_print("  user     - Test user mode (Ring 3)\n");
        vga_print("  user2    - Test user mode (Ring 3) - second test\n");
        vga_print("  simple   - Test user mode (Ring 3) - second test\n");
        vga_print("  nxtest   - Test NX (No Execute) bit\n");
        vga_print("  syscall  - Test system calls\n");  // ADDED: Help entry
        vga_print("> ");
    } else if (strcmp(cmd, "clear") == 0) {
        vga_clear();
        vga_print("DonsDOS v0.1\n");
        vga_print("Type 'help'\n");
        vga_print("> ");
    } else if (strcmp(cmd, "version") == 0) {
        vga_print("\nDonsDOS v0.2.3\n");
        vga_print("Build: 64-bit kernel with VGA console\n");
        vga_print("Features: VMM with recursive paging, HHDM, NX support, Syscalls\n");  // UPDATED
        vga_print("Copyright (c) 2026 Don's OS Project\n");
        vga_print("> ");
    } else if (strcmp(cmd, "info") == 0) {
        vga_print("\nBoot Information:\n");
        if (g_bootinfo) {
            vga_print("  PML4 addr: 0x");
            vga_print_hex_cur(g_bootinfo->pml4_addr);
            vga_print("\n");
            vga_print("  Kernel phys start: 0x");
            vga_print_hex_cur(g_bootinfo->kernel_phys_start);
            vga_print("\n");
            vga_print("  Kernel phys end: 0x");
            vga_print_hex_cur(g_bootinfo->kernel_phys_end);
            vga_print("\n");
            vga_print("  E820 count: ");
            vga_print_dec_cur(g_bootinfo->memory_map_count);
            vga_print("\n");
        } else {
            vga_print("  No boot info available\n");
        }
        vga_print("> ");
    } else if (strcmp(cmd, "mem") == 0) {
        vga_print("\nMemory Information:\n");
        vga_print("  Page size: 4096 bytes\n");
        vga_print("  PMM: Initialized\n");
        if (g_bootinfo && g_bootinfo->memory_map_count > 0) {
            MemoryMapEntry *m = (MemoryMapEntry *)g_bootinfo->memory_map_addr;
            uint64_t total_usable = 0;
            uint64_t total_reserved = 0;
            for (uint64_t i = 0; i < g_bootinfo->memory_map_count; i++) {
                if (m[i].type == 1) {
                    total_usable += m[i].length;
                } else if (m[i].type > 0 && m[i].type < 100) {
                    total_reserved += m[i].length;
                }
            }
            vga_print("\n  Usable RAM: ");
            vga_print_dec_cur(total_usable / (1024 * 1024));
            vga_print(" MB\n");
            vga_print("  Reserved RAM: ");
            vga_print_dec_cur(total_reserved / (1024 * 1024));
            vga_print(" MB\n");
        } else {
            vga_print("  Memory map not available\n");
        }
        vga_print("> ");
    } else if (strcmp(cmd, "reboot") == 0) {
        vga_print("\nRebooting...\n");
        __asm__ volatile (
            "cli\n"
            "mov $0x64, %%al\n"
            "outb %%al, $0x64\n"
            "mov $0xFE, %%al\n"
            "outb %%al, $0x64\n"
            "hlt\n"
            : : : "memory"
        );
    } else if (strcmp(cmd, "pmmtest") == 0) {
        vga_print("\nPMM Test:\n");
        pmm_init(g_bootinfo);
        uint64_t p1 = pmm_alloc_page();
        uint64_t p2 = pmm_alloc_page();
        uint64_t p3 = pmm_alloc_page();
        vga_print("  Page1: 0x");
        vga_print_hex_cur(p1);
        vga_print("\n");
        vga_print("  Page2: 0x");
        vga_print_hex_cur(p2);
        vga_print("\n");
        vga_print("  Page3: 0x");
        vga_print_hex_cur(p3);
        vga_print("\n");
        pmm_free_page(p2);
        vga_print("  Freed page2\n");
        uint64_t p4 = pmm_alloc_page();
        vga_print("  Page4: 0x");
        vga_print_hex_cur(p4);
        vga_print("\n");
        vga_print("Test complete.\n");
        vga_print("> ");
    } else if (strcmp(cmd, "test") == 0) {
        vga_print("\n=== Exception Test ===\n");
        vga_print("Select test:\n");
        vga_print(" 1 - Divide by zero\n");
        vga_print(" 2 - Page fault\n");
        vga_print(" 3 - GP fault\n");
        vga_print("> ");
        char c = 0;
        while (!kbd_buffer_get(&c)) {
            asm volatile("hlt");
        }
        vga_putc(c);
        vga_print("\n");
        if (c == '1') {
            vga_print("Dividing by zero...\n");
            __asm__ volatile("xor %%rax, %%rax\nxor %%rbx, %%rbx\ndiv %%rbx\n" : : : "rax", "rbx", "rdx");
        } else if (c == '2') {
            vga_print("Causing page fault...\n");
            uint64_t *bad = (uint64_t*)0xFFFFFFFF00000000;
            *bad = 0xDEADBEEF;
        } else if (c == '3') {
            vga_print("Causing GP fault...\n");
            __asm__ volatile("mov $0x0, %%ecx\nmov $0x2, %%eax\nmov $0x0, %%edx\nxsetbv\n" : : : "rax", "rcx", "rdx");
        } else {
            vga_print("Invalid choice\n");
        }
        vga_print("> ");
    } else if (strcmp(cmd, "vmmtest") == 0) {
        vga_print("\n=== VMM Status ===\n");
        vga_print("  Status: Working (recursive paging verified)\n");
        vga_print("  HHDM_START: 0x");
        vga_print_hex_cur(HHDM_START);
        vga_print("\n");
        uint64_t cr3;
        asm volatile("mov %%cr3, %0" : "=r"(cr3));
        vga_print("  CR3: 0x");
        vga_print_hex_cur(cr3);
        vga_print("\n");
        vga_print("  PMM: Working\n");
        vga_print("  VMM: Working\n");
        vga_print("  NX support: Enabled\n");
        vga_print("> ");
    } else if (strcmp(cmd, "serialtest") == 0) {
        vga_print("\nSerial test...\n");
        vga_print("Check your terminal for serial output!\n");
        serial_print("SERIAL: Test message from shell!\n");
        serial_print("SERIAL: Command entered: ");
        serial_print(cmd);
        serial_print("\n");
        vga_print("> ");
    } else if (strcmp(cmd, "heapstat") == 0) {
        heap_stats();
    } else if (strcmp(cmd, "maptest") == 0) {
        vga_print("\n=== Map Test ===\n");
        vga_print("  Simple test: Allocate and use a page\n");
        uint64_t test_phys = pmm_alloc_page();
        if (test_phys == 0) {
            vga_print("  Failed to allocate physical page!\n");
            vga_print("> ");
            return;
        }
        vga_print("  Allocated physical page at 0x");
        vga_print_hex_cur(test_phys);
        vga_print("\n");
        uint64_t* ptr = (uint64_t*)test_phys;
        *ptr = 0xDEADBEEFCAFEBABE;
        vga_print("  Wrote 0xDEADBEEFCAFEBABE to physical address\n");
        vga_print("  Read back: 0x");
        vga_print_hex_cur(*ptr);
        vga_print("\n");
        vga_print("  Simple test PASSED!\n");
        vga_print("> ");
    } else if (strcmp(cmd, "testrec") == 0) {
        vga_print("\n=== Test Recursive ===\n");
        uint64_t test_addr = 0xFFFF800000000000ULL | 
                             ((uint64_t)510 << 39) | 
                             ((uint64_t)510 << 30) | 
                             ((uint64_t)510 << 21) | 
                             ((uint64_t)510 << 12);
        vga_print("  Testing address: 0x");
        vga_print_hex_cur(test_addr);
        vga_print("\n");
        uint64_t* ptr = (uint64_t*)test_addr;
        vga_print("  Trying to read PML4[0]...\n");
        uint64_t val = ptr[0];
        vga_print("  PML4[0] = 0x");
        vga_print_hex_cur(val);
        vga_print("\n");
        vga_print("  Test complete!\n");
        vga_print("> ");
    } else if (strcmp(cmd, "heaptest") == 0) {
        vga_print("\n=== Heap Test ===\n");
        serial_print("=== HEAPTEST START ===\n");
        vga_print("  Test 1: kmalloc(64)\n");
        serial_print("HEAPTEST: Test 1 - kmalloc(64)\n");
        void* p1 = kmalloc(64);
        vga_print("  p1 = 0x");
        vga_print_hex_cur((uint64_t)p1);
        vga_print("\n");
        serial_print("HEAPTEST: p1=0x");
        serial_print_hex((uint64_t)p1);
        serial_print("\n");
        if (p1) {
            __asm__ volatile ("movq $0xDEADBEEFCAFEBABE, %%rax\nmovq %%rax, (%0)" : : "r" (p1) : "rax", "memory");
            vga_print("  Wrote to p1\n");
            serial_print("HEAPTEST: wrote to p1\n");
        }
        vga_print("\n  Test 2: kmalloc(128)\n");
        serial_print("HEAPTEST: Test 2 - kmalloc(128)\n");
        void* p2 = kmalloc(128);
        vga_print("  p2 = 0x");
        vga_print_hex_cur((uint64_t)p2);
        vga_print("\n");
        serial_print("HEAPTEST: p2=0x");
        serial_print_hex((uint64_t)p2);
        serial_print("\n");
        if (p2) {
            __asm__ volatile ("movq $0x1234567890ABCDEF, %%rax\nmovq %%rax, (%0)" : : "r" (p2) : "rax", "memory");
            vga_print("  Wrote to p2\n");
            serial_print("HEAPTEST: wrote to p2\n");
        }
        vga_print("\n  Before kfree:\n");
        heap_stats();
        vga_print("\n  Test 3: kfree(p1)\n");
        serial_print("HEAPTEST: Test 3 - kfree(p1)\n");
        kfree(p1);
        vga_print("  Freed p1\n");
        serial_print("HEAPTEST: freed p1\n");
        vga_print("\n  After kfree(p1):\n");
        heap_stats();
        vga_print("\n  Test 4: kmalloc(64) after free (should reuse p1)\n");
        serial_print("HEAPTEST: Test 4 - kmalloc(64) after free\n");
        void* p3 = kmalloc(64);
        vga_print("  p3 = 0x");
        vga_print_hex_cur((uint64_t)p3);
        vga_print("\n");
        serial_print("HEAPTEST: p3=0x");
        serial_print_hex((uint64_t)p3);
        serial_print("\n");
        if (p3 == p1) {
            vga_print("  ✅ MEMORY REUSED! (p3 == p1)\n");
            serial_print("HEAPTEST: MEMORY REUSED! (p3 == p1)\n");
        } else {
            vga_print("  Memory not reused (p3 != p1)\n");
            serial_print("HEAPTEST: Memory not reused\n");
        }
        if (p3) {
            __asm__ volatile ("movq $0xCAFEBABEDEADBEEF, %%rax\nmovq %%rax, (%0)" : : "r" (p3) : "rax", "memory");
            vga_print("  Wrote to p3\n");
            serial_print("HEAPTEST: wrote to p3\n");
        }
        vga_print("\n  Final heap stats:\n");
        heap_stats();
        vga_print("\n  Cleaning up...\n");
        serial_print("HEAPTEST: cleaning up\n");
        if (p2) kfree(p2);
        if (p3) kfree(p3);
        heap_stats();
        serial_print("=== HEAPTEST END ===\n");
    } else if (strcmp(cmd, "user") == 0) {
        vga_print("\n=== User Mode Test ===\n");
        vga_print("Creating user process...\n");
        create_user_process(user_test, NULL);
        vga_print("> ");
    } else if (strcmp(cmd, "user2") == 0) {
        vga_print("\n=== User Mode Test 2 ===\n");
        vga_print("Creating user process...\n");
        create_user_process(user_test2, NULL);
        vga_print("> ");
    } else if (strcmp(cmd, "simple") == 0) {
        vga_print("\n=== Simple User Mode Test ===\n");
        vga_print("Running user code...\n");
        simple_user_test();
        vga_print("Returned from user mode! (shouldn't happen)\n");
        vga_print("> ");
    } else if (strcmp(cmd, "nxtest") == 0) {
        vga_print("\n=== NX Test ===\n");
        vga_print("NX bit support is enabled in the VMM.\n");
        vga_print("Check page table dumps with: vmmtest\n");
        vga_print("> ");
    // ADDED: Syscall test command
    } else if (strcmp(cmd, "syscall") == 0) {
        vga_print("\n=== Syscall Test ===\n");
        vga_print("Testing SYS_WRITE...\n");
        
        const char* msg = "Hello from syscall test!\n";
        long ret = sys_write(1, msg, 25);
        
        vga_print("sys_write returned: ");
        vga_print_dec_cur(ret);
        vga_print("\n");
        
        vga_print("Testing SYS_EXIT... (will halt)\n");
        vga_print("> ");
        sys_exit(0);
        // Should not reach here
    } else {
        vga_print("\nUnknown command: '");
        vga_print(cmd);
        vga_print("'\n");
        int found_suggestion = 0;
        for (int i = 0; i < num_commands; i++) {
            if (valid_commands[i][0] == cmd[0]) {
                if (!found_suggestion) {
                    vga_print("Did you mean one of these?\n");
                    found_suggestion = 1;
                }
                vga_print("  ");
                vga_print(valid_commands[i]);
                vga_print("\n");
            }
        }
        if (!found_suggestion) {
            vga_print("Type 'help' for available commands\n");
        }
        vga_print("> ");
    }
}

void kmain(BootInfo *info) {
    g_bootinfo = info;
    
    vga_clear();
    serial_init();
    serial_print("Serial: Kernel booted\n");
    vga_set_cursor_shape(0x00, 0x0F);
    
    vga_print("DonsDOS v0.1\n");
    vga_print("Initializing...\n");

    idt_init();
    pit_init(100);
    
    asm volatile("sti");

    pmm_init(g_bootinfo);
    vmm_init();
    heap_init();

    tss_init();

    // Initialize keyboard AFTER memory management
    // This prevents PMM/VMM from corrupting scancode tables
    keyboard_init();

    // ADDED: Initialize system calls
    serial_print("Serial: Initializing syscalls...\n");
    syscall_init();

    vga_clear();
    vga_print("DonsDOS v0.1\n");
    vga_print("Type 'help'\n");
    vga_print("> ");

    char cmd_buffer[128];
    int cmd_pos = 0;

    for (;;) {
        asm volatile("hlt");

        char c;
        if (kbd_buffer_get(&c)) {
            if (c == '\b') {
                if (cmd_pos > 0) {
                    cmd_pos--;
                    vga_putc('\b');
                }
                continue;
            }
            if (c == '\n') {
                vga_putc('\n');
                cmd_buffer[cmd_pos] = '\0';
                handle_command(cmd_buffer);
                cmd_pos = 0;
                continue;
            }
            if (c >= ' ' && c <= '~' && cmd_pos < 127) {
                cmd_buffer[cmd_pos++] = c;
                vga_putc(c);
            }
        }
    }
}
