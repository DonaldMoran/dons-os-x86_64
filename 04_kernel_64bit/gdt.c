#include "include/gdt.h"
#include "include/tss.h"
#include "include/serial.h"
#include <stdint.h>
#include "include/vga.h"

/*
 * The kernel's own GDT.
 *
 * Before this change, the kernel used the bootloader's GDT, which
 * lives in low memory (physical 0x101DC, inside stage2.asm's loaded
 * image).  sgdt reported that address, and every GDT-dependent
 * operation — ltr in tss_init, the segment reloads in
 * gdt_fix_user_segments, the descriptor walks in isr13_handler and
 * gdt_dump — read through the bootloader's identity map to reach it.
 *
 * That worked because the identity map happens to cover low memory.
 * It was fragile for the same reason the old hardcoded boot stack was
 * fragile (item 3a): nothing guaranteed the bootloader's memory
 * stayed mapped, and if the identity map were ever narrowed, the
 * first ltr would fault with a non-obvious cause.  See MAINTENANCE.md
 * item 3h.
 *
 * kernel_gdt moves the table into the kernel's own .bss, at a known
 * higher-half address that the kernel controls.  gdt_init builds it,
 * loads it with lgdt, and reloads the segment registers.  From that
 * point on, sgdt returns the higher-half address and every consumer
 * — including gdt_dump and tss_init — reads from the kernel's own
 * table.
 *
 * 16 entries, not 8: the current layout uses indices 0..7, but the
 * extra slots cost 128 bytes and leave room for future descriptors
 * without changing the array size or the alignment guarantee.
 *
 * 16-byte aligned: the SDM does not require GDT alignment, but 16
 * matches the alignment the linker applies to .bss and keeps the
 * descriptor addressing simple to reason about.  No code depends on
 * this; it is a convention.
 */
static uint64_t kernel_gdt[16] __attribute__((aligned(16)));

/* Number of entries actually populated by gdt_init.  Used by the
 * gdtr's limit field.  Indices 8..15 are unused and read as zero if
 * anything walks past index 7. */
#define GDT_ENTRIES 9

/*
 * Build a code or data segment descriptor in the legacy 8-byte form.
 *
 *   base   : 32-bit base address (0 for flat segments)
 *   limit  : 20-bit limit (0xFFFFF for flat segments, with G=1)
 *   access : access byte (P | DPL | S | type)
 *   flags  : flags nibble (G | D/B | L | AVL)
 *
 * The encoder is not general — it assumes a flat 32-bit base of 0
 * and a limit that fits in 20 bits, which is all this kernel uses.
 * If a future descriptor needs a non-zero base (e.g. an LDT or a
 * non-flat segment), this function must be extended.
 */
static uint64_t gdt_encode_segment(uint32_t base, uint32_t limit,
                                   uint8_t access, uint8_t flags)
{
    uint64_t d = 0;
    d |= (uint64_t)(limit & 0xFFFF);            /* limit[15:0]  */
    d |= (uint64_t)(base & 0xFFFFFF) << 16;     /* base[23:0]   */
    d |= (uint64_t)access << 40;                /* access byte  */
    d |= (uint64_t)((limit >> 16) & 0xF) << 48; /* limit[19:16] */
    d |= (uint64_t)(flags & 0xF) << 52;         /* flags nibble */
    d |= (uint64_t)((base >> 24) & 0xFF) << 56; /* base[31:24]  */
    return d;
}

/*
 * Load the GDT and reload the segment registers.
 *
 * The CS reload needs a far jump, because mov into CS is illegal.
 * The push/lretq sequence is the standard way to do it in long mode:
 * push the new CS and the target RIP, then lretq pops both.  The
 * local label 1 is where execution resumes, at the same virtual
 * address, with CS = 0x18.
 *
 * This must be called after gdt_init has populated kernel_gdt and
 * issued lgdt, and it must be called in C, not in entry.asm, because
 * the segment reload clobbers registers the C ABI cares about.
 */
void gdt_reload(void) {
    __asm__ volatile (
        "pushq $0x18\n\t"          /* kernel code selector */
        "leaq 1f(%%rip), %%rax\n\t"
        "pushq %%rax\n\t"
        "lretq\n\t"
        "1:\n\t"
        "mov $0x20, %%ax\n\t"      /* kernel data selector */
        "mov %%ax, %%ds\n\t"
        "mov %%ax, %%es\n\t"
        "mov %%ax, %%fs\n\t"
        "mov %%ax, %%gs\n\t"
        "mov %%ax, %%ss\n\t"
        : : : "rax", "memory"
    );
}

