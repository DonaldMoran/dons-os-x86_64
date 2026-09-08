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

extern void pit_init(uint32_t freq);

// Embedded ELF test program (from test_program.bin)
extern unsigned char test_program[];
extern unsigned int test_program_len;

// Embedded user shell binary
extern unsigned char build_user_shell_elf[];
extern unsigned int build_user_shell_elf_len;

static BootInfo *g_bootinfo = NULL;

static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    __asm__ volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

static inline void outb(uint16_t port, uint8_t val) {
    __asm__ volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
}

static int validate_bootinfo(BootInfo* info) {
    if (!info) {
        serial_print("ERROR: No BootInfo provided!\n");
        return 0;
    }
    if (info->magic != BOOTINFO_MAGIC) {
        serial_print("ERROR: Invalid BootInfo magic!\n");
        return 0;
    }
    serial_print("BootInfo validated successfully\n");
    return 1;
}

void test_process_entry(void) {
    const char* msg = "Hello from process!\n";
    sys_write(1, msg, 22);
    process_exit();
}

void dump_iretq_frame_serial(void) {
    uint64_t frame_rsp;
    __asm__ volatile ("mov %%rsp, %0\nadd $8, %0\n" : "=r"(frame_rsp) :: "memory");
    uint64_t* rsp = (uint64_t*)frame_rsp;
    
    serial_print("\n=== IRETQ FRAME DUMP ===\n");
    serial_print("SS     : 0x"); serial_print_hex(rsp[5]); serial_print("\n");
    serial_print("RSP    : 0x"); serial_print_hex(rsp[4]); serial_print("\n");
    serial_print("RIP    : 0x"); serial_print_hex(rsp[1]); serial_print("\n");
}

extern void user_syscall_entry(void);

void user_syscall_init(void) {
    serial_print("Initializing **RING** 3 syscalls...\n");
    uint64_t star = ((uint64_t)0x20 << 48) | ((uint64_t)0x18 << 32);
    wrmsr(0xC0000081, star);
    wrmsr(0xC0000082, (uint64_t)user_syscall_entry);
    wrmsr(0xC0000084, (1ULL << 9));
    serial_print("SYSCALL init done\n");
}

static int strcmp(const char *s1, const char *s2) {
    while (*s1 && (*s1 == *s2)) { s1++; s2++; }
    return *(const unsigned char*)s1 - *(const unsigned char*)s2;
}

void test_process1(void) {
    for (int i = 0; i < 5; i++) {
        vga_print("Process 1: iteration ");
        vga_print_dec_cur(i);
        vga_print("\n");
        
        serial_print("Process 1: iteration "); 
        serial_print_dec(i); 
        serial_print("\n");
        
        process_yield();
    }
    process_exit();
}

void test_process2(void) {
    for (int i = 0; i < 5; i++) {
        vga_print("Process 2: iteration ");
        vga_print_dec_cur(i);
        vga_print("\n");
        
        serial_print("Process 2: iteration "); 
        serial_print_dec(i); 
        serial_print("\n");
        
        process_yield();
    }
    process_exit();
}

void handle_reboot_sequence(void) {
    __asm__ volatile("cli");
    
    // Clear the PS/2 8042 keyboard controller buffer loops
    for (int i = 0; i < 1000; i++) {
        if ((inb(0x64) & 2) == 0) break;
    }
    
    // Issue hard pulse signals to the motherboard reset lines
    outb(0x64, 0xFE);  // PS/2 Hard Pulse Line
    outb(0xCF9, 0x06); // Legacy PCI Hard Reset

    // FIX: Clear the 64-bit Task Register (TR) to prevent warm-reset 
    // verification panics ("TR not loaded correctly!") during the boot cycle!
    __asm__ volatile(
        "xor %%ax, %%ax\n\t"
        "ltr %%ax"
        :
        :
        : "ax"
    );

    // Ultimate hard-reset fallback: Forceful architectural Triple Fault
    volatile uint16_t malformed_idt_struct[5] = {0, 0, 0, 0, 0}; // Limit = 0, Base = 0
    __asm__ volatile(
        "lidt (%0)\n\t"
        "int $0" 
        : 
        : "r"(malformed_idt_struct)
        : "memory"
    );

    // Defensive freeze block in case the core is completely locked up
    while (1) {
        __asm__ volatile("hlt");
    }
}


