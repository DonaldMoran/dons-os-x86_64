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
#include "include/syscall.h"
#include "include/user_syscall.h"
#include "include/elf.h"
#include "include/gdt.h"
#include "include/debug.h"
#include "include/user_msr.h"

extern void pit_init(uint32_t freq);
// Embedded ELF test program (from test_program.bin)
extern unsigned char test_program[];
extern unsigned int test_program_len;
static BootInfo *g_bootinfo = NULL;

void dump_iretq_frame_serial(void) {
    uint64_t* rsp;
    uint64_t frame_rsp;
    
    // Get the frame stack pointer (original rsp before the call)
    __asm__ volatile (
        "mov %%rsp, %0\n"
        "add $8, %0\n"  // Skip the return address that was pushed
        : "=r"(frame_rsp)
        : 
        : "memory"
    );
    
    rsp = (uint64_t*)frame_rsp;
    
    serial_print("\n=== IRETQ FRAME DUMP (before iretq) ===\n");
    
    serial_print("SS     : 0x");
    serial_print_hex(rsp[5]);  // rsp+40
    serial_print("\n");
    
    serial_print("RSP    : 0x");
    serial_print_hex(rsp[4]);  // rsp+32
    serial_print("\n");
    
    serial_print("RFLAGS : 0x");
    serial_print_hex(rsp[3]);  // rsp+24
    serial_print("\n");
    
    serial_print("CS     : 0x");
    serial_print_hex(rsp[2]);  // rsp+16
    serial_print("\n");
    
    serial_print("RIP    : 0x");
    serial_print_hex(rsp[1]);  // rsp+8
    serial_print("\n");
    
    serial_print("=== END FRAME DUMP ===\n\n");
}

extern void user_syscall_entry(void);