/*
 * Build the kernel's own GDT and load it.
 *
 * Descriptor layout, matching the constants in gdt.h:
 *
 *   [0] 0x00  null
 *   [1] 0x08  code32  (unused in long mode, kept for compatibility)
 *   [2] 0x10  data32  (unused in long mode, kept for compatibility)
 *   [3] 0x18  code64, DPL=0, L=1
 *   [4] 0x20  data64, DPL=0
 *   [5] 0x28  user data, DPL=3        (SS = 0x2B when RPL=3)
 *   [6] 0x30  user code, DPL=3, L=1   (CS = 0x33 when RPL=3)
 *   [7] 0x38  TSS (populated later by gdt_set_tss)
 *   [8] 0x40  unused
 *
 * Selector arithmetic, for the record:
 *   SYSCALL loads CS = STAR[47:32] = 0x18, SS = 0x18 + 8 = 0x20.
 *   SYSRET loads CS = STAR[63:48] + 16, SS = STAR[63:48] + 8.
 *   user_syscall_init sets STAR[63:48] = 0x23.
 *   So SYSRET loads CS = 0x23 + 16 = 0x33 (index 6, RPL 3)
 *   and SS = 0x23 + 8 = 0x2B (index 5, RPL 3).
 *
 * context_switch.asm pushes CS = 0x33, SS = 0x2B for user frames,
 * matching.  So index 5 must be user DATA and index 6 must be user
 * CODE.  (The bootloader's GDT had these reversed; gdt_fix_user_segments
 * corrected them at runtime.  gdt_init now writes the correct values
 * on the first pass, so the fix-up is no longer needed.)
 *
 * The TSS descriptor is left zero here because the TSS base is not
 * known until tss_init runs, which happens after this function
 * returns.  tss_init calls gdt_set_tss, which writes index 7
 * directly in kernel_gdt.  Because lgdt has already been issued and
 * the CPU reads descriptors through the loaded GDTR, the write to
 * kernel_gdt[7] takes effect without another lgdt.  (The CPU
 * re-reads the descriptor on every ltr; there is no descriptor
 * cache to invalidate.  See the SDM, LTR.)
 *
 * Called from kmain before idt_init.  Before this call, the CPU is
 * using the bootloader's GDT; after it, the CPU is using the kernel's.
 * The window between entry.asm's last GDT use and this call contains
 * no GDT-dependent instructions.
 */
void gdt_init(void) {
    /* [0] null descriptor. */
    kernel_gdt[0] = 0;

    /* [1] 0x08 code32, [2] 0x10 data32.
     * Legacy protected-mode segments, kept so the layout matches the
     * bootloader's GDT and the constants in gdt.h.  Not used in long
     * mode, but harmless. */
    kernel_gdt[1] = gdt_encode_segment(0, 0xFFFFF, 0x9A, 0xC);
    kernel_gdt[2] = gdt_encode_segment(0, 0xFFFFF, 0x92, 0xC);

    /* [3] 0x18 code64.  L=1 (bit 1 of flags) selects 64-bit mode.
     * Access 0x9A = present, DPL=0, S=1, type=0xA (code). */
    kernel_gdt[3] = gdt_encode_segment(0, 0xFFFFF, 0x9A, 0xA);

    /* [4] 0x20 data64.
     * Access 0x92 = present, DPL=0, S=1, type=0x2 (data). */
    kernel_gdt[4] = gdt_encode_segment(0, 0xFFFFF, 0x92, 0xA);

    /* [5] 0x28 user data, DPL=3.
     * Access 0xF2 = present, DPL=3, S=1, type=0x2.
     * Flags 0xC = G=1, D/B=1, L=0.  The L bit is a don't-care on data
     * segments, but 0xC matches the value gdt_fix_user_segments used
     * to write, and keeps gdtdump's output from saying "64-bit" on a
     * data segment. */
    kernel_gdt[5] = gdt_encode_segment(0, 0xFFFFF, 0xF2, 0xC);

    /* [6] 0x30 user code, DPL=3, L=1.
     * Access 0xFA = present, DPL=3, S=1, type=0xA. */
    kernel_gdt[6] = gdt_encode_segment(0, 0xFFFFF, 0xFA, 0xA);

    /* [7] 0x38 TSS.  Left zero; gdt_set_tss writes it.
     * The TSS descriptor has a different format from code/data
     * segments (S=0, 64-bit base split across two slots) and is not
     * built by gdt_encode_segment. */
    kernel_gdt[7] = 0;
    kernel_gdt[8] = 0;

    /* Load the new GDT. */
    struct __attribute__((packed)) {
        uint16_t limit;
        uint64_t base;
    } gdtr = {
        .limit = (GDT_ENTRIES * 8) - 1,
        .base  = (uint64_t)kernel_gdt,
    };
    __asm__ volatile("lgdt %0" : : "m"(gdtr) : "memory");

    /* Reload segment registers with the new selectors.  This also
     * performs the far jump that updates CS. */
    gdt_reload();

    serial_print("GDT: kernel GDT at 0x");
    serial_print_hex((uint64_t)kernel_gdt);
    serial_print(", ");
    serial_print_dec(GDT_ENTRIES);
    serial_print(" entries\n");
}

