#include "include/heap.h"
#include "include/serial.h"
#include "include/vga.h"
#include "include/pmm.h"
#include "include/vmm.h"
#include <stdint.h>
#include <stddef.h>

#define HEAP_START   0xFFFF900000000000ULL
// Keep max heap well below LOW-zone capacity so page tables always have room.
#define HEAP_MAX_SIZE (24ULL * 1024ULL * 1024ULL)      // 24MB max heap

static uint64_t heap_start = HEAP_START;
static uint64_t heap_end   = HEAP_START;
static heap_header_t* free_list = NULL;
static uint64_t heap_allocated  = 0;
static uint64_t heap_used_bytes = 0;

// Extend the heap by allocating new kernel pages (LOW zone)
static int heap_extend(size_t size) {
    size_t pages_needed = (size + PAGE_SIZE - 1) / PAGE_SIZE;
    size_t total_size   = pages_needed * PAGE_SIZE;
    
    // Check if we're within limits
    if (heap_allocated + total_size > HEAP_MAX_SIZE) {
        serial_print("HEAP: Cannot extend - would exceed max size\n");
        return 0;
    }
    
    serial_print("HEAP: Extending heap by ");
    serial_print_dec(total_size / 1024);
    serial_print(" KB (");
    serial_print_dec(pages_needed);
    serial_print(" pages)\n");
    
    // Allocate physical pages for the heap from ELF type (HIGH zone)
    for (size_t i = 0; i < pages_needed; i++) {
        uint64_t phys = pmm_alloc_page_for_elf();
        if (!phys) {
            serial_print("HEAP: Failed to allocate physical page\n");
            return 0;
        }
        
        uint64_t virt = heap_end + (i * PAGE_SIZE);
        vmm_map_page(virt, phys, PT_PRESENT | PT_WRITE);
        
        // Ensure the mapping is visible
        __asm__ volatile ("invlpg (%0)" : : "r" (virt) : "memory");
    }
    
    heap_end       += total_size;
    heap_allocated += total_size;
    
    serial_print("HEAP: Extended to ");
    serial_print_dec(heap_allocated / 1024);
    serial_print(" KB total\n");
    
    return 1;
}

void heap_init(uint64_t start, size_t size) {
    serial_print("HEAP: Initializing...\n");
    
    heap_start      = start;
    heap_end        = start;
    heap_allocated  = 0;
    heap_used_bytes = 0;
    free_list       = NULL;
    
    // Allocate initial heap pages (small, safe chunk)
    if (!heap_extend(size)) {
        serial_print("HEAP: Failed to allocate initial heap!\n");
        return;
    }
    
    // Initialize the free list with the entire initial heap
    heap_header_t* header = (heap_header_t*)heap_start;
    header->magic = HEAP_MAGIC;
    header->size  = size - sizeof(heap_header_t);
    header->used  = 0;
    header->next  = NULL;
    header->prev  = NULL;
    free_list     = header;
    
    serial_print("HEAP: Initialized at 0x");
    serial_print_hex(heap_start);
    serial_print(" size=");
    serial_print_dec(size / 1024);
    serial_print(" KB\n");
}

static heap_header_t* find_free_block(size_t size) {
    heap_header_t* current = free_list;
    
    while (current) {
        if (!current->used && current->size >= size) {
            return current;
        }
        current = current->next;
    }
    
    return NULL;
}

static void split_block(heap_header_t* block, size_t size) {
    // If block is too small or exactly fits, don't split
    if (block->size <= size + sizeof(heap_header_t)) {
        return;
    }

    size_t remaining = block->size - size - sizeof(heap_header_t);

    // Only split if the remainder is big enough to be useful
    if (remaining <= sizeof(heap_header_t) + 16) {
        return;
    }

    heap_header_t* new_block =
        (heap_header_t*)((uint8_t*)block + sizeof(heap_header_t) + size);

    new_block->magic = HEAP_MAGIC;
    new_block->size  = remaining;
    new_block->used  = 0;
    new_block->next  = block->next;
    new_block->prev  = block;

    if (block->next) {
        block->next->prev = new_block;
    }

    block->next = new_block;
    block->size = size;
}


static void coalesce_blocks(void) {
    heap_header_t* current = free_list;

    while (current) {
        heap_header_t* next = current->next;

        if (!current->used && next && !next->used) {

            // Compute the physical end of the current block
            uint8_t* current_end =
                (uint8_t*)current + sizeof(heap_header_t) + current->size;

            // Only merge if blocks are physically adjacent
            if ((heap_header_t*)current_end == next) {
                current->size += sizeof(heap_header_t) + next->size;
                current->next = next->next;

                if (current->next) {
                    current->next->prev = current;
                }

                continue;  // re-check current with its new next
            }
        }

        current = current->next;
    }
}


