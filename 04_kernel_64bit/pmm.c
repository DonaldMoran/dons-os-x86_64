#include "pmm.h"
#include "bootinfo.h"
#include "serial.h"
#include "string.h"

#define DBG 0  // Set to 0 to disable, 1 to enable
#define MAX_PHYS_MEM   (128ULL * 1024 * 1024)
#define MAX_PAGES      (MAX_PHYS_MEM / PAGE_SIZE)
#define BITMAP_SIZE    (MAX_PAGES / 8)

static uint8_t pmm_bitmap[BITMAP_SIZE];
static page_info_t pmm_page_info[MAX_PAGES];
static uint64_t pmm_total_pages = 0;
static uint64_t pmm_free_pages = 0;
static uint64_t pmm_max_physical = 0;

// Allocation zones:
// - LOW zone: kernel + page tables (bottom portion of free memory)
// - HIGH zone: user data + user page tables (top portion of free memory)
static uint64_t pmm_low_start_page = 0;
static uint64_t pmm_low_end_page   = 0;
static uint64_t pmm_high_start_page = 0;
static uint64_t pmm_high_end_page   = 0;

// Allocation pointers within zones
static uint64_t pmm_next_low_page = 0;      // For PAGE_TABLE and KERNEL (bottom-up)
static uint64_t pmm_next_high_page = 0;     // For USER_DATA and USER_PAGE_TABLE (top-down)

// Statistics by type
static uint64_t pages_by_type[PAGE_TYPE_COUNT] = {0};

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

// Get page type as string for debugging
static const char* page_type_string(page_type_t type) {
    switch(type) {
        case PAGE_FREE: return "FREE";
        case PAGE_KERNEL: return "KERNEL";
        case PAGE_PAGE_TABLE: return "PAGE_TABLE";
        case PAGE_USER_DATA: return "USER_DATA";
        case PAGE_USER_PAGE_TABLE: return "USER_PT";
        case PAGE_DEVICE: return "DEVICE";
        case PAGE_RESERVED: return "RESERVED";
        default: return "UNKNOWN";
    }
}

// Compute allocation zones: low (kernel/VMM) vs high (user)
// We split the free range into two disjoint pools so that
// page tables and user data can never draw from the same pages.
static void pmm_compute_zones(uint64_t max_pages) {
    uint64_t first_free = max_pages;
    uint64_t last_free  = 0;

    // Find global free range
    for (uint64_t page = 0; page < max_pages; page++) {
        if (!bitmap_test(page)) {
            if (page < first_free) first_free = page;
            if (page > last_free)  last_free  = page;
        }
    }

    if (first_free == max_pages) {
        // No free pages at all
        pmm_low_start_page = pmm_low_end_page = 0;
        pmm_high_start_page = pmm_high_end_page = 0;
        pmm_next_low_page = 0;
        pmm_next_high_page = 0;
        serial_print("PMM: WARNING - No free pages to compute zones\n");
        return;
    }

    // Simple split: bottom 25% of free range for kernel/VMM,
    // top 75% for user. This scales with RAM size and keeps
    // zones disjoint.
    uint64_t span = last_free - first_free;
    uint64_t low_span = span / 4; // 25% for kernel/VMM

    pmm_low_start_page  = first_free;
    pmm_low_end_page    = first_free + low_span;

    // Ensure at least one page in low zone
    if (pmm_low_end_page < pmm_low_start_page) {
        pmm_low_end_page = pmm_low_start_page;
    }

    pmm_high_start_page = pmm_low_end_page + 1;
    pmm_high_end_page   = last_free;

    // If high zone collapses, give everything to low (degenerate case)
    if (pmm_high_start_page > pmm_high_end_page) {
        pmm_high_start_page = pmm_high_end_page = 0;
    }

    pmm_next_low_page  = pmm_low_start_page;
    pmm_next_high_page = pmm_high_end_page;

    serial_print("PMM: Zones computed:\n");
    serial_print("  LOW  zone: pages ");
    serial_print_dec(pmm_low_start_page);
    serial_print(" - ");
    serial_print_dec(pmm_low_end_page);
    serial_print("\n");
    serial_print("  HIGH zone: pages ");
    serial_print_dec(pmm_high_start_page);
    serial_print(" - ");
    serial_print_dec(pmm_high_end_page);
    serial_print("\n");
}

