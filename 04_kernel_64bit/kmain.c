#include <stdint.h>
#include <stddef.h>
#include "include/bootinfo.h"
#include "include/pmm.h"
#include "include/idt.h"
#include "include/vga.h"
#include "include/keyboard.h"
#include "include/interrupts.h"

extern void pit_init(uint32_t freq);

static BootInfo *g_bootinfo = NULL;

static int strcmp(const char *s1, const char *s2) {
    while (*s1 && (*s1 == *s2)) {
        s1++;
        s2++;
    }
    return *(const unsigned char*)s1 - *(const unsigned char*)s2;
}


static void print_hex_cur(uint64_t val) {
    const char *hex = "0123456789ABCDEF";
    for (int shift = 60; shift >= 0; shift -= 4) {
        vga_putc(hex[(val >> shift) & 0xF]);
    }
}


/*
static void print_hex_cur(uint64_t val) {
    const char *hex = "0123456789ABCDEF";
    for (int shift = 60; shift >= 0; shift -= 4) {
        vga_putc(hex[(val >> shift) & 0xF]);
        // Small delay after each character
        for (int i = 0; i < 5; i++) {
            __asm__ volatile("nop");
        }
    }
}
*/
static void handle_command(const char *cmd) {
    if (strcmp(cmd, "help") == 0) {
        vga_print("\nAvailable commands:\n");
        vga_print("  help     - Show this help\n");
        vga_print("  clear    - Clear the screen\n");
        vga_print("  version  - Show version info\n");
        vga_print("  reboot   - Reboot the system\n");
        vga_print("  pmmtest  - Test Physical Memory Manager\n");
        vga_print("  info     - Show boot information\n");
        vga_print("  mem      - Show memory information\n");
    } else if (strcmp(cmd, "clear") == 0) {
        vga_clear();
        vga_print("DonsDOS v0.1\n");
        vga_print("Type 'help'\n ");
    } else if (strcmp(cmd, "version") == 0) {
        vga_print("\nDonsDOS v0.1\n");
        vga_print("Build: 64-bit kernel with VGA console\n");
        vga_print("Copyright (c) 2024 Don's OS Project\n");
    } else if (strcmp(cmd, "info") == 0) {
        vga_print("\nBoot Information:\n");
        if (g_bootinfo) {
            vga_print("  PML4 addr: 0x");
            print_hex_cur(g_bootinfo->pml4_addr);
            vga_print("\n");
            
            vga_print("  Kernel phys start: 0x");
            print_hex_cur(g_bootinfo->kernel_phys_start);
            vga_print("\n");
            
            vga_print("  Kernel phys end: 0x");
            print_hex_cur(g_bootinfo->kernel_phys_end);
            vga_print("\n");
            
            vga_print("  E820 count: ");
            // Simple decimal print for count
            char buf[4];
            int n = g_bootinfo->memory_map_count;
            int i = 0;
            if (n == 0) { vga_print("0"); }
            else {
                while (n > 0) {
                    buf[i++] = '0' + (n % 10);
                    n /= 10;
                }
                while (i > 0) { vga_putc(buf[--i]); }
            }
            vga_print("\n");
        } else {
            vga_print("  No boot info available\n");
        }
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
            
            vga_print("  Usable RAM: ");
            // Simple decimal print
            uint64_t val = total_usable / (1024 * 1024);
            char buf[12];
            int i = 11;
            buf[i] = '\0';
            if (val == 0) { vga_print("0"); }
            else {
                while (val > 0 && i > 0) {
                    i--;
                    buf[i] = '0' + (val % 10);
                    val /= 10;
                }
                vga_print(&buf[i]);
            }
            vga_print(" MB\n");
            
            vga_print("  Reserved RAM: ");
            val = total_reserved / (1024 * 1024);
            i = 11;
            buf[i] = '\0';
            if (val == 0) { vga_print("0"); }
            else {
                while (val > 0 && i > 0) {
                    i--;
                    buf[i] = '0' + (val % 10);
                    val /= 10;
                }
                vga_print(&buf[i]);
            }
            vga_print(" MB\n");
        } else {
            vga_print("  Memory map not available\n");
        }
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
        print_hex_cur(p1);
        vga_print("\n");
        
        vga_print("  Page2: 0x");
        print_hex_cur(p2);
        vga_print("\n");
        
        vga_print("  Page3: 0x");
        print_hex_cur(p3);
        vga_print("\n");
        
        // Add a small delay or extra newline before free
        pmm_free_page(p2);
        vga_print("\r  Freed page2\n");
        
        uint64_t p4 = pmm_alloc_page();
        vga_print("  Page4: 0x");
        print_hex_cur(p4);
        vga_print("\n");
        vga_print("\r  Freed page2");
        vga_print("\r\nTest complete.\r\n");
    }


/*
    } else if (strcmp(cmd, "pmmtest") == 0) {
        vga_print("\nPMM Test:\n");
        pmm_init(g_bootinfo);
        
        uint64_t p1 = pmm_alloc_page();
        
        vga_print("  Page1: 0x");
        print_hex_cur(p1);
        vga_print("\n");
        vga_print("Test complete.\n");
    }

    */

}

void kmain(BootInfo *info) {
    g_bootinfo = info;
    
    vga_clear();
    vga_set_cursor_shape(0x00, 0x0F);
    
    vga_print("DonsDOS v0.1\n");
    vga_print("Type 'help'\n> ");

    idt_init();
    pit_init(100);
    keyboard_init();

    asm volatile("sti");

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
                vga_print("> ");
                continue;
            }
            
            if (c >= ' ' && c <= '~' && cmd_pos < 127) {
                cmd_buffer[cmd_pos++] = c;
                vga_putc(c);
            }
        }
    }
}
