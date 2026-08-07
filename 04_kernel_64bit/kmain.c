#include <stdint.h>
#include <stddef.h>
#include "include/bootinfo.h"
#include "include/pmm.h"
#include "include/idt.h"
#include "include/vga.h"
#include "include/keyboard.h"
#include "include/interrupts.h"

extern void pit_init(uint32_t freq);

// Store boot info globally for commands
static BootInfo *g_bootinfo = NULL;

// Simple string functions for command handling
static int strcmp(const char *s1, const char *s2) {
    while (*s1 && (*s1 == *s2)) {
        s1++;
        s2++;
    }
    return *(const unsigned char*)s1 - *(const unsigned char*)s2;
}

static void print_prompt(void) {
    vga_print("\n> ");
}

// Simple command handler
static void handle_command(const char *cmd) {
    if (strcmp(cmd, "help") == 0) {
        vga_print("\nAvailable commands:\n");
        vga_print("  help     - Show this help\n");
        vga_print("  clear    - Clear the screen\n");
        vga_print("  info     - Show system info\n");
        vga_print("  reboot   - Reboot the system\n");
        vga_print("  version  - Show version info\n");
        vga_print("  mem      - Show memory information\n");
    } else if (strcmp(cmd, "clear") == 0) {
        vga_clear();
        // Re-print boot banner and prompt
        vga_print("DonsDOS v0.1 - 64-bit Operating System\n");
        vga_print("Type 'help' for available commands\n");
        print_prompt();
    } else if (strcmp(cmd, "info") == 0) {
        vga_print("\nDonsDOS System Information:\n");
        vga_print("  Architecture: x86_64\n");
        vga_print("  Mode: Long mode (64-bit)\n");
        vga_print("  Console: VGA text mode\n");
        vga_print("  Keyboard: PS/2\n");
        vga_print("  Memory: PMM initialized\n");
        
        if (g_bootinfo) {
            vga_print("\nBoot Information:\n");
            vga_print("  PML4 addr: ");
            vga_print_hex(0, 0, g_bootinfo->pml4_addr);
            vga_print("\n");
            
            vga_print("  Kernel phys start: ");
            vga_print_hex(0, 0, g_bootinfo->kernel_phys_start);
            vga_print("\n");
            
            vga_print("  Kernel phys end: ");
            vga_print_hex(0, 0, g_bootinfo->kernel_phys_end);
            vga_print("\n");
            
            vga_print("  E820 entries: ");
            vga_print_dec(0, 0, g_bootinfo->memory_map_count);
            vga_print("\n");
            
            // Calculate total usable memory
            MemoryMapEntry *m = (MemoryMapEntry *)g_bootinfo->memory_map_addr;
            uint64_t total_usable = 0;
            for (uint64_t i = 0; i < g_bootinfo->memory_map_count; i++) {
                if (m[i].type == 1) {
                    total_usable += m[i].length;
                }
            }
            vga_print("  Total usable RAM: ");
            vga_print_dec(0, 0, total_usable / (1024 * 1024));
            vga_print(" MB\n");
        }
    } else if (strcmp(cmd, "version") == 0) {
        vga_print("\nDonsDOS v0.1\n");
        vga_print("Build: 64-bit kernel with VGA console\n");
        vga_print("Copyright (c) 2024 Don's OS Project\n");
    } else if (strcmp(cmd, "mem") == 0) {
        vga_print("\nMemory Information:\n");
        vga_print("  Page size: 4096 bytes\n");
        vga_print("  Max pages: 131072 (512MB max)\n");
        vga_print("  PMM: Initialized\n");
        
        if (g_bootinfo) {
            MemoryMapEntry *m = (MemoryMapEntry *)g_bootinfo->memory_map_addr;
            uint64_t total_usable = 0;
            uint64_t total_reserved = 0;
            
            for (uint64_t i = 0; i < g_bootinfo->memory_map_count; i++) {
                if (m[i].type == 1) {
                    total_usable += m[i].length;
                } else {
                    total_reserved += m[i].length;
                }
            }
            
            vga_print("  Usable RAM: ");
            vga_print_dec(0, 0, total_usable / (1024 * 1024));
            vga_print(" MB\n");
            
            vga_print("  Reserved RAM: ");
            vga_print_dec(0, 0, total_reserved / (1024 * 1024));
            vga_print(" MB\n");
        }
    } else if (strcmp(cmd, "reboot") == 0) {
        vga_print("\nRebooting...\n");
        // Trigger a reboot via keyboard controller
        __asm__ volatile (
            "cli\n"
            "mov $0x64, %%al\n"
            "outb %%al, $0x64\n"
            "mov $0xFE, %%al\n"
            "outb %%al, $0x64\n"
            "hlt\n"
            : : : "memory"
        );
    } else if (cmd[0] != '\0') {
        vga_print("\nUnknown command: ");
        vga_print(cmd);
        vga_print("\nType 'help' for available commands");
    }
}