void pmm_init(BootInfo *info) {
    serial_print("PMM: Initializing with page type system...\n");

    // Initialize bitmap - mark all pages as used (1)
    for (uint64_t i = 0; i < BITMAP_SIZE; i++) {
        pmm_bitmap[i] = 0xFF;
    }

    // Initialize page info
    for (uint64_t i = 0; i < MAX_PAGES; i++) {
        pmm_page_info[i].phys_addr = i * PAGE_SIZE;
        pmm_page_info[i].type = PAGE_RESERVED;
        pmm_page_info[i].ref_count = 0;
        pmm_page_info[i].owner_pid = 0;
        pmm_page_info[i].owner_virt = 0;
    }
    memset(pages_by_type, 0, sizeof(pages_by_type));

    pmm_total_pages = 0;
    pmm_free_pages = 0;
    pmm_max_physical = 0;
    pmm_next_low_page = 0;
    pmm_next_high_page = 0;
    pmm_low_start_page = pmm_low_end_page = 0;
    pmm_high_start_page = pmm_high_end_page = 0;

    if (!info || info->memory_map_count == 0) {
        serial_print("PMM: No memory map! Using fallback.\n");
        pmm_max_physical = 128ULL * 1024 * 1024;
        uint64_t max_pages = pmm_max_physical / PAGE_SIZE;
        for (uint64_t addr = 0; addr < pmm_max_physical; addr += PAGE_SIZE) {
            uint64_t page = addr / PAGE_SIZE;
            if (page < MAX_PAGES) {
                if (addr < 16ULL * 1024 * 1024) {
                    bitmap_set(page);
                    pmm_page_info[page].type = PAGE_KERNEL;
                    pages_by_type[PAGE_KERNEL]++;
                } else {
                    bitmap_clear(page);
                    pmm_page_info[page].type = PAGE_FREE;
                    pmm_total_pages++;
                    pmm_free_pages++;
                    pages_by_type[PAGE_FREE]++;
                }
            }
        }

        // In fallback, treat bottom quarter as kernel zone, top as user zone
        pmm_compute_zones(max_pages);

        serial_print("PMM: Fallback initialization complete\n");
        return;
    }

    MemoryMapEntry *m = (MemoryMapEntry *)info->memory_map_addr;

    // Find max physical memory
    for (uint64_t i = 0; i < info->memory_map_count; i++) {
        uint64_t end = m[i].base + m[i].length;
        if (end > pmm_max_physical) {
            pmm_max_physical = end;
        }
    }

    // Limit to MAX_PHYS_MEM for safety
    if (pmm_max_physical > MAX_PHYS_MEM) {
        pmm_max_physical = MAX_PHYS_MEM;
    }

    uint64_t max_pages = pmm_max_physical / PAGE_SIZE;
    if (max_pages > MAX_PAGES) max_pages = MAX_PAGES;

    // Mark ALL pages as used first
    for (uint64_t i = 0; i < max_pages; i++) {
        bitmap_set(i);
        pmm_page_info[i].type = PAGE_RESERVED;
    }

    // Mark usable memory as free based on memory map
    for (uint64_t i = 0; i < info->memory_map_count; i++) {
        if (m[i].type != 1) {
            continue;
        }

        uint64_t base = m[i].base;
        uint64_t length = m[i].length;
        uint64_t end = base + length;

        base = (base / PAGE_SIZE) * PAGE_SIZE;
        end = ((end + PAGE_SIZE - 1) / PAGE_SIZE) * PAGE_SIZE;

        for (uint64_t addr = base; addr < end && addr < pmm_max_physical; addr += PAGE_SIZE) {
            uint64_t page = addr / PAGE_SIZE;
            if (page < max_pages) {
                if (bitmap_test(page)) {
                    bitmap_clear(page);
                    pmm_page_info[page].type = PAGE_FREE;
                    pmm_total_pages++;
                    pmm_free_pages++;
                    pages_by_type[PAGE_FREE]++;
                }
            }
        }
    }

    // Reserve the first 1MB (BIOS/legacy area)
    for (uint64_t addr = 0; addr < 0x100000; addr += PAGE_SIZE) {
        uint64_t page = addr / PAGE_SIZE;
        if (page < max_pages) {
            if (!bitmap_test(page)) {
                bitmap_set(page);
                pmm_page_info[page].type = PAGE_KERNEL;
                pmm_free_pages--;
                pages_by_type[PAGE_FREE]--;
                pages_by_type[PAGE_KERNEL]++;
            }
        }
    }

    // Reserve kernel memory
    if (info->kernel_phys_start && info->kernel_phys_end) {
        uint64_t kstart = info->kernel_phys_start;
        uint64_t kend = info->kernel_phys_end;

        kstart = (kstart / PAGE_SIZE) * PAGE_SIZE;
        kend = ((kend + PAGE_SIZE - 1) / PAGE_SIZE) * PAGE_SIZE;

        for (uint64_t addr = kstart; addr < kend && addr < pmm_max_physical; addr += PAGE_SIZE) {
            uint64_t page = addr / PAGE_SIZE;
            if (page < max_pages) {
                if (!bitmap_test(page)) {
                    bitmap_set(page);
                    pmm_page_info[page].type = PAGE_KERNEL;
                    pmm_free_pages--;
                    pages_by_type[PAGE_FREE]--;
                    pages_by_type[PAGE_KERNEL]++;
                }
            }
        }
    }

    // Reserve ACPI/other reserved regions
    for (uint64_t i = 0; i < info->memory_map_count; i++) {
        if (m[i].type == 2 || m[i].type == 3 || m[i].type == 4 || m[i].type == 5) {
            uint64_t base = m[i].base;
            uint64_t end = base + m[i].length;
            base = (base / PAGE_SIZE) * PAGE_SIZE;
            end = ((end + PAGE_SIZE - 1) / PAGE_SIZE) * PAGE_SIZE;
            for (uint64_t addr = base; addr < end && addr < pmm_max_physical; addr += PAGE_SIZE) {
                uint64_t page = addr / PAGE_SIZE;
                if (page < max_pages) {
                    if (!bitmap_test(page)) {
                        bitmap_set(page);
                        pmm_page_info[page].type = PAGE_RESERVED;
                        pmm_free_pages--;
                        pages_by_type[PAGE_FREE]--;
                        pages_by_type[PAGE_RESERVED]++;
                    }
                }
            }
        }
    }

    // Compute disjoint low/high zones from remaining free pages
    pmm_compute_zones(max_pages);

    serial_print("PMM: Max physical memory: 0x");
    serial_print_hex(pmm_max_physical);
    serial_print(" (");
    serial_print_dec(pmm_max_physical / (1024 * 1024));
    serial_print(" MB)\n");
    serial_print("PMM: Total pages: ");
    serial_print_dec(pmm_total_pages);
    serial_print("\nPMM: Free pages: ");
    serial_print_dec(pmm_free_pages);
    serial_print("\nPMM: Free memory: ");
    serial_print_dec(pmm_free_pages * PAGE_SIZE / (1024 * 1024));
    serial_print(" MB\n");
}

