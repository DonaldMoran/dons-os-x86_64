#include <stdint.h>
#include "include/serial.h"

uint64_t syscall_dispatch(uint64_t num,
                          uint64_t arg0,
                          uint64_t arg1,
                          uint64_t arg2,
                          uint64_t arg3,
                          uint64_t arg4,
                          uint64_t arg5)
{
    // Mark unused parameters to silence warnings
    (void)arg3;
    (void)arg4;
    (void)arg5;
    switch (num) {
    case 1: { // write(fd, buf, count)
        uint64_t fd   = arg0;
        const char *buf = (const char *)arg1;
        uint64_t count = arg2;

        serial_print("sys_write: fd=");
        serial_print_hex(fd);
        serial_print(" buf=");
        serial_print_hex((uint64_t)buf);
        serial_print(" count=");
        serial_print_hex(count);
        serial_print("\n");

        if (fd == 1) {
            for (uint64_t i = 0; i < count; i++) {
                serial_putc(buf[i]);   // simple, unsafe copy-from-user
            }
        }

        return count;
    }

    case 2: { // exit(status)
        serial_print("sys_exit: status=");
        serial_print_hex(arg0);
        serial_print("\n");
        for (;;) ;
        return 0;
        // For now: just return to kernel with status in RAX
        //~ return arg0;
    }

    default:
        serial_print("sys_unknown: num=");
        serial_print_hex(num);
        serial_print("\n");
        return (uint64_t)-1;
    }
}