/*
static void test_gpf(void) {
    // Deliberately trigger a General Protection Fault
    __asm__ volatile (
        "mov $0x23, %%ax\n\t"   // bogus segment selector
        "mov %%ax, %%ds\n\t"    // should cause #GP
        :
        :
        : "ax"
    );
}
*/

/*
static void test_pf(void) {
    volatile uint64_t *ptr = (uint64_t *)0xFFFFFFFFFFFF;  // unmapped
    *ptr = 0x1234;  // triggers #PF
}
*/

void kmain(BootInfo *info) {
    // Store boot info for commands
    g_bootinfo = info;
    
    // Clear screen
    vga_clear();
    
    // Set cursor to a full block (instead of underline)
    vga_set_cursor_shape(0x00, 0x0F);  // Full block cursor
    // Shape	           Start	End	Description
    // Block	           0x00	        0x0F	Full character block
    // Top Half Block      0x00	        0x07	Top 8 scanlines
    // Bottom Half Block   0x08	        0x0F	Bottom 8 scanlines
    // Thick Block	   0x00	        0x0D	Almost full block
    // Thin Block	   0x02	        0x0D	Slightly smaller block
    // Underline	   0x0E	        0x0F	Default underline
    // Disabled	           0x20	        0x00	No cursor visible
    
    // Print header - starts at row 0 now
    vga_print("DonsDOS v0.1 - 64-bit Operating System\n");
    vga_print("========================================\n");
    vga_print("Type 'help' for available commands\n");
    
    // Calculate RAM
    MemoryMapEntry *m = (MemoryMapEntry *)info->memory_map_addr;
    uint64_t total_usable = 0;
    for (uint64_t i = 0; i < info->memory_map_count; i++) {
        if (m[i].type == 1) {
            total_usable += m[i].length;
        }
    }
    
    // Print system info - use vga_print_dec_cur for current cursor position
    vga_print("System: x86_64 Long Mode | RAM: ");
    vga_print_dec_cur(total_usable / (1024 * 1024));
    vga_print(" MB | Console: VGA 80x25\n");
    
    vga_print("----------------------------------------\n");

    // Initialize system components
    vga_print("Init: before IDT\n");
    idt_init();
    vga_print("Init: after IDT\n");
    pit_init(100);      // 100 Hz timer
    vga_print("Init: after PIT\n");
    keyboard_init();    // buffer initialized
    vga_print("Init: after keyboard\n");

    // Test GPF handler once
    // test_gpf();
    // Test for #PF
    // test_pf();

    // Enable interrupts
    asm volatile("sti");

    // Show the first prompt
    print_prompt();

    // Command buffer
    #define CMD_BUFFER_SIZE 128
    static char cmd_buffer[CMD_BUFFER_SIZE];
    int cmd_pos = 0;

    // MAIN LOOP
    for (;;) {
        asm volatile("hlt");

        char c;
        if (kbd_buffer_get(&c)) {
            // Handle backspace
            if (c == '\b') {
                if (cmd_pos > 0) {
                    cmd_pos--;
                    vga_putc('\b');
                }
                continue;
            }
            
            // Handle enter
            if (c == '\n') {
                vga_putc('\n');
                
                // Null-terminate the command
                cmd_buffer[cmd_pos] = '\0';
                
                // Handle the command
                handle_command(cmd_buffer);
                
                // Reset buffer for next command
                cmd_pos = 0;
                
                // Print new prompt
                print_prompt();
                continue;
            }
            
            // Handle printable characters
            if (c >= ' ' && c <= '~' && cmd_pos < CMD_BUFFER_SIZE - 1) {
                cmd_buffer[cmd_pos++] = c;
                vga_putc(c);
            }
        }
    }
}
