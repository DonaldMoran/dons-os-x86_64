# Note the following code seems to get a job in the scheduler then exit immediately

---

```c
// user_shell.c

#include "include/userlib.h"

static const char shell_strings[] =
    "User Shell v0.1\n"
    "Type 'help' for commands\n"
    "Commands:\n"
    "  help    - Show this help\n"
    "  exit    - Exit shell\n"
    "  clear   - Clear screen\n"
    "  whoami  - Show current user\n"
    "  version - Show version\n"
    "  echo    - Echo text back\n"
    "user\n"
    "$> "
    "Goodbye!\n"
    "Unknown command: "
    "\nType 'help' for available commands\n"
    "Usage: echo <text>\n";

void _start(void) {
    sys_write(1, shell_strings, sizeof(shell_strings) - 1);
    sys_exit(0);
}
```

```text
ELF: LOAD vaddr=0x8000000000 memsz=0x70F filesz=0x70F offset=0x1000
ELF: Copying filesz=0x70F to 0x8000000000
ELF: LOAD vaddr=0x8000000710 memsz=0x14E filesz=0x14E offset=0x1710
ELF: Copying filesz=0x14E to 0x8000000710
ELF: Process created successfully
ELF: Starting process via jump_to_user_mode
ELF: TSS RSP0 set to 0xFFFFFFFF80121280
sys_write: fd=1 buf=0x8000000710 count=297
sys_write: VGA OUTPUT: 'User Shell v0.1\nType 'help' for commands\nCommands:\n  help    - Show this help\n  exit    - Exit shell\n  clear   - Clear screen\n  whoami  - Show current user\n >
sys_exit: status=0
PROCESS: Process 1 (idle) exiting
SCHEDULER: Switching from PID 1 (idle) to PID 2 (elf_prog)
sys_write: fd=1 buf=0x8000000710 count=297
sys_write: VGA OUTPUT: 'User Shell v0.1\nType 'help' for commands\nCommands:\n  help    - Show this help\n  exit    - Exit shell\n  clear   - Clear screen\n  whoami  - Show current user\n >
sys_exit: status=0
PROCESS: Process 2 (elf_prog) exiting
PROCESS: No processes left, returning to shell
DonsDOS v0.4.6
Type 'help
```

---

# This version built on the exact shell_strings blob you just verified, with compiler‑computed offsets (no hand counting), either does not get a job or does not exit, I am not sure.

```c
#include "include/userlib.h"

// ============================================================
// All strings in one block
// ============================================================

static const char shell_strings[] =
    "User Shell v0.1\n"
    "Type 'help' for commands\n"
    "Commands:\n"
    "  help    - Show this help\n"
    "  exit    - Exit shell\n"
    "  clear   - Clear screen\n"
    "  whoami  - Show current user\n"
    "  version - Show version\n"
    "  echo    - Echo text back\n"
    "user\n"
    "$> "
    "Goodbye!\n"
    "Unknown command: "
    "\nType 'help' for available commands\n"
    "Usage: echo <text>\n";

// offsets and lengths, derived from the literals above
enum {
    OFF_BANNER      = 0,
    LEN_BANNER      = sizeof("User Shell v0.1\n") - 1,

    OFF_INTRO       = OFF_BANNER + LEN_BANNER,
    LEN_INTRO       = sizeof("Type 'help' for commands\n") - 1,

    OFF_CMDS_HDR    = OFF_INTRO + LEN_INTRO,
    LEN_CMDS_HDR    = sizeof("Commands:\n") - 1,

    OFF_HELP        = OFF_CMDS_HDR + LEN_CMDS_HDR,
    LEN_HELP        = sizeof("  help    - Show this help\n") - 1,

    OFF_EXIT        = OFF_HELP + LEN_HELP,
    LEN_EXIT        = sizeof("  exit    - Exit shell\n") - 1,

    OFF_CLEAR       = OFF_EXIT + LEN_EXIT,
    LEN_CLEAR       = sizeof("  clear   - Clear screen\n") - 1,

    OFF_WHOAMI      = OFF_CLEAR + LEN_CLEAR,
    LEN_WHOAMI      = sizeof("  whoami  - Show current user\n") - 1,

    OFF_VERSION     = OFF_WHOAMI + LEN_WHOAMI,
    LEN_VERSION     = sizeof("  version - Show version\n") - 1,

    OFF_ECHO        = OFF_VERSION + LEN_VERSION,
    LEN_ECHO        = sizeof("  echo    - Echo text back\n") - 1,

    OFF_USER        = OFF_ECHO + LEN_ECHO,
    LEN_USER        = sizeof("user\n") - 1,

    OFF_PROMPT      = OFF_USER + LEN_USER,
    LEN_PROMPT      = sizeof("$> ") - 1,

    OFF_GOODBYE     = OFF_PROMPT + LEN_PROMPT,
    LEN_GOODBYE     = sizeof("Goodbye!\n") - 1,

    OFF_UNKNOWN     = OFF_GOODBYE + LEN_GOODBYE,
    LEN_UNKNOWN     = sizeof("Unknown command: ") - 1,

    OFF_HELP_HINT   = OFF_UNKNOWN + LEN_UNKNOWN,
    LEN_HELP_HINT   = sizeof("\nType 'help' for available commands\n") - 1,

    OFF_ECHO_USAGE  = OFF_HELP_HINT + LEN_HELP_HINT,
    LEN_ECHO_USAGE  = sizeof("Usage: echo <text>\n") - 1,
};

// ============================================================
// Command handlers
// ============================================================

static void cmd_help(void) {
    sys_write(1, shell_strings + OFF_CMDS_HDR, LEN_CMDS_HDR);
    sys_write(1, shell_strings + OFF_HELP,     LEN_HELP);
    sys_write(1, shell_strings + OFF_EXIT,     LEN_EXIT);
    sys_write(1, shell_strings + OFF_CLEAR,    LEN_CLEAR);
    sys_write(1, shell_strings + OFF_WHOAMI,   LEN_WHOAMI);
    sys_write(1, shell_strings + OFF_VERSION,  LEN_VERSION);
    sys_write(1, shell_strings + OFF_ECHO,     LEN_ECHO);
}

static void cmd_clear(void) {
    for (int i = 0; i < 20; i++)
        sys_write(1, "\n", 1);
}

static void cmd_whoami(void) {
    sys_write(1, shell_strings + OFF_USER, LEN_USER);
}

static void cmd_version(void) {
    sys_write(1, shell_strings + OFF_BANNER, LEN_BANNER);
}

static void cmd_echo(char* arg) {
    if (arg && arg[0]) {
        sys_write(1, arg, strlen(arg));
        sys_write(1, "\n", 1);
    } else {
        sys_write(1, shell_strings + OFF_ECHO_USAGE, LEN_ECHO_USAGE);
    }
}

// ============================================================
// Main shell entry point
// ============================================================

void _start(void) {
    sys_write(1, shell_strings + OFF_BANNER, LEN_BANNER);
    sys_write(1, shell_strings + OFF_INTRO,  LEN_INTRO);
    
    char cmd_buffer[128];
    char* cmd;
    char* arg;
    
    while (1) {
        sys_write(1, shell_strings + OFF_PROMPT, LEN_PROMPT);
        
        int len = read_line(cmd_buffer, sizeof(cmd_buffer));
        if (len == 0) continue;
        
        cmd = cmd_buffer;
        arg = cmd_buffer;
        
        while (*arg == ' ') arg++;
        cmd = arg;
        
        while (*arg && *arg != ' ') arg++;
        
        if (*arg == ' ') {
            *arg = '\0';
            arg++;
            while (*arg == ' ') arg++;
        } else {
            arg = 0;
        }
        
        if (strcmp(cmd, "help") == 0) {
            cmd_help();
        } else if (strcmp(cmd, "exit") == 0) {
            sys_write(1, shell_strings + OFF_GOODBYE, LEN_GOODBYE);
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
            sys_write(1, shell_strings + OFF_UNKNOWN,   LEN_UNKNOWN);
            sys_write(1, cmd, strlen(cmd));
            sys_write(1, shell_strings + OFF_HELP_HINT, LEN_HELP_HINT);
        }
    }
}
```