void* kmalloc(size_t size) {
    if (size == 0) return NULL;
    
    // Align size to HEAP_ALIGNMENT
    size = (size + HEAP_ALIGNMENT - 1) & ~(HEAP_ALIGNMENT - 1);
    
    // Try to find a free block
    heap_header_t* block = find_free_block(size);
    
    if (!block) {
        // No suitable block found, extend heap modestly
        size_t total_size = size + sizeof(heap_header_t);
        if (!heap_extend(total_size)) {
            serial_print("HEAP: kmalloc failed - out of memory\n");
            return NULL;
        }
        
        // Create a new free block at the end of the heap
        heap_header_t* new_block = (heap_header_t*)(heap_end - total_size);
        new_block->magic = HEAP_MAGIC;
        new_block->size  = total_size - sizeof(heap_header_t);
        new_block->used  = 0;
        new_block->next  = NULL;
        new_block->prev  = NULL;
        
        // Append to free list
        if (!free_list) {
            free_list = new_block;
        } else {
            heap_header_t* cur = free_list;
            while (cur->next) cur = cur->next;
            cur->next = new_block;
            new_block->prev = cur;
        }
        
        // Try again
        block = find_free_block(size);
        if (!block) {
            serial_print("HEAP: kmalloc failed even after extension\n");
            return NULL;
        }
    }
    
    // Mark block as used
    block->used = 1;
    
    // Split if there's enough space left
    split_block(block, size);
    
    heap_used_bytes += block->size;
    
    // Return pointer to data area
    return (void*)((uint8_t*)block + sizeof(heap_header_t));
}

void kfree(void* ptr) {
    if (!ptr) return;
    
    heap_header_t* block = (heap_header_t*)((uint8_t*)ptr - sizeof(heap_header_t));
    
    if (block->magic != HEAP_MAGIC) {
        serial_print("HEAP: kfree called with invalid pointer!\n");
        serial_print("  ptr=0x");
        serial_print_hex((uint64_t)ptr);
        serial_print(" magic=0x");
        serial_print_hex(block->magic);
        serial_print("\n");
        return;
    }
    
    if (!block->used) {
        serial_print("HEAP: kfree called on already freed block!\n");
        serial_print("  ptr=0x");
        serial_print_hex((uint64_t)ptr);
        serial_print("\n");
        return;
    }
    
    block->used = 0;
    heap_used_bytes -= block->size;
    
    // Coalesce adjacent free blocks
    coalesce_blocks();
}

size_t heap_used(void) {
    return heap_used_bytes;
}

size_t heap_free(void) {
    return heap_allocated - heap_used_bytes;
}

size_t heap_total(void) {
    return heap_allocated;
}

void heap_stats(void) {
    vga_print("\n=== HEAP STATS ===\n");
    vga_print("  Total allocated: ");
    vga_print_dec_cur(heap_allocated / 1024);
    vga_print(" KB\n");
    vga_print("  Used: ");
    vga_print_dec_cur(heap_used_bytes / 1024);
    vga_print(" KB\n");
    vga_print("  Free: ");
    vga_print_dec_cur((heap_allocated - heap_used_bytes) / 1024);
    vga_print(" KB\n");
    
    serial_print("\n=== HEAP STATS ===\n");
    serial_print("  Total allocated: ");
    serial_print_dec(heap_allocated / 1024);
    serial_print(" KB\n");
    serial_print("  Used: ");
    serial_print_dec(heap_used_bytes / 1024);
    serial_print(" KB\n");
    serial_print("  Free: ");
    serial_print_dec((heap_allocated - heap_used_bytes) / 1024);
    serial_print(" KB\n");
    
    // Count free blocks
    size_t free_blocks = 0;
    heap_header_t* current = free_list;
    while (current) {
        if (!current->used) free_blocks++;
        current = current->next;
    }
    
    vga_print("  Free blocks: ");
    vga_print_dec_cur(free_blocks);
    vga_print("\n");
    
    // Serial output for debugging
    serial_print("\n=== HEAP STATS ===\n");
    serial_print("  Total allocated: ");
    serial_print_dec(heap_allocated / 1024);
    serial_print(" KB\n");
    serial_print("  Used: ");
    serial_print_dec(heap_used_bytes / 1024);
    serial_print(" KB\n");
    serial_print("  Free: ");
    serial_print_dec((heap_allocated - heap_used_bytes) / 1024);
    serial_print(" KB\n");
    serial_print("  Free blocks: ");
    serial_print_dec(free_blocks);
    serial_print("\n");
    
    // List free blocks
    serial_print("  Free block sizes: ");
    current = free_list;
    while (current) {
        if (!current->used) {
            serial_print_dec(current->size / 1024);
            serial_print("KB ");
        }
        current = current->next;
    }
    serial_print("\n");
    serial_print("==================\n");
    vga_print("> ");
}
