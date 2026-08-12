// This would be a user program to test syscalls
// For now, just test from kernel mode

void test_syscalls(void) {
    // Test SYS_WRITE
    const char* msg = "Hello from syscall!\n";
    long ret = sys_write(1, msg, 21);
    serial_printf("sys_write returned: %ld\n", ret);
    
    // Test SYS_EXIT
    // sys_exit(0);
}