```text
ELF: LOAD vaddr=0x8000000000 memsz=0xC7F filesz=0xC7F offset=0x1000
ELF: Copying filesz=0xC7F to 0x8000000000
ELF: LOAD vaddr=0x8000000C80 memsz=0x172 filesz=0x172 offset=0x1C80
ELF: Copying filesz=0x172 to 0x8000000C80
ELF: Process created successfully
ELF: Starting process via jump_to_user_mode
ELF: TSS RSP0 set to 0xFFFFFFFF801218C0
sys_write: fd=1 buf=0x8000000C80 count=16
sys_write: VGA OUTPUT: 'User Shell v0.1\n'
sys_write: fd=1 buf=0x8000000C90 count=25
sys_write: VGA OUTPUT: 'commands\nCommands:\n  help'
sys_write: fd=1 buf=0x8000000D55 count=3
sys_write: VGA OUTPUT: '\0\0\0'
```

















//~ #include "include/userlib.h"

//~ static const char shell_strings[] =
    //~ "User Shell v0.1\n"
    //~ "Type 'help' for commands\n"
    //~ "Commands:\n"
    //~ "  help    - Show this help\n"
    //~ "  exit    - Exit shell\n"
    //~ "  clear   - Clear screen\n"
    //~ "  whoami  - Show current user\n"
    //~ "  version - Show version\n"
    //~ "  echo    - Echo text back\n"
    //~ "user\n"
    //~ "$> "
    //~ "Goodbye!\n"
    //~ "Unknown command: "
    //~ "\nType 'help' for available commands\n"
    //~ "Usage: echo <text>\n";

//~ void _start(void) {
    //~ sys_write(1, shell_strings, sizeof("User Shell v0.1\n") - 1);
    //~ sys_write(1, shell_strings + sizeof("User Shell v0.1\n") - 1,
              //~ sizeof("Type 'help' for commands\n") - 1);
    //~ sys_exit(0);
//~ }















//~ #include "include/userlib.h"

//~ static const char shell_strings[] =
    //~ "User Shell v0.1\n"
    //~ "Type 'help' for commands\n"
    //~ "Commands:\n"
    //~ "  help    - Show this help\n"
    //~ "  exit    - Exit shell\n"
    //~ "  clear   - Clear screen\n"
    //~ "  whoami  - Show current user\n"
    //~ "  version - Show version\n"
    //~ "  echo    - Echo text back\n"
    //~ "user\n"
    //~ "$> "
    //~ "Goodbye!\n"
    //~ "Unknown command: "
    //~ "\nType 'help' for available commands\n"
    //~ "Usage: echo <text>\n";

//~ static const char *line_start(int target) {
    //~ const char *p = shell_strings;
    //~ int line = 0;

    //~ if (target == 0) return shell_strings;

    //~ while (*p && line < target) {
        //~ if (*p == '\n') line++;
        //~ p++;
    //~ }
    //~ return (*p && line == target) ? p : NULL;
//~ }

//~ static void print_line(int n) {
    //~ const char *start = line_start(n);
    //~ if (!start) return;

    //~ const char *p = start;
    //~ while (*p && *p != '\n') p++;
    //~ if (*p == '\n') p++;

    //~ sys_write(1, start, p - start);
//~ }

//~ static void print_lines(int first, int last) {
    //~ for (int i = first; i <= last; i++)
        //~ print_line(i);
//~ }

//~ // line numbers in shell_strings:
//~ // 0: "User Shell v0.1\n"
//~ // 1: "Type 'help' for commands\n"
//~ // 2: "Commands:\n"
//~ // 3: "  help    - Show this help\n"
//~ // 4: "  exit    - Exit shell\n"
//~ // 5: "  clear   - Clear screen\n"
//~ // 6: "  whoami  - Show current user\n"
//~ // 7: "  version - Show version\n"
//~ // 8: "  echo    - Echo text back\n"
//~ // 9: "user\n"
//~ // 10: "$> "   (no newline)
//~ // 11: "Goodbye!\n"
//~ // 12: "Unknown command: "
//~ // 13: "\nType 'help' for available commands\n"
//~ // 14: "Usage: echo <text>\n"

