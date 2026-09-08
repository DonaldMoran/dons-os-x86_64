#ifndef HEAP_H
#define HEAP_H

#include <stddef.h>
#include <stdint.h>

// LOW zone is ~32MB total; kernel + page tables need a big chunk.
// Keep initial heap modest and allow controlled growth.
// #define HEAP_INITIAL_SIZE (8ULL * 1024ULL * 1024ULL)   // 8MB initial heap
#define HEAP_START        0xFFFF900000000000ULL     // Base of global heap (HIGH zone)
#define HEAP_INITIAL_SIZE (1ULL * 1024ULL * 1024ULL)   // 1 MB initial heap
#define HEAP_MAGIC        0xDEADBEEFCAFEBABEULL
#define HEAP_ALIGNMENT    16

typedef struct heap_header {
    uint64_t magic;
    size_t size;
    int used;
    struct heap_header* next;
    struct heap_header* prev;
} heap_header_t;

void heap_init(uint64_t start, size_t size);
void* kmalloc(size_t size);
void kfree(void* ptr);
void heap_stats(void);
size_t heap_used(void);
size_t heap_free(void);
size_t heap_total(void);

#endif