//~ uint64_t pmm_alloc_page(page_type_t type) {
    //~ uint64_t max_pages = pmm_max_physical / PAGE_SIZE;
    //~ if (max_pages > MAX_PAGES) max_pages = MAX_PAGES;

    //~ // USER_DATA and USER_PAGE_TABLE: allocate strictly from HIGH zone (top of free memory)
    //~ if (type == PAGE_USER_DATA || type == PAGE_USER_PAGE_TABLE) {
        //~ if (pmm_high_start_page == 0 && pmm_high_end_page == 0) {
            //~ serial_print("PMM: ERROR - No HIGH zone for user allocations\n");
            //~ return 0;
        //~ }

        //~ for (uint64_t page = pmm_next_high_page; page >= pmm_high_start_page; page--) {
            //~ if (!bitmap_test(page)) {
                //~ // Found a free page in HIGH zone
                //~ bitmap_set(page);
                //~ pmm_free_pages--;
                //~ pmm_next_high_page = (page > pmm_high_start_page) ? (page - 1) : pmm_high_start_page;

                //~ pmm_page_info[page].type = type;
                //~ pmm_page_info[page].ref_count = 1;
                //~ pmm_page_info[page].owner_pid = 0;
                //~ pmm_page_info[page].owner_virt = 0;

                //~ pages_by_type[PAGE_FREE]--;
                //~ pages_by_type[type]++;

                //~ serial_print("PMM: ALLOC [USER] type=");
                //~ serial_print(page_type_string(type));
                //~ serial_print(" page=0x");
                //~ serial_print_hex(page * PAGE_SIZE);
                //~ serial_print(" (free: ");
                //~ serial_print_dec(pmm_free_pages);
                //~ serial_print(")\n");

                //~ return page * PAGE_SIZE;
            //~ }

            //~ if (page == pmm_high_start_page) {
                //~ break;
            //~ }
        //~ }

        //~ // No fallback into LOW zone: strict separation
        //~ serial_print("PMM: ERROR - Out of HIGH-zone memory for user type ");
        //~ serial_print(page_type_string(type));
        //~ serial_print("! Free pages: ");
        //~ serial_print_dec(pmm_free_pages);
        //~ serial_print("\n");
        //~ return 0;
    //~ } 
    //~ // PAGE_TABLE, KERNEL, RESERVED: allocate strictly from LOW zone (bottom of free memory)
    //~ else {
        //~ if (pmm_low_start_page == 0 && pmm_low_end_page == 0) {
            //~ serial_print("PMM: ERROR - No LOW zone for kernel/table allocations\n");
            //~ return 0;
        //~ }

        //~ for (uint64_t page = pmm_next_low_page; page <= pmm_low_end_page; page++) {
            //~ if (!bitmap_test(page)) {
                //~ // Found a free page in LOW zone
                //~ bitmap_set(page);
                //~ pmm_free_pages--;
                //~ pmm_next_low_page = (page < pmm_low_end_page) ? (page + 1) : pmm_low_end_page;

                //~ pmm_page_info[page].type = type;
                //~ pmm_page_info[page].ref_count = 1;
                //~ pmm_page_info[page].owner_pid = 0;
                //~ pmm_page_info[page].owner_virt = 0;

                //~ pages_by_type[PAGE_FREE]--;
                //~ pages_by_type[type]++;

                //~ serial_print("PMM: ALLOC [TABLE/KERNEL] type=");
                //~ serial_print(page_type_string(type));
                //~ serial_print(" page=0x");
                //~ serial_print_hex(page * PAGE_SIZE);
                //~ serial_print(" (free: ");
                //~ serial_print_dec(pmm_free_pages);
                //~ serial_print(")\n");

                //~ return page * PAGE_SIZE;
            //~ }

            //~ if (page == pmm_low_end_page) {
                //~ break;
            //~ }
        //~ }

        //~ // No fallback into HIGH zone: strict separation
        //~ serial_print("PMM: ERROR - Out of LOW-zone memory for kernel/table type ");
        //~ serial_print(page_type_string(type));
        //~ serial_print("! Free pages: ");
        //~ serial_print_dec(pmm_free_pages);
        //~ serial_print("\n");
        //~ return 0;
    //~ }