//~ static void cmd_help(void) {
    //~ print_lines(2, 8);
//~ }

//~ static void cmd_clear(void) {
    //~ for (int i = 0; i < 20; i++)
        //~ sys_write(1, "\n", 1);
//~ }

//~ static void cmd_whoami(void) {
    //~ print_line(9);
//~ }

//~ static void cmd_version(void) {
    //~ print_line(0);
//~ }

//~ static void cmd_echo(char *arg) {
    //~ if (arg && arg[0]) {
        //~ sys_write(1, arg, strlen(arg));
        //~ sys_write(1, "\n", 1);
    //~ } else {
        //~ print_line(14);
    //~ }
//~ }

//~ void _start(void) {
    //~ // banner + intro
    //~ print_line(0);
    //~ print_line(1);

    //~ char cmd_buffer[128];
    //~ char *cmd, *arg;

    //~ while (1) {
        //~ // prompt: "$> " (we know its literal)
        //~ sys_write(1, "$> ", 3);

        //~ int len = read_line(cmd_buffer, sizeof(cmd_buffer));
        //~ if (len == 0) continue;

        //~ cmd = cmd_buffer;
        //~ arg = cmd_buffer;

        //~ while (*arg == ' ') arg++;
        //~ cmd = arg;

        //~ while (*arg && *arg != ' ') arg++;

        //~ if (*arg == ' ') {
            //~ *arg = '\0';
            //~ arg++;
            //~ while (*arg == ' ') arg++;
        //~ } else {
            //~ arg = 0;
        //~ }

        //~ if (strcmp(cmd, "help") == 0) {
            //~ cmd_help();
        //~ } else if (strcmp(cmd, "exit") == 0) {
            //~ print_line(11);      // "Goodbye!\n"
            //~ sys_exit(0);
        //~ } else if (strcmp(cmd, "clear") == 0) {
            //~ cmd_clear();
        //~ } else if (strcmp(cmd, "whoami") == 0) {
            //~ cmd_whoami();
        //~ } else if (strcmp(cmd, "version") == 0) {
            //~ cmd_version();
        //~ } else if (strcmp(cmd, "echo") == 0) {
            //~ cmd_echo(arg);
        //~ } else {
            //~ // "Unknown command: " + cmd + hint line
            //~ sys_write(1, "Unknown command: ", 17);
            //~ sys_write(1, cmd, strlen(cmd));
            //~ print_line(13);      // "\nType 'help' for available commands\n"
        //~ }
    //~ }
//~ }




















//~ #include "include/userlib.h"

//~ // ============================================================
//~ // Stable, separate string objects (no offsets, no relocations)
//~ // ============================================================

//~ static const char str_banner[]          = "User Shell v0.1\n";
//~ static const char str_intro[]           = "Type 'help' for commands\n";
//~ static const char str_commands_hdr[]    = "Commands:\n";
//~ static const char str_help[]            = "  help    - Show this help\n";
//~ static const char str_exit[]            = "  exit    - Exit shell\n";
//~ static const char str_clear[]           = "  clear   - Clear screen\n";
//~ static const char str_whoami[]          = "  whoami  - Show current user\n";
//~ static const char str_version[]         = "  version - Show version\n";
//~ static const char str_echo[]            = "  echo    - Echo text back\n";
//~ static const char str_user[]            = "user\n";
//~ static const char str_prompt[]          = "$> ";
//~ static const char str_goodbye[]         = "Goodbye!\n";
//~ static const char str_unknown[]         = "Unknown command: ";
//~ static const char str_help_available[]  =
    //~ "\nType 'help' for available commands\n";
//~ static const char str_usage_echo[]      = "Usage: echo <text>\n";

//~ // ============================================================
//~ // Command handlers
//~ // ============================================================

//~ static void cmd_help(void) {
    //~ sys_write(1, str_commands_hdr,    sizeof(str_commands_hdr) - 1);
    //~ sys_write(1, str_help,            sizeof(str_help) - 1);
    //~ sys_write(1, str_exit,            sizeof(str_exit) - 1);
    //~ sys_write(1, str_clear,           sizeof(str_clear) - 1);
    //~ sys_write(1, str_whoami,          sizeof(str_whoami) - 1);
    //~ sys_write(1, str_version,         sizeof(str_version) - 1);
    //~ sys_write(1, str_echo,            sizeof(str_echo) - 1);
//~ }

//~ static void cmd_clear(void) {
    //~ for (int i = 0; i < 20; i++)
        //~ sys_write(1, "\n", 1);
//~ }

//~ static void cmd_whoami(void) {
    //~ sys_write(1, str_user, sizeof(str_user) - 1);
//~ }

//~ static void cmd_version(void) {
    //~ sys_write(1, str_banner, sizeof(str_banner) - 1);
//~ }

//~ static void cmd_echo(char* arg) {
    //~ if (arg && arg[0]) {
        //~ sys_write(1, arg, strlen(arg));
        //~ sys_write(1, "\n", 1);
    //~ } else {
        //~ sys_write(1, str_usage_echo, sizeof(str_usage_echo) - 1);
    //~ }
//~ }

//~ // ============================================================
//~ // Main shell entry point
//~ // ============================================================