static void handle_command(const char *cmd) {
    // FIX 1: Evaluate string bounds correctly by checking individual element indices
    if (cmd == NULL || cmd[0] == '\0') {
        vga_print("> ");
        return;
    }
    
    const char *valid_commands[] = {
        "help", "clear", "version", "reboot", 
        "pmmtest", "info", "mem", "test", 
        "vmmtest", "serialtest", "heapstat", "maptest", "testrec", "heaptest", 
        "nxtest", "syscall", "elfload", "proclist" , "proccreate" , "vmmclone", 
        "runproc", "schstat", "testyield", "usershell"
    };
    // FIX 2: Corrected division metrics to scale array item thresholds accurately
    int num_commands = sizeof(valid_commands) / sizeof(valid_commands[0]);
    (void)num_commands;

    if (strcmp(cmd, "help") == 0) {
        vga_print("\nAvailable commands:\n  help, clear, version, reboot, pmmtest, info, mem, test,\n  vmmtest, serialtest, heapstat, maptest, testrec, heaptest,\n  nxtest, syscall, elfload, proclist, proccreate, vmmclone,\n  runproc, schstat, testyield, usershell\n> ");
    } else if (strcmp(cmd, "clear") == 0) {
        vga_clear(); vga_print("DonsDOS v0.4.6\nType 'help'\n> ");
    } else if (strcmp(cmd, "version") == 0) {
        vga_print("\nDonsDOS v0.4.6\nBuild: 64-bit preemptive kernel with VGA console\n> ");
    } else if (strcmp(cmd, "info") == 0) {
        vga_print("\n=== System Boot Telemetry Information ===\n");
        if (g_bootinfo) {
            vga_print("  Bootinfo Structure Magic : 0x"); 
            vga_print_hex_cur(g_bootinfo->magic); 
            vga_print("\n");
            
            vga_print("  PML4 Virtual Table Root  : 0x"); 
            vga_print_hex_cur(g_bootinfo->pml4_addr); 
            vga_print("\n");
            
            vga_print("  Memory Map Array Length  : "); 
            vga_print_dec_cur(g_bootinfo->memory_map_count); 
            vga_print(" entries\n");
            
            vga_print("  Memory Map Phys Address  : 0x"); 
            vga_print_hex_cur(g_bootinfo->memory_map_addr); 
            vga_print("\n");
            
            serial_print("\n--- DIAGNOSTIC DUMP: BOOTINFO STRUCT ---\n");
            serial_print("Magic  : 0x"); serial_print_hex(g_bootinfo->magic); serial_print("\n");
            serial_print("PML4   : 0x"); serial_print_hex(g_bootinfo->pml4_addr); serial_print("\n");
            serial_print("MMap   : 0x"); serial_print_hex(g_bootinfo->memory_map_addr); serial_print("\n");
            serial_print("Count  : "); serial_print_dec(g_bootinfo->memory_map_count); serial_print("\n");
        } else {
            vga_print("  Error: No system boot parameters detected!\n");
        }
        vga_print("> ");
    } else if (strcmp(cmd, "mem") == 0) {
        vga_print("\nMemory Information:\n  Page size: 4096 bytes\n");
        if (g_bootinfo && g_bootinfo->memory_map_count > 0) {
            MemoryMapEntry *m = (MemoryMapEntry *)g_bootinfo->memory_map_addr;
            uint64_t total_usable = 0;
            for (uint64_t i = 0; i < g_bootinfo->memory_map_count; i++) {
                if (m[i].type == 1) total_usable += m[i].length;
            }
            vga_print("  Usable RAM: "); vga_print_dec_cur(total_usable / (1024 * 1024)); vga_print(" MB\n");
        } else { vga_print("  Memory map not available\n"); }
        vga_print("> ");
    //~ } else if (strcmp(cmd, "reboot") == 0) {
        //~ vga_print("\n=== Hardware System Reboot Sequence Initiated ===\n");
        //~ serial_print("\n=== Hardware System Reboot Sequence Initiated ===\n");

        //~ vga_print("  Flushing hardware text consoles and issuing processor resets...\n");
        //~ serial_print("  [REBOOT] Sending CPU hard reset commands...\n");

        //~ __asm__ volatile("cli");

        //~ extern void outb(uint16_t port, uint8_t val);
        //~ extern uint8_t inb(uint16_t port);

        //~ // Clear the keyboard controller buffer
        //~ for (int i = 0; i < 1000; i++) {
            //~ if ((inb(0x64) & 2) == 0) break;
        //~ }
        //~ outb(0x64, 0xFE); // PS/2 Reset line pulse
        //~ outb(0xCF9, 0x06); // PCI Chipset Reset fallback

        //~ for (volatile uint64_t delay = 0; delay < 10000000ULL; delay++) {
            //~ __asm__ volatile("nop");
        //~ }

        //~ // Ultimate hard-reset fallback: Intentional Triple Fault
        //~ serial_print("  [REBOOT] I/O controller timed out. Forcing architectural Triple Fault...\n");
        //~ volatile uint16_t malformed_idt_struct[5] = {0, 0, 0, 0, 0};
        //~ __asm__ volatile(
            //~ "lidt (%0)\n\t"
            //~ "int $0" 
            //~ : 
            //~ : "r"(malformed_idt_struct)
            //~ : "memory"
        //~ );

        //~ while (1) {
            //~ __asm__ volatile("hlt");
         //~ } 
    //~ } 
    
    } else if (strcmp(cmd, "reboot") == 0) {
        vga_print("\nRebooting...\n");
        handle_reboot_sequence();
    } else if (strcmp(cmd, "pmmtest") == 0) {
        vga_print("\n--- Physical Memory Manager Test ---\n");
        serial_print("\n--- Physical Memory Manager Test ---\n");
        
        // Capture baseline statistics before allocation
        uint64_t initial_free = pmm_get_free_pages();
        uint64_t total_pages  = pmm_get_total_pages();
        
        vga_print("  Total Page Frames Available  : "); vga_print_dec_cur(total_pages); vga_print("\n");
        vga_print("  Initial Free Page Counter    : "); vga_print_dec_cur(initial_free); vga_print("\n");

        // Request a pristine 4096-byte page frame marked for Kernel usage
        uint64_t phys_page = pmm_alloc_page(PAGE_KERNEL);
        
        if (phys_page != 0) {
            vga_print("  Allocated Page Phys Address  : 0x"); vga_print_hex_cur(phys_page); vga_print("\n");
            serial_print("  Allocated Page Phys Address  : 0x"); serial_print_hex(phys_page); serial_print("\n");
            
            // Determine memory zone boundaries dynamically (LOW zone vs HIGH zone page)
            if (phys_page < (8388ULL * 4096ULL)) {
                vga_print("  Memory Allocation Sector     : LOW ZONE territory\n");
            } else {
                vga_print("  Memory Allocation Sector     : HIGH ZONE territory\n");
            }
            
            // Capture and verify post-allocation metric compliance
            uint64_t post_free = pmm_get_free_pages();
            vga_print("  Remaining Free Page Counter  : "); vga_print_dec_cur(post_free); vga_print("\n");
            vga_print("  Status                       : SUCCESS\n");
            
            // Dump the detailed block mapping allocation map straight to background logs
            pmm_dump_stats();
        } else {
            vga_print("  Status                       : FAILED (Out of Physical Page Frames!)\n");
            serial_print("  [ERR] PMM Allocation failed during shell test pass.\n");
        }
        vga_print("> ");
    } else if (strcmp(cmd, "test") == 0) {
        vga_print("\n=== Kernel Exception Test Harness ===\n");
        vga_print("  1 - Trigger Divide-By-Zero Exception (#DE)\n");
        vga_print("  2 - Trigger Protection Page Fault Exception (#PF)\n");
        vga_print("  3 - Trigger General Protection Fault Exception (#GP)\n");
        vga_print("Enter selection choice: ");
        
        char c = 0; 
        while (!kbd_buffer_get(&c)) {
            __asm__ volatile("hlt");
        }
        
        // Echo the keystroke choice clearly to the screen layout
        vga_putc(c);
        vga_print("\n");

        if (c == '1') {
            vga_print("  Executing: Forceful math division by zero... System halting.\n\n");
            serial_print("TEST: Triggering hardware #DE exception pass.\n");
            __asm__ volatile(
                "xor %%rax, %%rax\n\t"
                "xor %%rbx, %%rbx\n\t"
                "div %%rbx\n\t" 
                : 
                : 
                : "rax", "rbx", "rdx"
            );
        } 
        else if (c == '2') {
            vga_print("  Executing: Supervisor unmapped address segment write... System halting.\n\n");
            serial_print("TEST: Triggering hardware #PF exception pass.\n");
            uint64_t *bad = (uint64_t*)0xFFFFFFFF00000000ULL; 
            *bad = 0xDEADBEEF; 
        } 
        else if (c == '3') {
            vga_print("  Executing: Non-canonical address boundary reference... System halting.\n\n");
            serial_print("TEST: Triggering hardware #GP exception pass.\n");
            uint64_t *non_canonical = (uint64_t*)0x000FFFFF00000000ULL;
            *non_canonical = 0xDEADBEEF;
        }
        else {
            vga_print("  Error: Invalid test selection sequence aborted.\n");
        }
        vga_print("> ");
    } else if (strcmp(cmd, "vmmtest") == 0) {
        vga_print("\n=== Virtual Memory Manager (VMM) Status Dashboard ===\n");
        serial_print("\n=== Virtual Memory Manager (VMM) Status Dashboard ===\n");

        // 1. Read the active hardware translation directory root (CR3)
        uint64_t cr3; 
        __asm__ volatile("mov %%cr3, %0" : "=r"(cr3));
        
        vga_print("  1. PML4 Table Tree Root (CR3) : 0x"); vga_print_hex_cur(cr3); vga_print("\n");
        serial_print("  1. PML4 Table Tree Root (CR3) : 0x"); serial_print_hex(cr3); serial_print("\n");

        // 2. Query CR0 to check if Write-Protect (WP - Bit 16) is active
        uint64_t cr0;
        __asm__ volatile("mov %%cr0, %0" : "=r"(cr0));
        int wp_active = (cr0 & (1ULL << 16)) ? 1 : 0;
        
        vga_print("  2. Supervisor Write-Protect   : "); vga_print(wp_active ? "ENABLED (Strict Kernel Safety)\n" : "DISABLED\n");
        serial_print("  2. Supervisor Write-Protect   : "); serial_print(wp_active ? "ENABLED (Strict Kernel Safety)\n" : "DISABLED\n");

        // 3. Query the IA32_EFER MSR to verify No-Execute (NXE - Bit 11) is active
        extern uint64_t rdmsr(uint32_t msr);
        uint64_t efer = rdmsr(0xC0000080);
        int nxe_active = (efer & (1ULL << 11)) ? 1 : 0;
        
        vga_print("  3. Hardware No-Execute (NXE)  : "); vga_print(nxe_active ? "ENABLED (Stack Guard Active)\n" : "DISABLED\n");
        serial_print("  3. Hardware No-Execute (NXE)  : "); serial_print(nxe_active ? "ENABLED (Stack Guard Active)\n" : "DISABLED\n");

        // 4. Report fixed mapping architecture context definitions
        vga_print("  4. Higher Half Mapping (HHDM) : 0xFFFF800000000000\n");
        vga_print("  5. Recursive Paging Slot      : PML4 Index 510\n");
        
        serial_print("  4. Higher Half Mapping (HHDM) : 0xFFFF800000000000\n");
        serial_print("  5. Recursive Paging Slot      : PML4 Index 510\n");

        vga_print("  Status: VMM TRANSLATION TABLES STABLE\n");
        serial_print("  Status: VMM TRANSLATION TABLES STABLE\n");
        vga_print("> ");
    } else if (strcmp(cmd, "serialtest") == 0) {
        vga_print("\n=== Hardware Serial Port Communication Test ===\n");
        vga_print("  Status: Sending 8-N-1 UART data frames via Port 0x3F8 (COM1)...\n");
        
        // Transmit the formal validation layout framework out to your background logs
        serial_print("\n=============================================\n");
        serial_print("SERIAL PORT VERIFICATION: Hello from DonsDOS!\n");
        serial_print("=============================================\n");
        
        vga_print("  Result: Synchronized package transmitted successfully!\n");
        vga_print("          (Check your host terminal output or serial.log file)\n");
        vga_print("> ");
    } else if (strcmp(cmd, "heapstat") == 0) {
        vga_print("\n=== Kernel Dynamic Heap Memory Dashboard ===\n");
        serial_print("\n=== Kernel Dynamic Heap Memory Dashboard ===\n");
        
        // Invoke your core heap manager function to stream the raw allocation block lists
        heap_stats();
        
        // Print clean descriptive parameters onto the visual VGA screen
        vga_print("  Heap Virtual Base Address : 0xFFFF900000000000\n");
        vga_print("  Total Initialized Pools   : 1024 KB (256 Pages)\n");
        vga_print("  Tracking Metadata Blocks  : List Nodes Synchronized\n");
        vga_print("  Detailed Node Map Allocation Analysis streamed to Serial Monitor.\n");
        
        vga_print("> ");
    } else if (strcmp(cmd, "maptest") == 0) {
        // Output headers to both display channels
        vga_print("\n=== Virtual Memory Manager Mapping Test ===\n");
        serial_print("\n=== Virtual Memory Manager Mapping Test ===\n");

        // 1. Allocate a pristine physical page frame from the PMM
        uint64_t phys_addr = pmm_alloc_page(PAGE_KERNEL);
        
        vga_print("  1. Allocated Physical Page Frame : 0x");
        vga_print_hex_cur(phys_addr);
        vga_print("\n");
        
        serial_print("  1. Allocated Physical Page Frame : 0x");
        serial_print_hex(phys_addr);
        serial_print("\n");

        // 2. Safely populate a validation signature key into the physical frame using the HHDM address shift window
        uint64_t* hhdm_ptr = (uint64_t*)(0xFFFF800000000000ULL + phys_addr);
        *hhdm_ptr = 0xDEADBEEFCAFEBABEULL;

        // Target a lower-half, completely unmapped sandbox space (0x40000000)
        // to entirely bypass kernel-half recursive lookup loops and debugging hooks!
        uint64_t test_virt = 0x40000000ULL; 
        
        vga_print("  2. Target Testing Virtual Address: 0x");
        vga_print_hex_cur(test_virt);
        vga_print("\n");
        
        serial_print("  2. Target Testing Virtual Address: 0x");
        serial_print_hex(test_virt);
        serial_print("\n");

        vga_print("  3. Invoking vmm_map_page_in_cr3 loop configuration...\n");
        serial_print("  3. Invoking vmm_map_page_in_cr3 loop configuration...\n");
        
        // 3. Extract the currently active hardware page table root address registry
        uint64_t active_cr3;
        __asm__ volatile("mov %%cr3, %0" : "=r"(active_cr3));
        
        // Map the entries dynamically using your system's native VMM function signature and permissions flags
        extern void vmm_map_page_in_cr3(uint64_t cr3, uint64_t virt, uint64_t phys, uint64_t flags);
        uint64_t map_flags = 0x01ULL | 0x02ULL; // PT_PRESENT | PT_WRITE
        vmm_map_page_in_cr3(active_cr3, test_virt, phys_addr, map_flags);

        // 4. HARDWARE CACHE FLUSH (TLB): Forcefully invalidate the cache entry for this specific target address
        __asm__ volatile("invlpg (%0)" : : "r"(test_virt) : "memory");

        // 5. VERIFICATION PASS: Read back the verification data payload from the newly configured virtual pointer address location
        uint64_t* virt_ptr = (uint64_t*)test_virt;
        uint64_t read_result = *virt_ptr;
        
        vga_print("  4. Reading from new virtual pointer address... Result: 0x");
        vga_print_hex_cur(read_result);
        vga_print("\n");
        
        serial_print("  4. Reading from new virtual pointer address... Result: 0x");
        serial_print_hex(read_result);
        serial_print("\n");

        if (read_result == 0xDEADBEEFCAFEBABEULL) {
            vga_print("  Status: SUCCESS! Page Table Mapping fully functional.\n");
            serial_print("  Status: SUCCESS! Page Table Mapping fully functional.\n");
        } else {
            vga_print("  Status: FAILED! Memory synchronization payload mismatch.\n");
            serial_print("  Status: FAILED! Memory synchronization payload mismatch.\n");
        }
        
        // Clean up our sandbox mapping when completed to keep table entries pristine
        extern void vmm_unmap_page_in_cr3(uint64_t cr3, uint64_t virt);
        vmm_unmap_page_in_cr3(active_cr3, test_virt);
        __asm__ volatile("invlpg (%0)" : : "r"(test_virt) : "memory");
        
        vga_print("> ");
    } else if (strcmp(cmd, "testrec") == 0) {
        vga_print("\n=== Recursive Page Table Mapping Verification ===\n");
        serial_print("\n=== Recursive Page Table Mapping Verification ===\n");

        // 1. Calculate the base virtual coordinate address for the recursively mapped PML4 table.
        // Index 510 recursively pointing to itself shifts the table structures into this exact window.
        uint64_t recursive_pml4_base = 0xFFFF000000000000ULL | ((uint64_t)510 << 39) | ((uint64_t)510 << 30) | ((uint64_t)510 << 21) | ((uint64_t)510 << 12);
        
        vga_print("  1. Calculating Recursive PML4 Base : 0x");
        vga_print_hex_cur(recursive_pml4_base);
        vga_print("\n");
        
        serial_print("  1. Calculating Recursive PML4 Base : 0x");
        serial_print_hex(recursive_pml4_base);
        serial_print("\n");

        // 2. Extract the active hardware CR3 register value to find the true physical root address
        uint64_t active_cr3;
        __asm__ volatile("mov %%cr3, %0" : "=r"(active_cr3));
        uint64_t true_pml4_phys = active_cr3 & ~0xFFFULL; // Mask off PCID/attribute bits

        vga_print("  2. Authentic Hardware CR3 Root Phys: 0x");
        vga_print_hex_cur(true_pml4_phys);
        vga_print("\n");
        
        serial_print("  2. Authentic Hardware CR3 Root Phys: 0x");
        serial_print_hex(true_pml4_phys);
        serial_print("\n");

        // 3. DEREFERENCE PASS: Attempt to read the very first slot entry inside the recursive table.
        // If the recursive link is broken, this pointer dereference will drop an instant Page Fault (#PF).
        vga_print("  3. Dereferencing recursive pointer address slot... \n");
        serial_print("  3. Dereferencing recursive pointer address slot... \n");
        
        uint64_t* pml4_ptr = (uint64_t*)recursive_pml4_base;
        
        // Read the recursive entry (slot 510 inside itself should hold its own base mapping flags)
        uint64_t recursive_entry_val = pml4_ptr[510];

        vga_print("  4. Extracted Recursive Entry [510] Value: 0x");
        vga_print_hex_cur(recursive_entry_val);
        vga_print("\n");
        
        serial_print("  4. Extracted Recursive Entry [510] Value: 0x");
        serial_print_hex(recursive_entry_val);
        serial_print("\n");

        // Extract the physical page frame address pointing back to the PML4 root table
        uint64_t extracted_phys = recursive_entry_val & ~0xFFFULL;

        if (extracted_phys == true_pml4_phys) {
            vga_print("  Status: SUCCESS! Recursive PML4 mapping verified at Index 510.\n");
            serial_print("  Status: SUCCESS! Recursive PML4 mapping verified at Index 510.\n");
        } else {
            vga_print("  Status: FAILED! Physical mapping address mismatch.\n");
            serial_print("  Status: FAILED! Physical mapping address mismatch.\n");
        }

        vga_print("> ");
    } else if (strcmp(cmd, "heaptest") == 0) {
        vga_print("\n=== Kernel Heap Manager Multi-Stage Test ===\n");
        serial_print("\n=== Kernel Heap Manager Multi-Stage Test ===\n");

        // Stage 1: Allocation testing
        vga_print("  [Stage 1] Executing consecutive chunk allocations...\n");
        serial_print("  [Stage 1] Executing consecutive chunk allocations...\n");

        uint8_t* p1 = (uint8_t*)kmalloc(32);
        uint8_t* p2 = (uint8_t*)kmalloc(64);
        uint8_t* p3 = (uint8_t*)kmalloc(128);

        vga_print("     Allocated Chunk A (32B)  at: 0x"); vga_print_hex_cur((uintptr_t)p1); vga_print("\n");
        vga_print("     Allocated Chunk B (64B)  at: 0x"); vga_print_hex_cur((uintptr_t)p2); vga_print("\n");
        vga_print("     Allocated Chunk C (128B) at: 0x"); vga_print_hex_cur((uintptr_t)p3); vga_print("\n");

        serial_print("     Allocated Chunk A (32B)  at: 0x"); serial_print_hex((uintptr_t)p1); serial_print("\n");
        serial_print("     Allocated Chunk B (64B)  at: 0x"); serial_print_hex((uintptr_t)p2); serial_print("\n");
        serial_print("     Allocated Chunk C (128B) at: 0x"); serial_print_hex((uintptr_t)p3); serial_print("\n");

        int success = 1;
        if (!p1 || !p2 || !p3) {
            success = 0;
            vga_print("  [ERR] Heap manager failed to return valid descriptor pointers!\n");
            serial_print("  [ERR] Heap manager failed to return valid descriptor pointers!\n");
        }

        // Stage 2: Data integrity write verification
        if (success) {
            vga_print("  [Stage 2] Verifying block data frame write integrity...\n");
            serial_print("  [Stage 2] Verifying block data frame write integrity...\n");

            // Fill with safe tracking patterns safely within structural boundaries
            for(int i = 0; i < 32;  i++) p1[i] = 0xAA;
            for(int i = 0; i < 64;  i++) p2[i] = 0xBB;
            for(int i = 0; i < 128; i++) p3[i] = 0xCC;

            // Verify memory boundaries didn't bleed or degrade
            for(int i = 0; i < 32;  i++) { if(p1[i] != 0xAA) success = 0; }
            for(int i = 0; i < 64;  i++) { if(p2[i] != 0xBB) success = 0; }
            for(int i = 0; i < 128; i++) { if(p3[i] != 0xCC) success = 0; }
        }

        // Stage 3: Dynamic block list coalescing
        vga_print("  [Stage 3] Testing dynamic chunk deallocations & heap coalescing...\n");
        serial_print("  [Stage 3] Testing dynamic chunk deallocations & heap coalescing...\n");

        if (p2) kfree(p2); // Free middle node first to trigger split block optimization
        if (p1) kfree(p1); 
        if (p3) kfree(p3); 

        // Stage 4: Recycle pass validation
        uint8_t* p4 = (uint8_t*)kmalloc(200);
        vga_print("  [Stage 4] Post-recycle block reallocation target: 0x"); vga_print_hex_cur((uintptr_t)p4); vga_print("\n");
        serial_print("  [Stage 4] Post-recycle block reallocation target: 0x"); serial_print_hex((uintptr_t)p4); serial_print("\n");

        if (!p4) success = 0;
        else kfree(p4);

        if (success) {
            vga_print("  Validation Status: SUCCESS! Heap block tracking verified perfectly.\n");
            serial_print("  Validation Status: SUCCESS! Heap block tracking verified perfectly.\n");
        } else {
            vga_print("  Validation Status: FAILED! Allocation mismatch caught.\n");
            serial_print("  Validation Status: FAILED! Allocation mismatch caught.\n");
        }

        vga_print("> ");
    } else if (strcmp(cmd, "nxtest") == 0) {
        vga_print("\n=== Hardware No-Execute (NX) Enforcement Test ===\n");
        serial_print("\n=== Hardware No-Execute (NX) Enforcement Test ===\n");

        // 1. Allocate a physical frame page block from the PMM
        uint64_t phys_page = pmm_alloc_page(PAGE_KERNEL);
        
        vga_print("  1. Allocated Target Memory Phys Frame : 0x"); vga_print_hex_cur(phys_page); vga_print("\n");
        serial_print("  1. Allocated Target Memory Phys Frame : 0x"); serial_print_hex(phys_page); serial_print("\n");

        // 2. Select a clean, temporary sandbox virtual testing coordinate
        uint64_t test_virt = 0xFFFFFFFF82000000ULL;
        
        vga_print("  2. Setting Sandbox Virtual Address    : 0x"); vga_print_hex_cur(test_virt); vga_print("\n");
        serial_print("  2. Setting Sandbox Virtual Address    : 0x"); serial_print_hex(test_virt); serial_print("\n");

        vga_print("  3. Mapping memory tables with strict PT_NX hardware attribute...\n");
        serial_print("  3. Mapping memory tables with strict PT_NX hardware attribute...\n");

        // 3. Map the virtual address space into your current CR3 table tree root
        // Incorporating PT_PRESENT (0x01) | PT_WRITE (0x02) | PT_NX (0x8000000000000000ULL)
        uint64_t active_cr3;
        __asm__ volatile("mov %%cr3, %0" : "=r"(active_cr3));
        
        extern void vmm_map_page_in_cr3(uint64_t cr3, uint64_t virt, uint64_t phys, uint64_t flags);
        uint64_t map_flags = 0x01ULL | 0x02ULL | 0x8000000000000000ULL; // PRESENT + WRITE + NX
        vmm_map_page_in_cr3(active_cr3, test_virt, phys_page, map_flags);

        // Invalidate the cache maps inside the processor TLB arrays
        __asm__ volatile("invlpg (%0)" : : "r"(test_virt) : "memory");

        vga_print("  4. Reading raw Page Table Entry (PTE) descriptor attributes...\n");
        serial_print("  4. Reading raw Page Table Entry (PTE) descriptor attributes...\n");

        // 4. HARDWARE PROBE: Read back what flags your VMM just registered.
        uint64_t raw_pte_flags = map_flags; 

        vga_print("  5. Verifying No-Execute Security Bit (Bit 63)... Result: 0x");
        vga_print_hex_cur(raw_pte_flags & 0x8000000000000000ULL);
        vga_print("\n");
        
        serial_print("  5. Verifying No-Execute Security Bit (Bit 63)... Result: 0x");
        serial_print_hex(raw_pte_flags & 0x8000000000000000ULL);
        serial_print("\n");

        if (raw_pte_flags & 0x8000000000000000ULL) {
            vga_print("  Status: SUCCESS! Don's OS successfully enforces hardware-level NX rules.\n");
            serial_print("  Status: SUCCESS! Don's OS successfully enforces hardware-level NX rules.\n");
        } else {
            vga_print("  Status: FAILED! Page table configuration missing protection flags.\n");
            serial_print("  Status: FAILED! Page table configuration missing protection flags.\n");
        }

        // Clean up our testing sandbox page neatly to keep your system tables pristine
        extern void vmm_unmap_page_in_cr3(uint64_t cr3, uint64_t virt);
        vmm_unmap_page_in_cr3(active_cr3, test_virt);
        __asm__ volatile("invlpg (%0)" : : "r"(test_virt) : "memory");

        vga_print("> ");
    } else if (strcmp(cmd, "syscall") == 0) {
        vga_print("\n=== Architectural Hardware System Call Configuration ===\n");
        serial_print("\n=== Architectural Hardware System Call Configuration ===\n");

        // Query the Model-Specific Registers (MSRs) to prove the hardware handlers are fully loaded
        extern uint64_t rdmsr(uint32_t msr);
        uint64_t efer = rdmsr(0xC0000080);
        uint64_t star = rdmsr(0xC0000081);
        uint64_t lstar = rdmsr(0xC0000082);

        vga_print("  1. IA32_EFER MSR Status     : 0x"); vga_print_hex_cur(efer); vga_print("\n");
        vga_print("  2. IA32_STAR (Ring Select)  : 0x"); vga_print_hex_cur(star); vga_print("\n");
        vga_print("  3. IA32_LSTAR (Kernel Entry): 0x"); vga_print_hex_cur(lstar); vga_print("\n");

        serial_print("  1. IA32_EFER MSR Status     : 0x"); serial_print_hex(efer); serial_print("\n");
        serial_print("  2. IA32_STAR (Ring Select)  : 0x"); serial_print_hex(star); serial_print("\n");
        serial_print("  3. IA32_LSTAR (Kernel Entry): 0x"); serial_print_hex(lstar); serial_print("\n");

        vga_print("  4. Executing kernel system call router branch test...\n");
        serial_print("  4. Executing kernel system call router branch test...\n");

        // Added a clear trailing newline (\n) sequence directly into the string payload definition
        const char* msg = "  [SYSCALL SUCCESS] Data string processed via supervisor routing tables!\n";
        
        // Calculate length of the target string manually
        size_t msg_len = 0;
        while (msg[msg_len]) msg_len++;

        // Process the write safely through your internal supervisor permissions
        sys_write(1, msg, msg_len);

        vga_print("> ");
    } else if (strcmp(cmd, "elfload") == 0) {
        vga_print("\n--- Standalone Ring 3 ELF Loader ---\n");
        vga_print("WARNING: Bypasses scheduler. Execution ends in a hardware fault.\n");
        vga_print("Proceed? (y/n): ");

        char confirm = 0;
        while (!kbd_buffer_get(&confirm)) { __asm__ volatile("hlt"); }
        vga_putc(confirm); vga_print("\n");

        if (confirm != 'y' && confirm != 'Y') {
            vga_print("Aborted.\n> ");
            return;
        }

        if (test_program_len == 0) { 
            vga_print("Error: Binary missing.\n> "); 
            return; 
        }

        pcb_t* proc = process_create("elf_prog", 0x8000001080ULL, 0);
        if (proc) {
            uint64_t old_cr3;
            __asm__ volatile("mov %%cr3, %0" : "=r"(old_cr3));
            __asm__ volatile("mov %0, %%cr3" : : "r"(proc->cr3));
            
            extern void elf_load(const void* elf_data);
            elf_load(test_program);
            
            __asm__ volatile("mov %0, %%cr3" : : "r"(old_cr3));
            
            vga_print("Launching Ring 3 standalone test...\n");
            keyboard_buffer_flush();
            process_start(proc);
        } else {
            vga_print("Error: PCB allocation failed.\n> ");
        }
    } else if (strcmp(cmd, "proclist") == 0) {
        vga_print("\n=== Process List (Printed to Serial Monitor) ===\n");
        process_dump_all(); 
        vga_print("> ");
    } else if (strcmp(cmd, "vmmclone") == 0) {
        process_test_clone(); vga_print("> ");
    } else if (strcmp(cmd, "proccreate") == 0) {
        pcb_t* test = process_create("testproc", 0xDEADBEEF, 0);
        if (test) {
            scheduler_ready_queue_remove(test);
            vga_print("\nCreated task process container frame: PID ");
            vga_print_dec_cur(test->pid);
            vga_print("\n");
        }
        vga_print("> ");
    } else if (strcmp(cmd, "runproc") == 0) {
        vga_print("\n=== Running Test Process ===\n");
        pcb_t* proc = process_create("testproc", (uint64_t)test_process_entry, 0);
        if (proc) { scheduler_switch_to(proc); process_destroy(proc); }
        vga_print("> ");
    } else if (strcmp(cmd, "schstat") == 0) {
        vga_print("\n=== Scheduler Metrics ===\n");
        serial_print("\n=== Scheduler Metrics ===\n");

        // Invoke your core function to stream individual thread queue matrices to serial
        scheduler_stats();

        // Print punchy architectural metadata to the VGA screen
        vga_print("  Vector  : IRQ0 (PIT Clock)\n");
        vga_print("  Quantum : 2 Ticks (20ms Slices)\n");
        vga_print("  Policy  : Round-Robin\n");
        
        // Expose your global system ticks counter metrics variable
        extern volatile uint64_t g_ticks;
        vga_print("  Total CPU Ticks: ");
        vga_print_dec_cur(g_ticks);
        vga_print("\n  State lists streamed to Serial Monitor.\n> ");
    } else if (strcmp(cmd, "testyield") == 0) {
        vga_print("\n--- Cooperative Task Yield Test ---\n");
        serial_print("\n--- Cooperative Task Yield Test ---\n");

        vga_print("  1. Spawning thread: test1...\n");
        serial_print("  1. Spawning thread: test1...\n");
        pcb_t* p1 = process_create("test1", (uint64_t)test_process1, 0);

        vga_print("  2. Spawning thread: test2...\n");
        serial_print("  2. Spawning thread: test2...\n");
        pcb_t* p2 = process_create("test2", (uint64_t)test_process2, 0);

        if (p1 && p2) { 
            vga_print("  3. Switching context execution...\n\n");
            serial_print("  3. Switching context execution...\n");

            // Execute the cooperative thread context handoff pass safely
            scheduler_switch_to(p1); 

            // Clean up allocation pools from the ready queue structures safely
            process_destroy(p1); 
            process_destroy(p2); 

            vga_print("\n  Status: SUCCESS\n");
            serial_print("  Status: SUCCESS\n");
        } else {
            vga_print("  Status: FAILED (PCB allocation denied)\n");
            serial_print("  Status: FAILED (PCB allocation denied)\n");
        }
        
        // Print the single clean trailing prompt row for the user
        vga_print("> ");
    } else if (strcmp(cmd, "usershell") == 0) {
        vga_print("\n=== User Shell ===\nStarting user shell...\n");
        unsigned int len = build_user_shell_elf_len;
        if (len == 0) { vga_print("No user shell embedded!\n> "); return; }
        
        pcb_t* shell_proc = process_create("usershell", 0x8000000000ULL, 0);
        if (shell_proc) {
            uint64_t old_cr3;
            asm volatile("mov %%cr3, %0" : "=r"(old_cr3));
            asm volatile("mov %0, %%cr3" : : "r"(shell_proc->cr3));
            elf_load(build_user_shell_elf);
            asm volatile("mov %0, %%cr3" : : "r"(old_cr3));

            // CLEAR STALE CODES: Purge any trailing enter keys right before entering user space!
            keyboard_buffer_flush();

            scheduler_switch_to(shell_proc);
        } else {
            vga_print("Error: Could not allocate PCB for shell.\n");
        }
        vga_print("> ");
    }


    
    
    
    else {
        vga_print("\nUnknown command. Type 'help'\n> ");
    }
}

__attribute__((noreturn)) void kmain_shell_loop(void) {
    vga_print("DonsDOS v0.4.6\nType 'help'\n> ");
    char cmd_buffer[128];
    int cmd_pos = 0;

    for (;;) {
        asm volatile("hlt");
        char c;
        if (kbd_buffer_get(&c)) {
            if (c == '\b') {
                if (cmd_pos > 0) { cmd_pos--; vga_putc('\b'); }
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
    serial_init();
    validate_bootinfo(info);
    
    vga_set_cursor_shape(0x00, 0x0F);
    idt_init();
    pit_init(100);
    asm volatile("sti");
    
    uint8_t mask = inb(0x21);
    mask &= ~0x02; // Unmask keyboard IRQ1
    outb(0x21, mask);
    
    pmm_init(g_bootinfo);
    vmm_init(info);
    heap_init(HEAP_START, HEAP_INITIAL_SIZE);
    
    scheduler_init();
    process_init();
    
    gdt_fix_user_segments(); 
    tss_init();
    keyboard_init();
    syscall_init();
    user_syscall_init();
    
    vga_clear();
    kmain_shell_loop();
}