//~ }
uint64_t pmm_alloc_page(page_type_t type) {
    uint64_t max_pages = pmm_max_physical / PAGE_SIZE;
    if (max_pages > MAX_PAGES) max_pages = MAX_PAGES;

    // USER_DATA and USER_PAGE_TABLE: allocate strictly from HIGH zone (top of free memory)
    if (type == PAGE_USER_DATA || type == PAGE_USER_PAGE_TABLE) {
        if (pmm_high_start_page == 0 && pmm_high_end_page == 0) {
            serial_print("PMM: ERROR - No HIGH zone for user allocations\n");
            return 0;
        }

        for (uint64_t page = pmm_next_high_page; page >= pmm_high_start_page; page--) {
            if (!bitmap_test(page)) {
                // Found a free page in HIGH zone
                bitmap_set(page);
                pmm_free_pages--;
                pmm_next_high_page = (page > pmm_high_start_page) ? (page - 1) : pmm_high_start_page;

                pmm_page_info[page].type = type;
                pmm_page_info[page].ref_count = 1;
                pmm_page_info[page].owner_pid = 0;
                pmm_page_info[page].owner_virt = 0;

                pages_by_type[PAGE_FREE]--;
                pages_by_type[type]++;
                
                if (DBG) {
                    serial_print("PMM: ALLOC [USER] type=");
                    serial_print(page_type_string(type));
                    serial_print(" page=0x");
                    serial_print_hex(page * PAGE_SIZE);
                    serial_print(" (free: ");
                    serial_print_dec(pmm_free_pages);
                    serial_print(")\n");
				}

                return page * PAGE_SIZE;
            }

            if (page == pmm_high_start_page) {
                break;
            }
        }

        // No fallback into LOW zone: strict separation
        serial_print("PMM: ERROR - Out of HIGH-zone memory for user type ");
        serial_print(page_type_string(type));
        serial_print("! Free pages: ");
        serial_print_dec(pmm_free_pages);
        serial_print("\n");
        return 0;
    } 

    // PAGE_TABLE, KERNEL, RESERVED: allocate strictly from LOW zone (bottom of free memory)
    else {
        if (pmm_low_start_page == 0 && pmm_low_end_page == 0) {
            serial_print("PMM: ERROR - No LOW zone for kernel/table allocations\n");
            return 0;
        }

        for (uint64_t page = pmm_next_low_page; page <= pmm_low_end_page; page++) {

            uint64_t phys = page * PAGE_SIZE;

            // ⭐ NEW SAFETY CHECK ⭐
            // Avoid allocating LOW-zone pages below 2MB for kernel/table/reserved.
            // This prevents heap pages from overlapping kernel .data/.bss (including g_bootinfo).
            if (type == PAGE_KERNEL || type == PAGE_PAGE_TABLE || type == PAGE_RESERVED) {
                if (phys < 0x200000) {
                    continue;   // skip pages too close to kernel image/globals
                }
            }

            if (!bitmap_test(page)) {
                // Found a free page in LOW zone
                bitmap_set(page);
                pmm_free_pages--;
                pmm_next_low_page = (page < pmm_low_end_page) ? (page + 1) : pmm_low_end_page;

                pmm_page_info[page].type = type;
                pmm_page_info[page].ref_count = 1;
                pmm_page_info[page].owner_pid = 0;
                pmm_page_info[page].owner_virt = 0;

                pages_by_type[PAGE_FREE]--;
                pages_by_type[type]++;

                if (DBG) {
                    serial_print("PMM: ALLOC [TABLE/KERNEL] type=");
                    serial_print(page_type_string(type));
                    serial_print(" page=0x");
                    serial_print_hex(phys);
                    serial_print(" (free: ");
                    serial_print_dec(pmm_free_pages);
                    serial_print(")\n");
				}

                return phys;
            }

            if (page == pmm_low_end_page) {
                break;
            }
        }

        // No fallback into HIGH zone: strict separation
        serial_print("PMM: ERROR - Out of LOW-zone memory for kernel/table type ");
        serial_print(page_type_string(type));
        serial_print("! Free pages: ");
        serial_print_dec(pmm_free_pages);
        serial_print("\n");
        return 0;
    }
}

