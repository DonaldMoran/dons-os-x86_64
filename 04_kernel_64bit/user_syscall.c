#include "include/user_syscall.h"
#include <stdint.h>
#include "include/serial.h"
#include "include/vmm.h"
#include "include/keyboard.h"
#include "include/syscall.h"
#include "include/vga.h"
#include "include/process.h"
#include "include/scheduler.h"

// ============================================================
// SAFE COPY FROM USER → KERNEL
// ============================================================

static int safe_copy_from_user(void* kernel_dest, const void* user_src, size_t count) {
    uint64_t base = (uint64_t)user_src;
    uint8_t* dst = (uint8_t*)kernel_dest;

    serial_print("safe_copy_from_user: ENTERED\n");
    serial_print("safe_copy_from_user: base=0x");
    serial_print_hex(base);
    serial_print(" count=");
    serial_print_dec(count);
    serial_print("\n");

    size_t copied = 0;

    while (copied < count) {
        uint64_t cur = base + copied;

        if (cur > 0xFFFFFFFFFFFFFFFFULL) {
            serial_print("safe_copy_from_user: invalid user address 0x");
            serial_print_hex(cur);
            serial_print("\n");
            return -1;
        }

        serial_print("safe_copy: ABOUT to call vmm_get_phys for 0x");
        serial_print_hex(cur);
        serial_print("\n");

        uint64_t phys_with_offset = vmm_get_phys(cur);

        serial_print("safe_copy: cur=0x");
        serial_print_hex(cur);
        serial_print(" phys=0x");
        serial_print_hex(phys_with_offset);
        serial_print("\n");

        if (!phys_with_offset) {
            serial_print("safe_copy_from_user: unmapped user address 0x");
            serial_print_hex(cur);
            serial_print("\n");
            return -1;
        }

        // phys_with_offset ALREADY includes (cur & 0xFFF)
        uint8_t* src = (uint8_t*)(HHDM_START + phys_with_offset);

        size_t chunk;
        {
            uint64_t page_off = cur & 0xFFF;
            size_t page_rem = 4096 - page_off;
            chunk = count - copied;
            if (chunk > page_rem)
                chunk = page_rem;
        }

        serial_print("safe_copy: chunk=");
        serial_print_dec(chunk);
        serial_print(" src=0x");
        serial_print_hex((uint64_t)src);
        serial_print(" dst=0x");
        serial_print_hex((uint64_t)&dst[copied]);
        serial_print("\n");

        serial_print("safe_copy: source bytes: ");
        for (size_t i = 0; i < chunk && i < 16; i++) {
            serial_print_hex(src[i]);
            serial_print(" ");
        }
        serial_print("\n");

        for (size_t i = 0; i < chunk; i++)
            dst[copied + i] = src[i];

        serial_print("safe_copy: dest bytes after: ");
        for (size_t i = 0; i < chunk && i < 16; i++) {
            serial_print_hex(dst[copied + i]);
            serial_print(" ");
        }
        serial_print("\n");

        copied += chunk;
    }

    serial_print("safe_copy_from_user: done, copied=");
    serial_print_dec(copied);
    serial_print("\n");
    return 0;
}

// ============================================================
// SAFE COPY KERNEL → USER
// ============================================================

static int safe_copy_to_user(void* user_dest, const void* kernel_src, size_t count) {
    uint64_t base = (uint64_t)user_dest;
    const uint8_t* src = (const uint8_t*)kernel_src;

    size_t copied = 0;

    while (copied < count) {
        uint64_t cur = base + copied;

        if (cur > 0xFFFFFFFFFFFFFFFFULL) {
            serial_print("safe_copy_to_user: invalid user address 0x");
            serial_print_hex(cur);
            serial_print("\n");
            return -1;
        }

        uint64_t phys_with_offset = vmm_get_phys(cur);
        if (!phys_with_offset) {
            serial_print("safe_copy_to_user: unmapped user address 0x");
            serial_print_hex(cur);
            serial_print("\n");
            return -1;
        }

        uint8_t* dst = (uint8_t*)(HHDM_START + phys_with_offset);

        size_t chunk;
        {
            uint64_t page_off = cur & 0xFFF;
            size_t page_rem = 4096 - page_off;
            chunk = count - copied;
            if (chunk > page_rem)
                chunk = page_rem;
        }

        for (size_t i = 0; i < chunk; i++)
            dst[i] = src[copied + i];

        copied += chunk;
    }

    return 0;
}

