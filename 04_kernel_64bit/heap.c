// heap.c - Simple heap allocator with free list and size metadata
#include <stdint.h>
#include <stddef.h>
#include "heap.h"
#include "vmm.h"
#include "pmm.h"
#include "vga.h"
#include "serial.h"

typedef struct free_block {
    struct free_block* next;
    size_t size;
} free_block_t;

static uint64_t heap_start = 0;
static uint64_t heap_end = 0;
static uint64_t heap_brk = 0;
static uint64_t heap_mapped_end = 0;
static free_block_t* free_list = NULL;
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

        if (!vmm_is_mapped(addr)) {
            serial_print("HEAP: Page 0x");
            serial_print_hex(addr);
            serial_print(" is NOT mapped!\n");
            return -1;
        }
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

static void* alloc_from_free_list(size_t size) {
    free_block_t** prev = &free_list;
    free_block_t* curr = free_list;

    while (curr) {
        if (curr->size >= size) {
            *prev = curr->next;
            return (void*)(curr + 1);
        }
        prev = &curr->next;
        curr = curr->next;
    }

    return NULL;
}

void* kmalloc(size_t size) {
    if (!heap_initialized || size == 0) return NULL;

    size = (size + 7) & ~7;
    size_t total_size = size + sizeof(free_block_t);

    void* ptr = alloc_from_free_list(size);
    if (ptr) {
        return ptr;
    }

    if (heap_brk + total_size > heap_mapped_end) {
        size_t needed = (heap_brk + total_size) - heap_mapped_end;
        if (heap_expand(needed) != 0) return NULL;
    }

    if (heap_brk + total_size > heap_end) {
        return NULL;
    }

    free_block_t* header = (free_block_t*)heap_brk;
    header->size = size;
    header->next = NULL;

    void* user_ptr = (void*)(header + 1);
    heap_brk += total_size;

    return user_ptr;
}

void kfree(void* ptr) {
    if (!ptr || !heap_initialized) return;

    uint64_t addr = (uint64_t)ptr;
    if (addr < heap_start || addr >= heap_brk) {
        return;
    }

    free_block_t* header = ((free_block_t*)ptr) - 1;
    header->next = free_list;
    free_list = header;
}

void* krealloc(void* ptr, size_t new_size) {
    if (ptr == NULL) return kmalloc(new_size);
    if (new_size == 0) { kfree(ptr); return NULL; }

    free_block_t* old_header = ((free_block_t*)ptr) - 1;
    size_t old_size = old_header->size;

    void* new_ptr = kmalloc(new_size);
    if (!new_ptr) return NULL;

    size_t copy_size = (old_size < new_size) ? old_size : new_size;
    uint8_t* src = (uint8_t*)ptr;
    uint8_t* dst = (uint8_t*)new_ptr;
    for (size_t i = 0; i < copy_size; i++) {
        dst[i] = src[i];
    }

    kfree(ptr);
    return new_ptr;
}

void heap_stats(void) {
    if (!heap_initialized) {
        vga_print("\nHeap not initialized\n> ");
        return;
    }

    size_t used_raw = heap_brk - heap_start;

    size_t free_bytes = 0;
    int free_count = 0;
    free_block_t* curr = free_list;
    while (curr) {
        free_bytes += curr->size + sizeof(free_block_t);
        free_count++;
        curr = curr->next;
    }

    size_t used = used_raw - free_bytes;
    size_t free = (heap_end - heap_start) - used;

    vga_print("\n=== Heap Stats ===\n");
    vga_print("  Used: ");
    vga_print_dec_cur(used / 1024);
    vga_print(" KB\n");
    vga_print("  Free: ");
    vga_print_dec_cur(free / 1024);
    vga_print(" KB\n");
    vga_print("  Total: ");
    vga_print_dec_cur((heap_end - heap_start) / 1024);
    vga_print(" KB\n");
    vga_print("  Free list: ");
    vga_print_dec_cur(free_count);
    vga_print(" blocks\n> ");
}

size_t heap_used(void) {
    size_t used_raw = heap_brk - heap_start;

    size_t free_bytes = 0;
    free_block_t* curr = free_list;
    while (curr) {
        free_bytes += curr->size + sizeof(free_block_t);
        curr = curr->next;
    }

    return used_raw - free_bytes;
}

size_t heap_free(void) {
    return (heap_end - heap_start) - heap_used();
}

size_t heap_total(void) {
    return heap_end - heap_start;
}