//~ void _start(void) {
    //~ // Initial banner + intro
    //~ sys_write(1, str_banner, sizeof(str_banner) - 1);
    //~ sys_write(1, str_intro,  sizeof(str_intro) - 1);

    //~ char cmd_buffer[128];
    //~ char* cmd;
    //~ char* arg;

    //~ while (1) {
        //~ sys_write(1, str_prompt, sizeof(str_prompt) - 1);

        //~ int len = read_line(cmd_buffer, sizeof(cmd_buffer));
        //~ if (len == 0) continue;

        //~ cmd = cmd_buffer;
        //~ arg = cmd_buffer;

        //~ while (*arg == ' ') arg++;
        //~ cmd = arg;

        //~ while (*arg && *arg != ' ') arg++;

        //~ if (*arg == ' ') {
            //~ *arg = '\0';
            //~ arg++;
            //~ while (*arg == ' ') arg++;
        //~ } else {
            //~ arg = 0;
        //~ }

        //~ if (strcmp(cmd, "help") == 0) {
            //~ cmd_help();
        //~ } else if (strcmp(cmd, "exit") == 0) {
            //~ sys_write(1, str_goodbye, sizeof(str_goodbye) - 1);
            //~ sys_exit(0);
        //~ } else if (strcmp(cmd, "clear") == 0) {
            //~ cmd_clear();
        //~ } else if (strcmp(cmd, "whoami") == 0) {
            //~ cmd_whoami();
        //~ } else if (strcmp(cmd, "version") == 0) {
            //~ cmd_version();
        //~ } else if (strcmp(cmd, "echo") == 0) {
            //~ cmd_echo(arg);
        //~ } else {
            //~ sys_write(1, str_unknown,        sizeof(str_unknown) - 1);
            //~ sys_write(1, cmd,                strlen(cmd));
            //~ sys_write(1, str_help_available, sizeof(str_help_available) - 1);
        //~ }
    //~ }
//~ }
































//~ #include "include/userlib.h"

//~ static const char shell_strings[] =
    //~ "User Shell v0.1\n"
    //~ "Type 'help' for commands\n"
    //~ "Commands:\n"
    //~ "  help    - Show this help\n"
    //~ "  exit    - Exit shell\n"
    //~ "  clear   - Clear screen\n"
    //~ "  whoami  - Show current user\n"
    //~ "  version - Show version\n"
    //~ "  echo    - Echo text back\n"
    //~ "user\n"
    //~ "$> "
    //~ "Goodbye!\n"
    //~ "Unknown command: "
    //~ "\nType 'help' for available commands\n"
    //~ "Usage: echo <text>\n";

//~ static void print_line_range(int start_line, int end_line) {
    //~ const char *p = shell_strings;
    //~ const char *start = shell_strings;
    //~ int line = 0;

    //~ while (*p) {
        //~ if (line == start_line)
            //~ start = p;

        //~ if (*p == '\n') {
            //~ if (line >= start_line && line <= end_line) {
                //~ const char *end = p + 1;
                //~ sys_write(1, start, end - start);
            //~ }
            //~ line++;
        //~ }
        //~ p++;
    //~ }
//~ }

//~ static void cmd_help(void) {
    //~ // lines: 2..8 (0: banner, 1: intro)
    //~ print_line_range(2, 8);
//~ }

//~ static void cmd_clear(void) {
    //~ for (int i = 0; i < 20; i++)
        //~ sys_write(1, "\n", 1);
//~ }

//~ static void cmd_whoami(void) {
    //~ // the "user\n" line is line 9
    //~ print_line_range(9, 9);
//~ }

//~ static void cmd_version(void) {
    //~ // banner is line 0
    //~ print_line_range(0, 0);
//~ }

//~ static void cmd_echo(char* arg) {
    //~ if (arg && arg[0]) {
        //~ sys_write(1, arg, strlen(arg));
        //~ sys_write(1, "\n", 1);
    //~ } else {
        //~ // last line: "Usage: echo <text>\n" (line 13)
        //~ print_line_range(13, 13);
    //~ }
//~ }

//~ void _start(void) {
    //~ // banner (0) + intro (1)
    //~ print_line_range(0, 1);

    //~ char cmd_buffer[128];
    //~ char* cmd;
    //~ char* arg;

    //~ while (1) {
        //~ // the "$> " is right after "user\n", so just write it directly:
        //~ sys_write(1, "$> ", 3);

        //~ int len = read_line(cmd_buffer, sizeof(cmd_buffer));
        //~ if (len == 0) continue;

        //~ cmd = cmd_buffer;
        //~ arg = cmd_buffer;

        //~ while (*arg == ' ') arg++;
        //~ cmd = arg;

        //~ while (*arg && *arg != ' ') arg++;

        //~ if (*arg == ' ') {
            //~ *arg = '\0';
            //~ arg++;
            //~ while (*arg == ' ') arg++;
        //~ } else {
            //~ arg = 0;
        //~ }

        //~ if (strcmp(cmd, "help") == 0) {
            //~ cmd_help();
        //~ } else if (strcmp(cmd, "exit") == 0) {
            //~ // "Goodbye!\n" is line 10
            //~ print_line_range(10, 10);
            //~ sys_exit(0);
        //~ } else if (strcmp(cmd, "clear") == 0) {
            //~ cmd_clear();
        //~ } else if (strcmp(cmd, "whoami") == 0) {
            //~ cmd_whoami();
        //~ } else if (strcmp(cmd, "version") == 0) {
            //~ cmd_version();
        //~ } else if (strcmp(cmd, "echo") == 0) {
            //~ cmd_echo(arg);
        //~ } else {
            //~ // "Unknown command: " + hint lines
            //~ sys_write(1, "Unknown command: ", 17);
            //~ sys_write(1, cmd, strlen(cmd));
            //~ print_line_range(11, 12); // the two hint lines
        //~ }
    //~ }
//~ }






//~ #include "include/userlib.h"

//~ // ============================================================
//~ // All strings in one block
//~ // ============================================================

//~ static const char shell_strings[] =
    //~ "User Shell v0.1\n"
    //~ "Type 'help' for commands\n"
    //~ "Commands:\n"
    //~ "  help    - Show this help\n"
    //~ "  exit    - Exit shell\n"
    //~ "  clear   - Clear screen\n"
    //~ "  whoami  - Show current user\n"
    //~ "  version - Show version\n"
    //~ "  echo    - Echo text back\n"
    //~ "user\n"
    //~ "$> "
    //~ "Goodbye!\n"
    //~ "Unknown command: "
    //~ "\nType 'help' for available commands\n"
    //~ "Usage: echo <text>\n";

