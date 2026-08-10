// Comment
#include <stdint.h>
#include <stddef.h>
#include "include/heap.h"
#include "include/vmm.h"
#include "include/pmm.h"
#include "include/vga.h"
#include "include/serial.h"

static uint64_t heap_start = 0;
static uint64_t heap_end = 0;
static uint64_t heap_brk = 0;
static int heap_initialized = 0;

static int heap_expand(size_t size) {
    size_t pages = (size + 0xFFF) / 4096;
    size_t bytes = pages * 4096;
    uint64_t new_brk = heap_brk + bytes;
    
    if (new_brk > heap_end) {
        return -1;
    }
    
    for (uint64_t addr = heap_brk; addr < new_brk; addr += 4096) {
        uint64_t phys = pmm_alloc_page();
        if (phys == 0) {
            return -1;
        }
        // Use VMM to map the page
        vmm_map_page(addr, phys, PT_PRESENT | PT_WRITE);
    }
    
    heap_brk = new_brk;
    return 0;
}

void heap_init(void) {
    vga_print("HEAP: Initializing...\n");
    serial_print("HEAP: Initializing...\n");
    
    heap_start = HEAP_START;
    heap_end = HEAP_START + HEAP_INITIAL_SIZE;
    heap_brk = heap_start;
    
    if (heap_expand(HEAP_INITIAL_SIZE / 4) != 0) {
        vga_print("HEAP: Failed to allocate initial memory!\n");
        serial_print("HEAP: Failed to allocate initial memory!\n");
        return;
    }
    
    heap_initialized = 1;
    
    vga_print("HEAP: Initialized at 0x");
    vga_print_hex_cur(heap_start);
    vga_print("\n");
    serial_print("HEAP: Initialized at 0x");
    serial_print_hex(heap_start);
    serial_print("\n");
}

void* kmalloc(size_t size) {
    if (!heap_initialized || size == 0) {
        return NULL;
    }
    
    size = (size + 7) & ~7;
    
    if (heap_brk + size > heap_end) {
        if (heap_expand(size) != 0) {
            return NULL;
        }
    }
    
    void* ptr = (void*)heap_brk;
    heap_brk += size;
    return ptr;
}

void kfree(void* ptr) {
    (void)ptr;
}

void* krealloc(void* ptr, size_t new_size) {
    if (ptr == NULL) return kmalloc(new_size);
    if (new_size == 0) { kfree(ptr); return NULL; }
    return kmalloc(new_size);
}

void heap_stats(void) {
    if (!heap_initialized) {
        vga_print("\nHeap not initialized\n> ");
        return;
    }
    
    size_t used = heap_brk - heap_start;
    size_t total = heap_end - heap_start;
    
    vga_print("\n=== Heap Stats ===\n");
    vga_print("  Used: ");
    vga_print_dec_cur(used / 1024);
    vga_print(" KB\n");
    vga_print("  Free: ");
    vga_print_dec_cur((total - used) / 1024);
    vga_print(" KB\n");
    vga_print("  Total: ");
    vga_print_dec_cur(total / 1024);
    vga_print(" KB\n> ");
}