void user_syscall_init(void)
{
    serial_print("Initializing **RING** 3 syscalls...\n");

    // U = 0x20 → SYSRET CS = 0x30 (user code), SS = 0x28 (user data)
    uint64_t star =
        ((uint64_t)0x20 << 48) |   // user CS base
        ((uint64_t)0x18 << 32);    // kernel 64-bit code (GDT_CODE64)

    wrmsr(MSR_STAR, star);
    wrmsr(MSR_LSTAR, (uint64_t)user_syscall_entry);
    wrmsr(MSR_FMASK, (1ULL << 9)); // mask IF

    serial_print("SYSCALL init done\n");

    uint64_t lstar = rdmsr(MSR_LSTAR);
    serial_print("LSTAR = 0x");
    serial_print_hex(lstar);
    serial_print("\n");
}

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
        "nxtest", "syscall", "elfload", "segtest"
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
        vga_print("  simple   - Test user mode (Ring 3) - simple test\n");
        vga_print("  nxtest   - Test NX (No Execute) bit\n");
        vga_print("  syscall  - Test system calls\n");
        vga_print("  elfload  - Load and run ELF program\n");
        vga_print("> ");
    } else if (strcmp(cmd, "clear") == 0) {
        vga_clear();
        vga_print("DonsDOS v0.4.0\n");
        vga_print("Type 'help'\n");
        vga_print("> ");
    } else if (strcmp(cmd, "version") == 0) {
        vga_print("\nDonsDOS v0.4.0\n");
        vga_print("Build: 64-bit kernel with VGA console\n");
        vga_print("Features: VMM with recursive paging, HHDM, NX support, Syscalls, ELF loader\n");
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
            serial_print("\nRebooting...\n");
            vga_print("\nWill likely hang in QEMU...\n");
            serial_print("\nWill likely hang in QEMU...\n");
            __asm__ volatile (
                "cli\n"
                "mov $0x0F, %%al\n"
                "outb %%al, $0x70\n"
                "mov $0x00, %%al\n"
                "outb %%al, $0x71\n"
                "hlt\n"
                : : : "memory", "al"
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
        //~ if (p1) {
            //~ serial_print("HEAPTEST: About to write to p1=0x");
            //~ serial_print_hex((uint64_t)p1);
            //~ serial_print("\n");
            //~ __asm__ volatile ("movq $0xDEADBEEFCAFEBABE, %%rax\nmovq %%rax, (%0)" : : "r" (p1) : "rax", "memory");
            //~ vga_print("  Wrote to p1\n");
            //~ serial_print("HEAPTEST: wrote to p1\n");
        //~ }
        
        if (p1) {
            serial_print("HEAPTEST: About to write to p1=0x");
            serial_print_hex((uint64_t)p1);
            serial_print("\n");
            
            // Try writing with a simple C assignment
            // This will cause a page fault if the page isn't mapped/writable
            
            serial_print("Page fault will happen on next line!\n");
            serial_print("\n");
            uint64_t* test_ptr = (uint64_t*)p1;
            
            
            
            uint32_t pml4_idx = ((uint64_t)p1 >> 39) & 0x1FF;
            uint32_t pdpt_idx = ((uint64_t)p1 >> 30) & 0x1FF;
            uint32_t pd_idx = ((uint64_t)p1 >> 21) & 0x1FF;
            uint32_t pt_idx = ((uint64_t)p1 >> 12) & 0x1FF;
            
            uint64_t* pte_ptr = (uint64_t*)(0xFFFFFF7FBFDFE000ULL | 
                                             ((uint64_t)pml4_idx << 12) |
                                             ((uint64_t)pdpt_idx << 12) |
                                             ((uint64_t)pd_idx << 12) |
                                             ((uint64_t)pt_idx << 3));
            serial_print("HEAP: PTE for 0x");
            serial_print_hex((uint64_t)p1);
            serial_print(" = 0x");
            serial_print_hex(*pte_ptr);
            serial_print("\n");
            serial_print("HEAP: PTE flags: Present=");
            serial_print_dec(*pte_ptr & 0x1);
            serial_print(" Write=");
            serial_print_dec((*pte_ptr >> 1) & 0x1);
            serial_print(" User=");
            serial_print_dec((*pte_ptr >> 2) & 0x1);
            serial_print(" Exec=");
            serial_print_dec((*pte_ptr >> 3) & 0x1);
            serial_print("\n");
            
            serial_print("Point A");
            serial_print("\n");
            *test_ptr = 0xDEADBEEFCAFEBABE;
            serial_print("Point B");
            serial_print("\n");
            
            // If we get here, it worked!
            serial_print("HEAPTEST: Write succeeded via C assignment!\n");
            vga_print("  Wrote to p1\n");
            
            // Now try the inline assembly
            __asm__ volatile ("movq $0xDEADBEEFCAFEBABE, %%rax\nmovq %%rax, (%0)" : : "r" (p1) : "rax", "memory");
            vga_print("  Wrote to p1 via inline asm\n");
            serial_print("HEAPTEST: wrote to p1 via inline asm\n");
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
        
    } else if (strcmp(cmd, "elfload") == 0) {
        vga_print("\n=== ELF Load Test ===\n");
        vga_print("Loading embedded ELF program...\n");
        
        // Check if test program exists
        if (test_program_len == 0) {
            vga_print("No ELF program embedded!\n");
            vga_print("> ");
            return;
        }
        vga_print("ELF size: ");
        vga_print_dec_cur(test_program_len);
        vga_print(" bytes\n");
        
        // Print first few bytes to verify it's an ELF
        vga_print("ELF magic: ");
        for (int i = 0; i < 4; i++) {
            vga_print_hex_cur(test_program[i]);
            vga_print(" ");
        }
        vga_print("\n");
        
        // Load and run the ELF
        extern void elf_load(const void* elf_data);
        debug_rsp("kmain before elf_load");
        elf_load(test_program);
        debug_rsp("kmain after elf_load");
        vga_print("> ");
    }  else {
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

void kmain_shell_loop(void) {
    vga_print("DonsDOS v0.4.0\n");
    serial_print("DonsDOS v0.4.0\n");
    
    vga_print("Type 'help'\n");
    serial_print("Type 'help'\n");
    
    vga_print("> ");
    serial_print("> ");

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


void kmain(BootInfo *info) {
    g_bootinfo = info;
    vga_clear();
    
    serial_print("serial_init\n");
    serial_init();
    
    //serial_print("Serial: Kernel booted\n");
    vga_set_cursor_shape(0x00, 0x0F);
    vga_print("DonsDOS v0.4.0\n");
    vga_print("Initializing...\n");

    serial_print("idt_init\n");
    idt_init();
    
    serial_print("pit_init\n");
    pit_init(100);
    
    asm volatile("sti");
    
    serial_print("pm_init\n");
    pmm_init(g_bootinfo);
    
    serial_print("vmm_init\n");
    vmm_init();
    
    serial_print("heap_init\n");
    heap_init();

    // FIX: Patch user segments to be 64-bit before TSS init
    gdt_fix_user_segments(); 
    gdt_debug_print();
    struct {
    uint16_t limit;
    uint64_t base __attribute__((packed));
    } gdt_ptr;
    __asm__ volatile ("sgdt %0" : "=m"(gdt_ptr));
    //uint64_t *gdt = (uint64_t *)gdt_ptr.base;   
    serial_print("=== GDT FOCUSED DUMP ===\n");
    serial_print("Kernel code (0x08): 0x");
    serial_print_hex(((uint64_t*)gdt_ptr.base)[1]);
    serial_print("\n");
    serial_print("Kernel data (0x10): 0x");
    serial_print_hex(((uint64_t*)gdt_ptr.base)[2]);
    serial_print("\n");
    serial_print("User code   (0x28): 0x");
    serial_print_hex(((uint64_t*)gdt_ptr.base)[5]);
    serial_print("\n");
    serial_print("User data   (0x30): 0x");
    serial_print_hex(((uint64_t*)gdt_ptr.base)[6]);
    serial_print("\n========================\n");
    serial_print("\n");

    //serial_print("kmain: about to debug GDT before TS_INIT\n");
    //gdt_debug_print();
    
    serial_print("tss_init\n");
    tss_init();
    
    // Test I/O permission after TSS init
    serial_print("Testing I/O permission...\n");
    uint8_t tmp;
    __asm__ volatile (
        "inb $0x64, %%al\n"
        : "=a"(tmp)
    );
    serial_print("I/O port 0x64 readable, value=0x");
    serial_print_hex(tmp);
    serial_print("\n");

    // Initialize keyboard AFTER memory management
    keyboard_init();

    // Initialize ring 0 system calls
    serial_print("Initializing **RING** 0 syscalls...\n");
    syscall_init();
    serial_print("RING 0 Done.\n");
    
    // Initialize ring 3 user system calls
    serial_print("Initializing **RING** 3 syscalls...\n");
    user_syscall_init();
    uint64_t star = rdmsr(MSR_STAR);
    serial_print("STAR = 0x");
    serial_print_hex(star);
    serial_print("\n");

    serial_print("RING 3 Done.\n");
    
    uint64_t lstar = rdmsr(MSR_LSTAR);
    serial_print("LSTAR = 0x");
    serial_print_hex(lstar);
    serial_print("\n");

    serial_print("Done.\n");
    vga_clear();
    
    kmain_shell_loop();

}