//~ // offsets and lengths, derived from the literals above
//~ enum {
    //~ OFF_BANNER      = 0,
    //~ LEN_BANNER      = sizeof("User Shell v0.1\n") - 1,

    //~ OFF_INTRO       = OFF_BANNER + LEN_BANNER,
    //~ LEN_INTRO       = sizeof("Type 'help' for commands\n") - 1,

    //~ OFF_CMDS_HDR    = OFF_INTRO + LEN_INTRO,
    //~ LEN_CMDS_HDR    = sizeof("Commands:\n") - 1,

    //~ OFF_HELP        = OFF_CMDS_HDR + LEN_CMDS_HDR,
    //~ LEN_HELP        = sizeof("  help    - Show this help\n") - 1,

    //~ OFF_EXIT        = OFF_HELP + LEN_HELP,
    //~ LEN_EXIT        = sizeof("  exit    - Exit shell\n") - 1,

    //~ OFF_CLEAR       = OFF_EXIT + LEN_EXIT,
    //~ LEN_CLEAR       = sizeof("  clear   - Clear screen\n") - 1,

    //~ OFF_WHOAMI      = OFF_CLEAR + LEN_CLEAR,
    //~ LEN_WHOAMI      = sizeof("  whoami  - Show current user\n") - 1,

    //~ OFF_VERSION     = OFF_WHOAMI + LEN_WHOAMI,
    //~ LEN_VERSION     = sizeof("  version - Show version\n") - 1,

    //~ OFF_ECHO        = OFF_VERSION + LEN_VERSION,
    //~ LEN_ECHO        = sizeof("  echo    - Echo text back\n") - 1,

    //~ OFF_USER        = OFF_ECHO + LEN_ECHO,
    //~ LEN_USER        = sizeof("user\n") - 1,

    //~ OFF_PROMPT      = OFF_USER + LEN_USER,
    //~ LEN_PROMPT      = sizeof("$> ") - 1,

    //~ OFF_GOODBYE     = OFF_PROMPT + LEN_PROMPT,
    //~ LEN_GOODBYE     = sizeof("Goodbye!\n") - 1,

    //~ OFF_UNKNOWN     = OFF_GOODBYE + LEN_GOODBYE,
    //~ LEN_UNKNOWN     = sizeof("Unknown command: ") - 1,

    //~ OFF_HELP_HINT   = OFF_UNKNOWN + LEN_UNKNOWN,
    //~ LEN_HELP_HINT   = sizeof("\nType 'help' for available commands\n") - 1,

    //~ OFF_ECHO_USAGE  = OFF_HELP_HINT + LEN_HELP_HINT,
    //~ LEN_ECHO_USAGE  = sizeof("Usage: echo <text>\n") - 1,
//~ };

//~ // ============================================================
//~ // Command handlers
//~ // ============================================================

//~ static void cmd_help(void) {
    //~ sys_write(1, shell_strings + OFF_CMDS_HDR, LEN_CMDS_HDR);
    //~ sys_write(1, shell_strings + OFF_HELP,     LEN_HELP);
    //~ sys_write(1, shell_strings + OFF_EXIT,     LEN_EXIT);
    //~ sys_write(1, shell_strings + OFF_CLEAR,    LEN_CLEAR);
    //~ sys_write(1, shell_strings + OFF_WHOAMI,   LEN_WHOAMI);
    //~ sys_write(1, shell_strings + OFF_VERSION,  LEN_VERSION);
    //~ sys_write(1, shell_strings + OFF_ECHO,     LEN_ECHO);
//~ }

//~ static void cmd_clear(void) {
    //~ for (int i = 0; i < 20; i++)
        //~ sys_write(1, "\n", 1);
//~ }

//~ static void cmd_whoami(void) {
    //~ sys_write(1, shell_strings + OFF_USER, LEN_USER);
//~ }

//~ static void cmd_version(void) {
    //~ sys_write(1, shell_strings + OFF_BANNER, LEN_BANNER);
//~ }

//~ static void cmd_echo(char* arg) {
    //~ if (arg && arg[0]) {
        //~ sys_write(1, arg, strlen(arg));
        //~ sys_write(1, "\n", 1);
    //~ } else {
        //~ sys_write(1, shell_strings + OFF_ECHO_USAGE, LEN_ECHO_USAGE);
    //~ }
//~ }

//~ // ============================================================
//~ // Main shell entry point
//~ // ============================================================

//~ void _start(void) {
    //~ sys_write(1, shell_strings + OFF_BANNER, LEN_BANNER);
    //~ sys_write(1, shell_strings + OFF_INTRO,  LEN_INTRO);
    
    //~ char cmd_buffer[128];
    //~ char* cmd;
    //~ char* arg;
    
    //~ while (1) {
        //~ sys_write(1, shell_strings + OFF_PROMPT, LEN_PROMPT);
        
        //~ int len = read_line(cmd_buffer, sizeof(cmd_buffer));
        //~ if (len == 0) continue;
        
        //~ cmd = cmd_buffer;
        //~ arg = cmd_buffer;
        
        //~ while (*arg == ' ') arg++;
        //~ cmd = arg;
        
        //~ while (*arg && *arg != ' ') arg++;
        
        //~ if (*arg == ' ') {
            //~ *arg = '\0';
            //~ arg++;
            //~ while (*arg == ' ') arg++;
        //~ } else {
            //~ arg = 0;
        //~ }
        
        //~ if (strcmp(cmd, "help") == 0) {
            //~ cmd_help();
        //~ } else if (strcmp(cmd, "exit") == 0) {
            //~ sys_write(1, shell_strings + OFF_GOODBYE, LEN_GOODBYE);
            //~ sys_exit(0);
        //~ } else if (strcmp(cmd, "clear") == 0) {
            //~ cmd_clear();
        //~ } else if (strcmp(cmd, "whoami") == 0) {
            //~ cmd_whoami();
        //~ } else if (strcmp(cmd, "version") == 0) {
            //~ cmd_version();
        //~ } else if (strcmp(cmd, "echo") == 0) {
            //~ cmd_echo(arg);
        //~ } else {
            //~ sys_write(1, shell_strings + OFF_UNKNOWN,   LEN_UNKNOWN);
            //~ sys_write(1, cmd, strlen(cmd));
            //~ sys_write(1, shell_strings + OFF_HELP_HINT, LEN_HELP_HINT);
        //~ }
    //~ }
