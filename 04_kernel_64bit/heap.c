// heap.c - Simple bump allocator with free list
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
static uint64_t heap_mapped_end = 0;
static void* free_list = NULL;
static int heap_initialized = 0;

static int heap_expand(size_t size) {
    size_t pages = (size + 0xFFF) / 4096;
    size_t bytes = pages * 4096;
    uint64_t new_mapped_end = heap_mapped_end + bytes;
    
    if (new_mapped_end > heap_end) {
        return -1;
    }
    
    for (uint64_t addr = heap_mapped_end; addr < new_mapped_end; addr += 4096) {
        uint64_t phys = pmm_alloc_page();
        if (phys == 0) {
            return -1;
        }
        vmm_map_page(addr, phys, PT_PRESENT | PT_WRITE);
    }
    
    heap_mapped_end = new_mapped_end;
    return 0;
}

void heap_init(void) {
    vga_print("HEAP: Initializing...\n");
    serial_print("HEAP: Initializing...\n");
    
    heap_start = HEAP_START;
    heap_end = HEAP_START + HEAP_INITIAL_SIZE;
    heap_brk = heap_start;
    heap_mapped_end = heap_start;
    free_list = NULL;
    
    if (heap_expand(1 * 1024 * 1024) != 0) {
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
    if (!heap_initialized || size == 0) return NULL;
    size = (size + 7) & ~7;
    void* ptr = free_list;
    if (ptr) {
        free_list = *(void**)ptr;
        return ptr;
    }
    if (heap_brk + size > heap_mapped_end) {
        size_t needed = (heap_brk + size) - heap_mapped_end;
        if (heap_expand(needed) != 0) return NULL;
    }
    if (heap_brk + size > heap_end) {
        return NULL;
    }
    ptr = (void*)heap_brk;
    heap_brk += size;
    return ptr;
}

void kfree(void* ptr) {
    if (!ptr || !heap_initialized) return;
    void** free_ptr = (void**)ptr;
    *free_ptr = free_list;
    free_list = ptr;
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
    size_t free = total - used;
    
    vga_print("\n=== Heap Stats ===\n");
    vga_print("  Used: ");
    vga_print_dec_cur(used / 1024);
    vga_print(" KB\n");
    vga_print("  Free: ");
    vga_print_dec_cur(free / 1024);
    vga_print(" KB\n");
    vga_print("  Total: ");
    vga_print_dec_cur(total / 1024);
    vga_print(" KB\n");
    
    int free_count = 0;
    void* curr = free_list;
    while (curr) {
        free_count++;
        curr = *(void**)curr;
    }
    vga_print("  Free list: ");
    vga_print_dec_cur(free_count);
    vga_print(" blocks\n> ");
}

size_t heap_used(void) { 
    return heap_brk - heap_start; 
}

size_t heap_free(void) { 
    return (heap_end - heap_start) - (heap_brk - heap_start); 
}

size_t heap_total(void) { 
    return heap_end - heap_start; 
}
