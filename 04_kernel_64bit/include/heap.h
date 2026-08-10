// Comment
#pragma once
#include <stdint.h>
#include <stddef.h>

// Heap functions
void heap_init(void);
void* kmalloc(size_t size);
void kfree(void* ptr);
void* krealloc(void* ptr, size_t new_size);

// Debug functions
void heap_stats(void);
size_t heap_used(void);
size_t heap_free(void);
size_t heap_total(void);

// Heap constants
#define HEAP_START 0xFFFF900000000000  // Start of kernel heap
#define HEAP_INITIAL_SIZE (64 * 1024 * 1024)  // 64 MB initial heap