// ============================================================
// SYSCALL DISPATCHER
// ============================================================

uint64_t syscall_dispatch(uint64_t num,
                          uint64_t arg0,
                          uint64_t arg1,
                          uint64_t arg2,
                          uint64_t arg3,
                          uint64_t arg4,
                          uint64_t arg5)
{
    (void)arg3;
    (void)arg4;
    (void)arg5;

    uint64_t cr3;
    asm volatile("mov %%cr3, %0" : "=r"(cr3));
    serial_print("syscall_dispatch: CR3 = 0x");
    serial_print_hex(cr3);
    serial_print(" num=");
    serial_print_dec(num);
    serial_print("\n");

    switch (num) {

    case SYS_WRITE: {
        uint64_t fd       = arg0;
        uint64_t user_buf = arg1;
        uint64_t count    = arg2;

        serial_print("sys_write: fd=");
        serial_print_dec(fd);
        serial_print(" buf=0x");
        serial_print_hex(user_buf);
        serial_print(" count=");
        serial_print_dec(count);
        serial_print("\n");

        if (fd == 1) {
            #define MAX_SYS_WRITE 4096
            char kernel_buf[MAX_SYS_WRITE + 1];

            size_t to_copy = (count > MAX_SYS_WRITE ? MAX_SYS_WRITE : count);

            if (safe_copy_from_user(kernel_buf, (const void*)user_buf, to_copy) == 0) {
                kernel_buf[to_copy] = '\0';

                serial_print("sys_write: VGA OUTPUT: '");
                for (size_t i = 0; i < to_copy; i++) {
                    char c = kernel_buf[i];
                    if (c == '\n') serial_print("\\n");
                    else if (c == '\r') serial_print("\\r");
                    else if (c == '\t') serial_print("\\t");
                    else if (c == 0) serial_print("\\0");
                    else if (c >= 32 && c <= 126) serial_putc(c);
                    else serial_print(".");
                }
                serial_print("'\n");

                vga_write(kernel_buf, to_copy);

            } else {
                serial_print("sys_write: FAILED to copy from user space!\n");
                return (uint64_t)-1;
            }
        }

        return count;
    }

    case SYS_EXIT: {
        serial_print("sys_exit: status=");
        serial_print_hex(arg0);
        serial_print("\n");

        extern void process_exit(void) __attribute__((noreturn));
        process_exit();
        while (1) asm volatile("hlt");
    }

    case SYS_READ: {
        uint64_t fd       = arg0;
        uint64_t user_buf = arg1;
        uint64_t count    = arg2;

        if (fd == 0) {
            char c;
            uint64_t bytes_read = 0;

            while (bytes_read < count) {
                if (kbd_buffer_get(&c)) {

                    if (c == '\n') {
                        char nl = '\n';
                        vga_write(&nl, 1);
                        serial_putc(nl);
                    }
                    else if (c == '\b' || c == 127) {
                        char bs[] = {'\b',' ','\b'};
                        vga_write(bs, 3);
                        for (int i = 0; i < 3; i++) serial_putc(bs[i]);
                    }
                    else if (c >= 32 && c <= 126) {
                        vga_write(&c, 1);
                        serial_putc(c);
                    }

                    if (safe_copy_to_user((void*)(user_buf + bytes_read), &c, 1) == 0)
                        bytes_read++;
                    else
                        return (uint64_t)-1;

                } else {
                    return bytes_read;
                }
            }

            return bytes_read;
        }

        return 0;
    }

    default:
        serial_print("sys_unknown: num=");
        serial_print_hex(num);
        serial_print("\n");
        return (uint64_t)-1;
    }
}

void syscall_init(void) {
    extern void syscall_init_asm(void);
    syscall_init_asm();
    serial_print("SYSCALL init done\n");
}
