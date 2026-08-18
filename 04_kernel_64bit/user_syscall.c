#include <user_syscall.h>
#include <stdint.h>
#include "include/serial.h"
#include "include/vmm.h"

// Safe copy from user space to kernel buffer using HHDM
static int safe_copy_from_user(void* kernel_dest, const void* user_src, size_t count) {
    if (count == 0) return 0;
    
    uint64_t user_addr = (uint64_t)user_src;
    
    serial_print("safe_copy: user_addr=0x");
    serial_print_hex(user_addr);
    serial_print(" count=");
    serial_print_dec(count);
    serial_print("\n");
    
    // Basic validation: user address should be in user space
    if (user_addr > 0x7FFFFFFFFFFFFFFFULL) {
        serial_print("ERROR: User address 0x");
        serial_print_hex(user_addr);
        serial_print(" is not in user space!\n");
        return -1;
    }
    
    // Get the physical address of the user page
    uint64_t phys = vmm_get_phys(user_addr);
    if (!phys) {
        serial_print("ERROR: User address 0x");
        serial_print_hex(user_addr);
        serial_print(" is not mapped!\n");
        return -1;
    }
    
    serial_print("safe_copy: phys=0x");
    serial_print_hex(phys);
    serial_print("\n");
    
    // Calculate offset within the page
    uint64_t offset = user_addr & 0xFFF;
    
    // Map the user page into kernel space (HHDM)
    void* kernel_mapped = (void*)(HHDM_START + phys + offset);
    
    serial_print("safe_copy: HHDM addr=0x");
    serial_print_hex((uint64_t)kernel_mapped);
    serial_print("\n");
    
    // Copy from HHDM to kernel buffer
    for (size_t i = 0; i < count; i++) {
        ((char*)kernel_dest)[i] = ((char*)kernel_mapped)[i];
    }
    
    return 0;
}

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
    
    switch (num) {
    case 1: { // write(fd, buf, count)
        uint64_t fd = arg0;
        uint64_t user_buf = arg1;
        uint64_t count = arg2;
        
        serial_print("sys_write: fd=");
        serial_print_dec(fd);
        serial_print(" buf=0x");
        serial_print_hex(user_buf);
        serial_print(" count=");
        serial_print_dec(count);
        serial_print("\n");
        
        if (fd == 1) { // stdout
            #define MAX_SYS_WRITE 256
            char kernel_buf[MAX_SYS_WRITE + 1];
            
            size_t to_copy = count;
            if (to_copy > MAX_SYS_WRITE) {
                to_copy = MAX_SYS_WRITE;
            }
            
            serial_print("sys_write: copying ");
            serial_print_dec(to_copy);
            serial_print(" bytes from user\n");
            
            if (safe_copy_from_user(kernel_buf, (const void*)user_buf, to_copy) == 0) {
                kernel_buf[to_copy] = '\0';
                serial_print("sys_write: data='");
                for (size_t i = 0; i < to_copy; i++) {
                    serial_putc(kernel_buf[i]);
                }
                serial_print("'\n");
            } else {
                serial_print("sys_write: FAILED to copy from user space!\n");
                return (uint64_t)-1;
            }
        }
        
        return count;
    }
    
    case 2: { // exit(status)
        serial_print("sys_exit: status=");
        serial_print_hex(arg0);
        serial_print("\n");
        return arg0;
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
