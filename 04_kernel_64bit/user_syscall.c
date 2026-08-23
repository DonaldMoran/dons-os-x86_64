#include "include/user_syscall.h"
#include <stdint.h>
#include "include/serial.h"
#include "include/vmm.h"
#include "include/keyboard.h"
#include "include/syscall.h"

// Safe copy from user space to kernel buffer using HHDM
static int safe_copy_from_user(void* kernel_dest, const void* user_src, size_t count) {
    if (count == 0) return 0;
    
    uint64_t user_addr = (uint64_t)user_src;
    
    serial_print("safe_copy: user_addr=0x");
    serial_print_hex(user_addr);
    serial_print(" count=");
    serial_print_dec(count);
    serial_print("\n");
    
    if (user_addr > 0x7FFFFFFFFFFFFFFFULL) {
        serial_print("ERROR: User address 0x");
        serial_print_hex(user_addr);
        serial_print(" is not in user space!\n");
        return -1;
    }
    
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
    
    uint64_t offset = user_addr & 0xFFF;
    void* kernel_mapped = (void*)(HHDM_START + phys + offset);
    
    serial_print("safe_copy: HHDM addr=0x");
    serial_print_hex((uint64_t)kernel_mapped);
    serial_print("\n");
    
    for (size_t i = 0; i < count; i++) {
        ((char*)kernel_dest)[i] = ((char*)kernel_mapped)[i];
    }
    
    return 0;
}

static int safe_copy_to_user(void* user_dest, const void* kernel_src, size_t count) {
    if (count == 0) return 0;
    
    uint64_t user_addr = (uint64_t)user_dest;
    
    if (user_addr > 0x7FFFFFFFFFFFFFFFULL) {
        serial_print("ERROR: User address 0x");
        serial_print_hex(user_addr);
        serial_print(" is not in user space!\n");
        return -1;
    }
    
    uint64_t phys = vmm_get_phys(user_addr);
    if (!phys) {
        serial_print("ERROR: User address 0x");
        serial_print_hex(user_addr);
        serial_print(" is not mapped!\n");
        return -1;
    }
    
    uint64_t offset = user_addr & 0xFFF;
    void* kernel_mapped = (void*)(HHDM_START + phys + offset);
    
    for (size_t i = 0; i < count; i++) {
        ((char*)kernel_mapped)[i] = ((char*)kernel_src)[i];
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
    case SYS_WRITE: {
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
        
        if (fd == 1) {
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
    
    case SYS_EXIT: {
        serial_print("sys_exit: status=");
        serial_print_hex(arg0);
        serial_print("\n");
        return arg0;
    }
    
    case SYS_READ: {
        uint64_t fd = arg0;
        uint64_t user_buf = arg1;
        uint64_t count = arg2;
        
        serial_print("sys_read: fd=");
        serial_print_dec(fd);
        serial_print(" buf=0x");
        serial_print_hex(user_buf);
        serial_print(" count=");
        serial_print_dec(count);
        serial_print("\n");
        
        if (fd == 0) {
            char c;
            uint64_t bytes_read = 0;
            
            while (bytes_read < count) {
                // Block until a key is available
                while (!kbd_buffer_get(&c)) {
                    // No data - wait for interrupt
                    __asm__ volatile ("pause");
                }
                
                // Echo character back to console (handled in userlib)
                // But we need to echo here so the user sees what they type
                if (c == '\n') {
                    // sys_write(1, "\n", 1); // Already handled by read_line
                } else if (c == '\b' || c == 127) {
                    // Backspace handled by read_line
                } else if (c >= 32 && c <= 126) {
                    // Printable characters are echoed by read_line
                }
                
                if (safe_copy_to_user((void*)(user_buf + bytes_read), &c, 1) == 0) {
                    bytes_read++;
                } else {
                    serial_print("sys_read: FAILED to copy to user space!\n");
                    return (uint64_t)-1;
                }
            }
            
            serial_print("sys_read: read ");
            serial_print_dec(bytes_read);
            serial_print(" bytes\n");
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
