#include "include/userlib.h"

// ============================================================
// Command handlers
// ============================================================

static void cmd_help(void) {
    print_string("Commands:\n");
    print_string("  help    - Show this help\n");
    print_string("  exit    - Exit shell\n");
    print_string("  clear   - Clear screen\n");
    print_string("  whoami  - Show current user\n");
    print_string("  version - Show version\n");
    print_string("  echo    - Echo text back\n");
}

static void cmd_clear(void) {
    for (int i = 0; i < 20; i++) {
        print_newline();
    }
}

static void cmd_whoami(void) {
    print_string("user\n");
}

static void cmd_version(void) {
    print_string("User Shell v0.1\n");
}

static void cmd_echo(char* arg) {
    if (arg && arg[0]) {
        print_string(arg);
        print_newline();
    } else {
        print_string("Usage: echo <text>\n");
    }
}

// ============================================================
// Main shell entry point
// ============================================================

void _start(void) {
    print_string("User Shell v0.1\n");
    print_string("Type 'help' for commands\n");
    
    char cmd_buffer[128];
    char* cmd;
    char* arg;
    
    while (1) {
        print_string("$> ");
        
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
            print_string("Goodbye!\n");
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
            print_string("Unknown command: ");
            print_string(cmd);
            print_string("\nType 'help' for available commands\n");
        }
    }
}
