#include "pmm.h"

#define MAX_PHYS_MEM   (128ULL * 1024 * 1024)   // support up to 512 MB for now
#define MAX_PAGES      (MAX_PHYS_MEM / PAGE_SIZE)
#define BITMAP_SIZE    (MAX_PAGES / 8)

static uint8_t pmm_bitmap[BITMAP_SIZE];
static uint64_t pmm_total_pages = 0;

static void bitmap_set(uint64_t page) {
    uint64_t byte = page / 8;
    uint8_t bit = page % 8;
    pmm_bitmap[byte] |= (1 << bit);
}

static void bitmap_clear(uint64_t page) {
    uint64_t byte = page / 8;
    uint8_t bit = page % 8;
    pmm_bitmap[byte] &= ~(1 << bit);
}

static int bitmap_test(uint64_t page) {
    uint64_t byte = page / 8;
    uint8_t bit = page % 8;
    return (pmm_bitmap[byte] >> bit) & 1;
}

void pmm_init(BootInfo *info) {
    // mark everything used by default — force simple byte loop
    for (uint64_t i = 0; i < BITMAP_SIZE; i++) {
        ((volatile uint8_t *)pmm_bitmap)[i] = 0xFF;
    }

    // ...rest of your code unchanged...

    MemoryMapEntry *m = (MemoryMapEntry *)info->memory_map_addr;

    // mark usable regions as free
    for (uint64_t i = 0; i < info->memory_map_count; i++) {
        if (m[i].type != 1) {
            continue;
        }

        uint64_t base = m[i].base;
        uint64_t length = m[i].length;
        uint64_t end = base + length;

        // walk pages in this region
        for (uint64_t addr = base; addr < end; addr += PAGE_SIZE) {
            uint64_t page = addr / PAGE_SIZE;
            if (page < MAX_PAGES) {
                bitmap_clear(page);   // mark as free
                pmm_total_pages++;
            }
        }
    }

    // reserve first 1 MiB
    for (uint64_t addr = 0; addr < 0x100000; addr += PAGE_SIZE) {
        uint64_t page = addr / PAGE_SIZE;
        if (page < MAX_PAGES) {
            bitmap_set(page);
        }
    }

    // reserve kernel range
    uint64_t kstart = info->kernel_phys_start;
    uint64_t kend   = info->kernel_phys_end;

    for (uint64_t addr = kstart; addr < kend; addr += PAGE_SIZE) {
        uint64_t page = addr / PAGE_SIZE;
        if (page < MAX_PAGES) {
            bitmap_set(page);
        }
    }
}

uint64_t pmm_alloc_page(void) {
    // Allocate from the END of memory (high addresses first)
    for (uint64_t page = MAX_PAGES - 1; page > 0; page--) {
        uint64_t phys = page * PAGE_SIZE;
        if (!bitmap_test(page)) {
            bitmap_set(page);
            return phys;
        }
    }
    // Fallback: scan from beginning
    for (uint64_t page = 0; page < MAX_PAGES; page++) {
        if (!bitmap_test(page)) {
            bitmap_set(page);
            return page * PAGE_SIZE;
        }
    }
    return 0; // out of memory
}


void pmm_free_page(uint64_t phys_addr) {
    uint64_t page = phys_addr / PAGE_SIZE;
    if (page < MAX_PAGES) {
        bitmap_clear(page);
    }
}