//~ }





//~ // user_shell.c

//~ #include "include/userlib.h"

//~ static const char shell_strings[] =
    //~ "User Shell v0.1\n"
    //~ "Type 'help' for commands\n"
    //~ "Commands:\n"
    //~ "  help    - Show this help\n"
    //~ "  exit    - Exit shell\n"
    //~ "  clear   - Clear screen\n"
    //~ "  whoami  - Show current user\n"
    //~ "  version - Show version\n"
    //~ "  echo    - Echo text back\n"
    //~ "user\n"
    //~ "$> "
    //~ "Goodbye!\n"
    //~ "Unknown command: "
    //~ "\nType 'help' for available commands\n"
    //~ "Usage: echo <text>\n";

//~ void _start(void) {
    //~ sys_write(1, shell_strings, sizeof(shell_strings) - 1);
    //~ sys_exit(0);
//~ }














//~ #include "include/userlib.h"

//~ // ============================================================
//~ // All strings in one block, with compiler-computed offsets
//~ // ============================================================

//~ static const char shell_strings[] =
    //~ "User Shell v0.1\n"
    //~ "Type 'help' for commands\n"
    //~ "Commands:\n"
    //~ "  help    - Show this help\n"
    //~ "  exit    - Exit shell\n"
    //~ "  clear   - Clear screen\n"
    //~ "  whoami  - Show current user\n"
    //~ "  version - Show version\n"
    //~ "  echo    - Echo text back\n"
    //~ "user\n"
    //~ "$> "
    //~ "Goodbye!\n"
    //~ "Unknown command: "
    //~ "\nType 'help' for available commands\n"
    //~ "Usage: echo <text>\n";

//~ // offsets and lengths, derived from the literals above
//~ enum {
    //~ OFF_BANNER      = 0,
    //~ LEN_BANNER      = sizeof("User Shell v0.1\n") - 1,

    //~ OFF_INTRO       = OFF_BANNER + LEN_BANNER,
    //~ LEN_INTRO       = sizeof("Type 'help' for commands\n") - 1,

    //~ OFF_CMDS_HDR    = OFF_INTRO + LEN_INTRO,
    //~ LEN_CMDS_HDR    = sizeof("Commands:\n") - 1,

    //~ OFF_HELP        = OFF_CMDS_HDR + LEN_CMDS_HDR,
    //~ LEN_HELP        = sizeof("  help    - Show this help\n") - 1,

    //~ OFF_EXIT        = OFF_HELP + LEN_HELP,
    //~ LEN_EXIT        = sizeof("  exit    - Exit shell\n") - 1,

    //~ OFF_CLEAR       = OFF_EXIT + LEN_EXIT,
    //~ LEN_CLEAR       = sizeof("  clear   - Clear screen\n") - 1,

    //~ OFF_WHOAMI      = OFF_CLEAR + LEN_CLEAR,
    //~ LEN_WHOAMI      = sizeof("  whoami  - Show current user\n") - 1,

    //~ OFF_VERSION     = OFF_WHOAMI + LEN_WHOAMI,
    //~ LEN_VERSION     = sizeof("  version - Show version\n") - 1,

    //~ OFF_ECHO        = OFF_VERSION + LEN_VERSION,
    //~ LEN_ECHO        = sizeof("  echo    - Echo text back\n") - 1,

    //~ OFF_USER        = OFF_ECHO + LEN_ECHO,
    //~ LEN_USER        = sizeof("user\n") - 1,

    //~ OFF_PROMPT      = OFF_USER + LEN_USER,
    //~ LEN_PROMPT      = sizeof("$> ") - 1,

    //~ OFF_GOODBYE     = OFF_PROMPT + LEN_PROMPT,
    //~ LEN_GOODBYE     = sizeof("Goodbye!\n") - 1,

    //~ OFF_UNKNOWN     = OFF_GOODBYE + LEN_GOODBYE,
    //~ LEN_UNKNOWN     = sizeof("Unknown command: ") - 1,

    //~ OFF_HELP_HINT   = OFF_UNKNOWN + LEN_UNKNOWN,
    //~ LEN_HELP_HINT   = sizeof("\nType 'help' for available commands\n") - 1,

    //~ OFF_ECHO_USAGE  = OFF_HELP_HINT + LEN_HELP_HINT,
    //~ LEN_ECHO_USAGE  = sizeof("Usage: echo <text>\n") - 1,
//~ };

//~ // ============================================================
//~ // Command handlers
//~ // ============================================================

//static void cmd_help(void) {
//    sys_write(1, shell_strings + OFF_CMDS_HDR, LEN_CMDS_HDR);
//    sys_write(1, shell_strings + OFF_HELP,     LEN_HELP);
//    sys_write(1, shell_strings + OFF_EXIT,     LEN_EXIT);
//    sys_write(1, shell_strings + OFF_CLEAR,    LEN_CLEAR);
//    sys_write(1, shell_strings + OFF_WHOAMI,   LEN_WHOAMI);
//    sys_write(1, shell_strings + OFF_VERSION,  LEN_VERSION);
//    sys_write(1, shell_strings + OFF_ECHO,     LEN_ECHO);
//}
//~ static void cmd_help(void) {
    //~ // TEMPORARY: dump the entire string block
    //~ sys_write(1, shell_strings, sizeof(shell_strings) - 1);
//~ }

//~ static void cmd_clear(void) {
    //~ for (int i = 0; i < 20; i++)
        //~ sys_write(1, "\n", 1);
//~ }

//~ static void cmd_whoami(void) {
    //~ sys_write(1, shell_strings + OFF_USER, LEN_USER);
//~ }

//~ static void cmd_version(void) {
    //~ sys_write(1, shell_strings + OFF_BANNER, LEN_BANNER);
//~ }