void pmm_free_page(uint64_t phys_addr) {
    if (phys_addr == 0) return;

    uint64_t page = phys_addr / PAGE_SIZE;
    if (page >= MAX_PAGES) return;

    if (!bitmap_test(page)) {
        serial_print("PMM: WARNING - Double free of page 0x");
        serial_print_hex(phys_addr);
        serial_print("\n");
        return;
    }

    // Decrement ref count
    if (pmm_page_info[page].ref_count > 0) {
        pmm_page_info[page].ref_count--;
    }

    // Only free if ref_count reaches 0
    if (pmm_page_info[page].ref_count == 0) {
        page_type_t old_type = pmm_page_info[page].type;
        bitmap_clear(page);
        pmm_page_info[page].type = PAGE_FREE;
        pmm_free_pages++;
        pages_by_type[old_type]--;
        pages_by_type[PAGE_FREE]++;

        serial_print("PMM: FREE page 0x");
        serial_print_hex(phys_addr);
        serial_print(" (was ");
        serial_print(page_type_string(old_type));
        serial_print(", free: ");
        serial_print_dec(pmm_free_pages);
        serial_print(")\n");

        // Update allocation pointers if needed, but keep them within zones
        if (page >= pmm_low_start_page && page <= pmm_low_end_page) {
            if (page < pmm_next_low_page) {
                pmm_next_low_page = page;
            }
        }
        if (page >= pmm_high_start_page && page <= pmm_high_end_page) {
            if (page > pmm_next_high_page) {
                pmm_next_high_page = page;
            }
        }
    } else {
        serial_print("PMM: REFCOUNT page 0x");
        serial_print_hex(phys_addr);
        serial_print(" now ");
        serial_print_dec(pmm_page_info[page].ref_count);
        serial_print("\n");
    }
}

page_type_t pmm_get_page_type(uint64_t phys_addr) {
    uint64_t page = phys_addr / PAGE_SIZE;
    if (page >= MAX_PAGES) return PAGE_RESERVED;
    return pmm_page_info[page].type;
}