/*
 * Write the TSS descriptor into kernel_gdt[7..8].
 *
 * Before this change, the function read sgdt and wrote to whatever
 * address it returned.  Now it writes to kernel_gdt directly, which
 * is correct regardless of which GDT the CPU is currently using.
 * That makes the function independent of load order.
 *
 * The descriptor format for a 64-bit TSS:
 *
 *   bits  0..15   limit[15:0]
 *   bits 16..39   base[23:0]
 *   bits 40..47   access byte (P | DPL | S=0 | type)
 *                 0x89 = present, DPL=0, system, type=0x9 (available
 *                 64-bit TSS)
 *   bits 48..51   limit[19:16]
 *   bits 52..55   flags (G | 0 | 0 | AVL)
 *   bits 56..63   base[31:24]
 *   bits 64..95   base[63:32]
 *
 * The caller's tss_size is the size of the TSS region including the
 * I/O bitmap.  The SDM requires limit >= 0x67 for a 64-bit TSS; the
 * caller (tss_init) passes the full region size, which is larger.
 */
void gdt_set_tss(uint64_t tss_addr, uint32_t tss_size) {
    uint64_t base  = tss_addr;
    uint64_t limit = tss_size;

    if (limit < 0x67) limit = 0x67;

    uint64_t desc_low = 0;
    desc_low |= (limit & 0xFFFF);
    desc_low |= (base & 0xFFFFFF) << 16;
    desc_low |= ((uint64_t)0x89) << 40;
    desc_low |= ((limit >> 16) & 0xF) << 48;
    desc_low |= ((base >> 24) & 0xFF) << 56;

    uint64_t desc_high = (base >> 32) & 0xFFFFFFFF;

    kernel_gdt[7] = desc_low;
    kernel_gdt[8] = desc_high;
}

/*
 * Legacy function.  Before gdt_init wrote the user descriptors
 * itself, this was where the bootloader's placeholder user segments
 * were overwritten with correct DPL=3 descriptors.  The bootloader's
 * GDT had indices 5 and 6 reversed (0xFA at 5, 0xF2 at 6); gdt_init
 * now writes them correctly on the first pass, so this function has
 * nothing left to do.
 *
 * Kept (rather than deleted) because the call site in kmain still
 * exists in some branches of the tree.  It can be removed in a future
 * cleanup commit once every caller has been updated.  Do not add new
 * work here; if a user-segment change is needed, change gdt_init.
 */
void gdt_fix_user_segments(void) {
    serial_print("GDT: user segments already correct (gdt_init)\n");
}

/* Decode and print the current GDT on demand.  Called from the kernel
   shell's gdtdump command.  Prints to both serial and VGA.
   Uses sgdt so it always reports what the CPU is actually using, not
   what the kernel thinks it built. */
void gdt_dump(void) {
    struct {
        uint16_t limit;
        uint64_t base __attribute__((packed));
    } gdt_ptr;

    __asm__ volatile ("sgdt %0" : "=m"(gdt_ptr) : : "memory");

    uint64_t* gdt = (uint64_t*)gdt_ptr.base;
    int max_entries = (gdt_ptr.limit + 1) / 8;

    serial_print("\n=== GDT DUMP ===\n");
    vga_print("=== GDT DUMP ===\n");

    serial_print("GDT base=0x"); serial_print_hex(gdt_ptr.base);
    serial_print(" limit=0x"); serial_print_hex(gdt_ptr.limit);
    serial_print(" entries="); serial_print_dec(max_entries);
    serial_print("\n");
    vga_print("GDT base=0x"); vga_print_hex_cur(gdt_ptr.base);
    vga_print(" limit=0x"); vga_print_hex_cur(gdt_ptr.limit);
    vga_print("\n");

    for (int i = 0; i < max_entries; i++) {
        uint64_t desc = gdt[i];
        uint8_t access  = (desc >> 40) & 0xFF;
        uint8_t flags   = (desc >> 52) & 0xF;
        uint8_t dpl     = (access >> 5) & 0x3;
        uint8_t type    = (access >> 1) & 0x7;
        bool present    = (access >> 7) & 0x1;
        bool is_system  = !((access >> 4) & 0x1);
        bool is_code    = (access >> 3) & 0x1;
        bool is_64bit   = (flags & 0x2) ? true : false;

        serial_print("["); serial_print_dec(i); serial_print("] 0x");
        serial_print_hex(i * 8);
        serial_print(" = 0x"); serial_print_hex(desc);
        serial_print("  ");
        vga_print("["); vga_print_dec_cur(i); vga_print("] 0x");
        vga_print_hex_cur(i * 8);
        vga_print(" = 0x"); vga_print_hex_cur(desc);
        vga_print("  ");

        const char* kind;
        if (!present) {
            kind = "NotPresent";
        } else if (is_system) {
            kind = (type == 9) ? "TSS" : "System";
        } else if (is_code) {
            kind = "Code";
        } else {
            kind = "Data";
        }

        serial_print(kind);
        vga_print(kind);

        if (present) {
            serial_print(" DPL="); serial_print_dec(dpl);
            vga_print(" DPL="); vga_print_dec_cur(dpl);
            if (is_64bit) {
                serial_print(" 64-bit");
                vga_print(" 64-bit");
            }
        }
        serial_print("\n");
        vga_print("\n");
    }

    serial_print("=== END GDT DUMP ===\n\n");
    vga_print("=== END GDT DUMP ===\n");
}