//~ static void cmd_echo(char* arg) {
    //~ if (arg && arg[0]) {
        //~ sys_write(1, arg, strlen(arg));
        //~ sys_write(1, "\n", 1);
    //~ } else {
        //~ sys_write(1, shell_strings + OFF_ECHO_USAGE, LEN_ECHO_USAGE);
    //~ }
//~ }

//~ // ============================================================
//~ // Main shell entry point
//~ // ============================================================

//~ void _start(void) {
    //~ sys_write(1, shell_strings + OFF_BANNER, LEN_BANNER);
    //~ sys_write(1, shell_strings + OFF_INTRO,  LEN_INTRO);
    
    //~ char cmd_buffer[128];
    //~ char* cmd;
    //~ char* arg;
    
    //~ while (1) {
        //~ sys_write(1, shell_strings + OFF_PROMPT, LEN_PROMPT);
        
        //~ int len = read_line(cmd_buffer, sizeof(cmd_buffer));
        //~ if (len == 0) continue;
        
        //~ cmd = cmd_buffer;
        //~ arg = cmd_buffer;
        
        //~ while (*arg == ' ') arg++;
        //~ cmd = arg;
        
        //~ while (*arg && *arg != ' ') arg++;
        
        //~ if (*arg == ' ') {
            //~ *arg = '\0';
            //~ arg++;
            //~ while (*arg == ' ') arg++;
        //~ } else {
            //~ arg = 0;
        //~ }
        
        //~ if (strcmp(cmd, "help") == 0) {
            //~ cmd_help();
        //~ } else if (strcmp(cmd, "exit") == 0) {
            //~ sys_write(1, shell_strings + OFF_GOODBYE, LEN_GOODBYE);
            //~ sys_exit(0);
        //~ } else if (strcmp(cmd, "clear") == 0) {
            //~ cmd_clear();
        //~ } else if (strcmp(cmd, "whoami") == 0) {
            //~ cmd_whoami();
        //~ } else if (strcmp(cmd, "version") == 0) {
            //~ cmd_version();
        //~ } else if (strcmp(cmd, "echo") == 0) {
            //~ cmd_echo(arg);
        //~ } else {
            //~ sys_write(1, shell_strings + OFF_UNKNOWN,   LEN_UNKNOWN);
            //~ sys_write(1, cmd, strlen(cmd));
            //~ sys_write(1, shell_strings + OFF_HELP_HINT, LEN_HELP_HINT);
        //~ }
    //~ }
//~ }

























//~ #include "include/userlib.h"

//~ // ============================================================
//~ // All strings in one block
//~ // ============================================================

//~ static const char shell_strings[] =
    //~ "User Shell v0.1\n"                              // 0–15   len 16
    //~ "Type 'help' for commands\n"                     // 16–40  len 25
    //~ "Commands:\n"                                    // 41–50  len 10
    //~ "  help    - Show this help\n"                   // 51–77  len 27
    //~ "  exit    - Exit shell\n"                       // 78–100 len 23
    //~ "  clear   - Clear screen\n"                     // 101–125 len 25
    //~ "  whoami  - Show current user\n"                // 126–155 len 30
    //~ "  version - Show version\n"                     // 156–180 len 25
    //~ "  echo    - Echo text back\n"                   // 181–207 len 27
    //~ "user\n"                                         // 208–212 len 5
    //~ "$> "                                            // 213–215 len 3
    //~ "Goodbye!\n"                                     // 216–224 len 9
    //~ "Unknown command: "                              // 225–241 len 17
    //~ "\nType 'help' for available commands\n"         // 242–277 len 36
    //~ "Usage: echo <text>\n";                          // 278–296 len 19

//~ // ============================================================
//~ // Command handlers
//~ // ============================================================

//~ static void cmd_help(void) {
    //~ sys_write(1, shell_strings + 41, 10);   // "Commands:\n"
    //~ sys_write(1, shell_strings + 51, 27);   // "  help    - Show this help\n"
    //~ sys_write(1, shell_strings + 78, 23);   // "  exit    - Exit shell\n"
    //~ sys_write(1, shell_strings + 101, 25);  // "  clear   - Clear screen\n"
    //~ sys_write(1, shell_strings + 126, 30);  // "  whoami  - Show current user\n"
    //~ sys_write(1, shell_strings + 156, 25);  // "  version - Show version\n"
    //~ sys_write(1, shell_strings + 181, 27);  // "  echo    - Echo text back\n"
//~ }

//~ static void cmd_clear(void) {
    //~ for (int i = 0; i < 20; i++)
        //~ sys_write(1, "\n", 1);
//~ }

//~ static void cmd_whoami(void) {
    //~ sys_write(1, shell_strings + 208, 5);   // "user\n"
//~ }

//~ static void cmd_version(void) {
    //~ sys_write(1, shell_strings + 0, 16);    // "User Shell v0.1\n"
//~ }

//~ static void cmd_echo(char* arg) {
    //~ if (arg && arg[0]) {
        //~ sys_write(1, arg, strlen(arg));
        //~ sys_write(1, "\n", 1);
    //~ } else {
        //~ sys_write(1, shell_strings + 278, 19); // "Usage: echo <text>\n"
    //~ }
//~ }

//~ // ============================================================
//~ // Main shell entry point
//~ // ============================================================

