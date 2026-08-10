// heap.h - Heap allocator interface
#pragma once
#include <stdint.h>
#include <stddef.h>

void heap_init(void);
void* kmalloc(size_t size);
void kfree(void* ptr);
void* krealloc(void* ptr, size_t new_size);

void heap_stats(void);
size_t heap_used(void);
size_t heap_free(void);
size_t heap_total(void);

// Heap is at 0xFFFF900000000000 (NOT HHDM!)
#define HEAP_START 0xFFFF900000000000ULL
#define HEAP_INITIAL_SIZE (64ULL * 1024ULL * 1024ULL)
