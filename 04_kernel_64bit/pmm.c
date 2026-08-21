#include "pmm.h"
#include "bootinfo.h"
#include "serial.h"

#define MAX_PHYS_MEM   (128ULL * 1024 * 1024)
#define MAX_PAGES      (MAX_PHYS_MEM / PAGE_SIZE)
#define BITMAP_SIZE    (MAX_PAGES / 8)

static uint8_t pmm_bitmap[BITMAP_SIZE];
static uint64_t pmm_total_pages = 0;
static uint64_t pmm_free_pages = 0;

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
    for (uint64_t i = 0; i < BITMAP_SIZE; i++) {
        ((volatile uint8_t *)pmm_bitmap)[i] = 0xFF;
    }

    pmm_total_pages = 0;
    pmm_free_pages = 0;

    MemoryMapEntry *m = (MemoryMapEntry *)info->memory_map_addr;

    for (uint64_t i = 0; i < info->memory_map_count; i++) {
        if (m[i].type != 1) {
            continue;
        }

        uint64_t base = m[i].base;
        uint64_t length = m[i].length;
        uint64_t end = base + length;

        for (uint64_t addr = base; addr < end; addr += PAGE_SIZE) {
            uint64_t page = addr / PAGE_SIZE;
            if (page < MAX_PAGES) {
                if (bitmap_test(page)) {
                    bitmap_clear(page);
                    pmm_total_pages++;
                    pmm_free_pages++;
                }
            }
        }
    }

    for (uint64_t addr = 0; addr < 0x100000; addr += PAGE_SIZE) {
        uint64_t page = addr / PAGE_SIZE;
        if (page < MAX_PAGES) {
            if (!bitmap_test(page)) {
                bitmap_set(page);
                pmm_free_pages--;
            }
        }
    }

    uint64_t kstart = info->kernel_phys_start;
    uint64_t kend   = info->kernel_phys_end;

    for (uint64_t addr = kstart; addr < kend; addr += PAGE_SIZE) {
        uint64_t page = addr / PAGE_SIZE;
        if (page < MAX_PAGES) {
            if (!bitmap_test(page)) {
                bitmap_set(page);
                pmm_free_pages--;
            }
        }
    }

    serial_print("PMM: Total pages: ");
    serial_print_dec(pmm_total_pages);
    serial_print("\nPMM: Free pages: ");
    serial_print_dec(pmm_free_pages);
    serial_print("\n");
}

uint64_t pmm_alloc_page(void) {
    for (uint64_t page = MAX_PAGES - 1; page > 0; page--) {
        if (!bitmap_test(page)) {
            bitmap_set(page);
            pmm_free_pages--;
            return page * PAGE_SIZE;
        }
    }
    for (uint64_t page = 0; page < MAX_PAGES; page++) {
        if (!bitmap_test(page)) {
            bitmap_set(page);
            pmm_free_pages--;
            return page * PAGE_SIZE;
        }
    }
    return 0;
}

void pmm_free_page(uint64_t phys_addr) {
    uint64_t page = phys_addr / PAGE_SIZE;
    if (page >= MAX_PAGES) return;

    if (!bitmap_test(page)) {
        return;
    }

    bitmap_clear(page);
    pmm_free_pages++;
}

uint64_t pmm_get_free_pages(void) {
    return pmm_free_pages;
}

uint64_t pmm_get_total_pages(void) {
    return pmm_total_pages;
}
