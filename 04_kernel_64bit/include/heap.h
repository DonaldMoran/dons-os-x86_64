#ifndef HEAP_H
#define HEAP_H

#include <stddef.h>
#include <stdint.h>

/*
 * Global kernel heap.
 *
 * Layout: a singly-linked list of blocks, in address order.  Each
 * block is preceded by a heap_header_t.  A block's payload is the
 * memory immediately after its header; the payload size is
 * header->size, rounded up to HEAP_ALIGNMENT at allocation time.
 *
 * Invariants (checked by heap_validate):
 *   - Every block's magic == HEAP_MAGIC.
 *   - Every block's used is 0 or 1.
 *   - Blocks are in strictly increasing address order.
 *   - block->next == block + sizeof(heap_header_t) + block->size,
 *     for every block with a next.
 *   - The last block's end == heap_brk (all mapped memory is
 *     accounted for; no gaps, no leaks).
 *   - Every block lies within [heap_start, heap_brk).
 *   - block->next->prev == block, for every block with a next.
 *
 * The header is padded to 48 bytes so that payloads are 16-aligned
 * when size is a multiple of HEAP_ALIGNMENT.  This matters: newlib's
 * malloc is documented to return 16-aligned pointers on x86-64, and
 * code compiled with SSE may assume it.  A 40-byte header would make
 * payload addresses alternate between 8- and 16-aligned.
 *
 * Heap backing pages come from the PMM's HIGH zone (PAGE_USER_DATA,
 * via pmm_alloc_page_for_elf).  The heap is mapped into the kernel's
 * higher-half address space with PT_WRITE and no PT_USER, so users
 * cannot reach it; the PMM type is a zone tag, not a privilege tag.
 * Using the LOW zone (PAGE_KERNEL) instead would starve the kernel's
 * page-table and kernel-stack allocations, because LOW is only one
 * quarter of free physical memory.
 */

#define HEAP_START        0xFFFF900000000000ULL
#define HEAP_INITIAL_SIZE (1ULL * 1024ULL * 1024ULL)   /* 1 MB */
#define HEAP_MAX_SIZE     (24ULL * 1024ULL * 1024ULL)  /* 24 MB */
#define HEAP_MAGIC        0xDEADBEEFCAFEBABEULL
#define HEAP_ALIGNMENT    16

typedef struct heap_header {
    uint64_t magic;
    size_t   size;          /* payload size in bytes, 16-aligned */
    uint64_t used;          /* 0 = free, 1 = allocated */
    struct heap_header* next;
    struct heap_header* prev;
    uint64_t _pad;          /* pad to 48 bytes so payloads are 16-aligned */
} heap_header_t;

void   heap_init(uint64_t start, size_t size);
void*  kmalloc(size_t size);
void   kfree(void* ptr);
void   heap_stats(void);
size_t heap_used(void);
size_t heap_free(void);
size_t heap_total(void);

/*
 * Walk the block list and check every invariant.  Returns 0 if the
 * heap is intact, or the address of the first block whose invariants
 * failed (nonzero).  Does not allocate, so it is safe to call when
 * the heap is suspected of being corrupt.
 */
uint64_t heap_validate(void);

/*
 * Allocate and free blocks in a deterministic pattern, verify
 * contents, force at least one extension, and run heap_validate at
 * the end.  Returns 0 on success, or a nonzero sentinel on failure
 * (content mismatch, allocation failure, validator failure, or leak).
 * Prints diagnostics to serial on failure only.
 */
size_t heap_stress(void);

#endif /* HEAP_H */