int pmm_can_use_as_page_table(uint64_t phys_addr) {
    uint64_t page = phys_addr / PAGE_SIZE;
    if (page >= MAX_PAGES) return 0;

    page_type_t type = pmm_get_page_type(phys_addr);

    // Only pages that are currently FREE and lie in the LOW zone
    // are allowed to become page tables. This prevents user-data
    // pages from ever being reused as page tables.
    if (type != PAGE_FREE) {
        return 0;
    }
    if (page < pmm_low_start_page || page > pmm_low_end_page) {
        return 0;
    }
    return 1;
}

void pmm_reserve_page(uint64_t phys_addr) {
    uint64_t page = phys_addr / PAGE_SIZE;
    if (page >= MAX_PAGES) return;

    if (bitmap_test(page)) {
        page_type_t old_type = pmm_page_info[page].type;
        pmm_page_info[page].type = PAGE_RESERVED;
        pages_by_type[old_type]--;
        pages_by_type[PAGE_RESERVED]++;
        serial_print("PMM: RESERVED page 0x");
        serial_print_hex(phys_addr);
        serial_print(" (was ");
        serial_print(page_type_string(old_type));
        serial_print(")\n");
    } else {
        bitmap_set(page);
        pmm_page_info[page].type = PAGE_RESERVED;
        pmm_page_info[page].ref_count = 1;
        pmm_free_pages--;
        pages_by_type[PAGE_FREE]--;
        pages_by_type[PAGE_RESERVED]++;
        serial_print("PMM: RESERVED free page 0x");
        serial_print_hex(phys_addr);
        serial_print("\n");
    }
}

void pmm_unreserve_page(uint64_t phys_addr) {
    uint64_t page = phys_addr / PAGE_SIZE;
    if (page >= MAX_PAGES) return;

    if (pmm_page_info[page].type == PAGE_RESERVED) {
        pmm_page_info[page].type = PAGE_FREE;
        bitmap_clear(page);
        pmm_free_pages++;
        pages_by_type[PAGE_RESERVED]--;
        pages_by_type[PAGE_FREE]++;
        serial_print("PMM: UNRESERVED page 0x");
        serial_print_hex(phys_addr);
        serial_print("\n");

        // Adjust zone pointers if this free page lies in a zone
        if (page >= pmm_low_start_page && page <= pmm_low_end_page) {
            if (page < pmm_next_low_page) {
                pmm_next_low_page = page;
            }
        }
        if (page >= pmm_high_start_page && page <= pmm_high_end_page) {
            if (page > pmm_next_high_page) {
                pmm_next_high_page = page;
            }
        }
    }
}

uint64_t pmm_get_free_pages(void) {
    return pmm_free_pages;
}

uint64_t pmm_get_total_pages(void) {
    return pmm_total_pages;
}

void pmm_dump_stats(void) {
    serial_print("\n=== PMM STATS ===\n");
    serial_print("Total pages: ");
    serial_print_dec(pmm_total_pages);
    serial_print("\nFree pages: ");
    serial_print_dec(pmm_free_pages);
    serial_print("\n");
    serial_print("Pages by type:\n");
    serial_print("  FREE: ");
    serial_print_dec(pages_by_type[PAGE_FREE]);
    serial_print("\n  KERNEL: ");
    serial_print_dec(pages_by_type[PAGE_KERNEL]);
    serial_print("\n  PAGE_TABLE: ");
    serial_print_dec(pages_by_type[PAGE_PAGE_TABLE]);
    serial_print("\n  USER_DATA: ");
    serial_print_dec(pages_by_type[PAGE_USER_DATA]);
    serial_print("\n  USER_PAGE_TABLE: ");
    serial_print_dec(pages_by_type[PAGE_USER_PAGE_TABLE]);
    serial_print("\n  RESERVED: ");
    serial_print_dec(pages_by_type[PAGE_RESERVED]);
    serial_print("\n");
    serial_print("Allocation pointers:\n");
    serial_print("  LOW  zone: ");
    serial_print_dec(pmm_low_start_page);
    serial_print(" - ");
    serial_print_dec(pmm_low_end_page);
    serial_print("\n  HIGH zone: ");
    serial_print_dec(pmm_high_start_page);
    serial_print(" - ");
    serial_print_dec(pmm_high_end_page);
    serial_print("\n  Next low:  ");
    serial_print_dec(pmm_next_low_page);
    serial_print("\n  Next high: ");
    serial_print_dec(pmm_next_high_page);
    serial_print("\n");
    serial_print("==================\n\n");
}