//~ void _start(void) {
    //~ sys_write(1, shell_strings + 0, 16);   // "User Shell v0.1\n"
    //~ sys_write(1, shell_strings + 16, 25);  // "Type 'help' for commands\n"
    
    //~ char cmd_buffer[128];
    //~ char* cmd;
    //~ char* arg;
    
    //~ while (1) {
        //~ sys_write(1, shell_strings + 213, 3); // "$> "
        
        //~ int len = read_line(cmd_buffer, sizeof(cmd_buffer));
        //~ if (len == 0) continue;
        
        //~ cmd = cmd_buffer;
        //~ arg = cmd_buffer;
        
        //~ while (*arg == ' ') arg++;
        //~ cmd = arg;
        
        //~ while (*arg && *arg != ' ') arg++;
        
        //~ if (*arg == ' ') {
            //~ *arg = '\0';
            //~ arg++;
            //~ while (*arg == ' ') arg++;
        //~ } else {
            //~ arg = 0;
        //~ }
        
        //~ if (strcmp(cmd, "help") == 0) {
            //~ cmd_help();
        //~ } else if (strcmp(cmd, "exit") == 0) {
            //~ sys_write(1, shell_strings + 216, 9); // "Goodbye!\n"
            //~ sys_exit(0);
        //~ } else if (strcmp(cmd, "clear") == 0) {
            //~ cmd_clear();
        //~ } else if (strcmp(cmd, "whoami") == 0) {
            //~ cmd_whoami();
        //~ } else if (strcmp(cmd, "version") == 0) {
            //~ cmd_version();
        //~ } else if (strcmp(cmd, "echo") == 0) {
            //~ cmd_echo(arg);
        //~ } else {
            //~ sys_write(1, shell_strings + 225, 17); // "Unknown command: "
            //~ sys_write(1, cmd, strlen(cmd));
            //~ sys_write(1, shell_strings + 242, 36); // "\nType 'help' for available commands\n"
        //~ }
    //~ }
//~ }






//~ // ============================================================
//~ // All strings in one block - just like test_program
//~ // ============================================================

//~ static const char shell_strings[] = 
    //~ "User Shell v0.1\n"
    //~ "Type 'help' for commands\n"
    //~ "Commands:\n"
    //~ "  help    - Show this help\n"
    //~ "  exit    - Exit shell\n"
    //~ "  clear   - Clear screen\n"
    //~ "  whoami  - Show current user\n"
    //~ "  version - Show version\n"
    //~ "  echo    - Echo text back\n"
    //~ "user\n"
    //~ "$> "
    //~ "Goodbye!\n"
    //~ "Unknown command: "
    //~ "\nType 'help' for available commands\n"
    //~ "Usage: echo <text>\n";

//~ // ============================================================
//~ // Command handlers - using direct offsets
//~ // ============================================================

//~ static void cmd_help(void) {
    //~ sys_write(1, shell_strings + 34, 10);   // "Commands:\n"
    //~ sys_write(1, shell_strings + 44, 26);   // "  help    - Show this help\n"
    //~ sys_write(1, shell_strings + 70, 23);   // "  exit    - Exit shell\n"
    //~ sys_write(1, shell_strings + 93, 25);   // "  clear   - Clear screen\n"
    //~ sys_write(1, shell_strings + 118, 29);  // "  whoami  - Show current user\n"
    //~ sys_write(1, shell_strings + 147, 25);  // "  version - Show version\n"
    //~ sys_write(1, shell_strings + 172, 27);  // "  echo    - Echo text back\n"
//~ }

//~ static void cmd_clear(void) {
    //~ for (int i = 0; i < 20; i++) {
        //~ sys_write(1, "\n", 1);
    //~ }
//~ }

//~ static void cmd_whoami(void) {
    //~ sys_write(1, shell_strings + 199, 5);   // "user\n"
//~ }

//~ static void cmd_version(void) {
    //~ sys_write(1, shell_strings + 0, 16);    // "User Shell v0.1\n"
//~ }

//~ static void cmd_echo(char* arg) {
    //~ if (arg && arg[0]) {
        //~ sys_write(1, arg, strlen(arg));
        //~ sys_write(1, "\n", 1);
    //~ } else {
        //~ sys_write(1, shell_strings + 332, 19); // "Usage: echo <text>\n"
    //~ }
//~ }

//~ // ============================================================
//~ // Main shell entry point
//~ // ============================================================

//~ void _start(void) {
    //~ sys_write(1, shell_strings + 0, 16);     // "User Shell v0.1\n"
    //~ sys_write(1, shell_strings + 16, 25);    // "Type 'help' for commands\n"
    
    //~ char cmd_buffer[128];
    //~ char* cmd;
    //~ char* arg;
    
    //~ while (1) {
        //~ sys_write(1, shell_strings + 204, 3); // "$> "
        
        //~ int len = read_line(cmd_buffer, sizeof(cmd_buffer));
        
        //~ // Debug: print the length returned by read_line
        //~ sys_write(1, "len=", 4);
        //~ print_dec(len);
        //~ sys_write(1, "\n", 1);
        
        //~ if (len == 0) continue;
        
        //~ // Debug: print the raw buffer
        //~ sys_write(1, "buf='", 5);
        //~ sys_write(1, cmd_buffer, len);
        //~ sys_write(1, "'\n", 2);
        
        //~ // Parse command and argument
        //~ cmd = cmd_buffer;
        //~ arg = cmd_buffer;
        
        //~ while (*arg == ' ') arg++;
        //~ cmd = arg;
        
        //~ while (*arg && *arg != ' ') arg++;
        
        //~ if (*arg == ' ') {
            //~ *arg = '\0';
            //~ arg++;
            //~ while (*arg == ' ') arg++;
        //~ } else {
            //~ arg = 0;
        //~ }
        
        //~ // Debug: print parsed command
        //~ sys_write(1, "cmd='", 5);
        //~ sys_write(1, cmd, strlen(cmd));
        //~ sys_write(1, "'\n", 2);
        
        //~ // Handle commands - use strcmp
        //~ if (strcmp(cmd, "help") == 0) {
            //~ cmd_help();
        //~ } else if (strcmp(cmd, "exit") == 0) {
            //~ sys_write(1, shell_strings + 207, 9); // "Goodbye!\n"
            //~ sys_exit(0);
        //~ } else if (strcmp(cmd, "clear") == 0) {
            //~ cmd_clear();
        //~ } else if (strcmp(cmd, "whoami") == 0) {
            //~ cmd_whoami();
        //~ } else if (strcmp(cmd, "version") == 0) {
            //~ cmd_version();
        //~ } else if (strcmp(cmd, "echo") == 0) {
            //~ cmd_echo(arg);
        //~ } else {
            //~ sys_write(1, shell_strings + 216, 17); // "Unknown command: "
            //~ sys_write(1, cmd, strlen(cmd));
            //~ sys_write(1, shell_strings + 233, 35); // "\nType 'help' for available commands\n"
        //~ }
    //~ }
//~ }
