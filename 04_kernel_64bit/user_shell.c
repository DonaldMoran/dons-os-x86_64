#include "include/userlib.h"

// ============================================================
// Command handlers
// ============================================================

static void cmd_help(void) {
    sys_write(1, "Commands:\n", 10);
    sys_write(1, "  help    - Show this help\n", 26);
    sys_write(1, "  exit    - Exit shell\n", 23);
    sys_write(1, "  clear   - Clear screen\n", 25);
    sys_write(1, "  whoami  - Show current user\n", 29);
    sys_write(1, "  version - Show version\n", 25);
    sys_write(1, "  echo    - Echo text back\n", 27);
}

static void cmd_clear(void) {
    for (int i = 0; i < 20; i++) {
        sys_write(1, "\n", 1);
    }
}

static void cmd_whoami(void) {
    sys_write(1, "user\n", 5);
}

static void cmd_version(void) {
    sys_write(1, "User Shell v0.1\n", 16);
}

static void cmd_echo(char* arg) {
    if (arg && arg[0]) {
        sys_write(1, arg, strlen(arg));
        sys_write(1, "\n", 1);
    } else {
        sys_write(1, "Usage: echo <text>\n", 19);
    }
}

// ============================================================
// Main shell entry point
// ============================================================

void _start(void) {
    sys_write(1, "User Shell v0.1\n", 16);
    sys_write(1, "Type 'help' for commands\n", 25);
    
    char cmd_buffer[128];
    char* cmd;
    char* arg;
    
    while (1) {
        sys_write(1, "$> ", 3);
        
        int len = read_line(cmd_buffer, sizeof(cmd_buffer));
        if (len == 0) continue;
        
        // Parse command and argument
        cmd = cmd_buffer;
        arg = cmd_buffer;
        
        // Skip leading spaces
        while (*arg == ' ') arg++;
        cmd = arg;
        
        // Find end of command
        while (*arg && *arg != ' ') arg++;
        
        if (*arg == ' ') {
            *arg = '\0';
            arg++;
            // Skip spaces in argument
            while (*arg == ' ') arg++;
        } else {
            arg = 0;
        }
        
        // Handle commands
        if (strcmp(cmd, "help") == 0) {
            cmd_help();
        } else if (strcmp(cmd, "exit") == 0) {
            sys_write(1, "Goodbye!\n", 9);
            sys_exit(0);
        } else if (strcmp(cmd, "clear") == 0) {
            cmd_clear();
        } else if (strcmp(cmd, "whoami") == 0) {
            cmd_whoami();
        } else if (strcmp(cmd, "version") == 0) {
            cmd_version();
        } else if (strcmp(cmd, "echo") == 0) {
            cmd_echo(arg);
        } else {
            sys_write(1, "Unknown command: ", 17);
            sys_write(1, cmd, strlen(cmd));
            sys_write(1, "\nType 'help' for available commands\n", 35);
        }
    }
}
