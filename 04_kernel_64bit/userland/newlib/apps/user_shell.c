/*
 * user_shell.c — dons-os user shell, v0.7.0 REPL with argv.
 *
 * Line-oriented REPL.  Reads a line, tokenizes on whitespace, resolves
 * the first token to 0:/NAME.ELF, spawns it via SYS_EXEC with the full
 * argv, and waits via SYS_WAITPID.
 *
 * Built-ins: exit, help, reboot.
 *
 * Stage 4 (v0.7.0): argv is now passed to spawned programs.  cat and
 * echo are external and use it.
 */

#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include "donsdos.h"

#define LINE_MAX   256
#define ARGV_MAX   16

static int readline(char *buf, int cap) {
    int len = 0;
    for (;;) {
        char c;
        int n = read(0, &c, 1);
        if (n <= 0) return -1;
        if (c == '\r' || c == '\n') {
            write(1, "\r\n", 2);
            buf[len] = '\0';
            return len;
        }
        if (c == '\b' || c == 0x7f) {
            if (len > 0) {
                len--;
                write(1, "\b \b", 3);
            }
            continue;
        }
        if (c < 0x20 || c > 0x7e) continue;
        if (len + 1 >= cap) continue;
        buf[len++] = c;
        write(1, &c, 1);
    }
}

static int tokenize(char *buf, char **argv, int max) {
    int argc = 0;
    char *p = buf;
    while (*p != '\0' && argc < max - 1) {
        while (*p == ' ' || *p == '\t') p++;
        if (*p == '\0') break;
        argv[argc++] = p;
        while (*p != '\0' && *p != ' ' && *p != '\t') p++;
        if (*p != '\0') {
            *p = '\0';
            p++;
        }
    }
    argv[argc] = NULL;
    return argc;
}

static void print_help(void) {
    printf("dons-os shell (v0.7.0)\n");
    printf("  Built-ins: exit, help, reboot\n");
    printf("  Anything else is resolved to 0:/NAME.ELF and run.\n");
    printf("  Examples: memtest, ls, cat HELLO-WORLD.TXT, echo hello\n");
}

static int run_external(int argc, char **argv) {
    char path[64];
    int n = snprintf(path, sizeof(path), "0:/%s.ELF", argv[0]);
    if (n < 0 || n >= (int)sizeof(path)) {
        printf("shell: command name too long\n");
        return -1;
    }

    int pid = spawn(path, argc, argv);
    if (pid < 0) {
        printf("shell: %s: command not found\n", argv[0]);
        return -1;
    }

    int status = 0;
    int reaped = waitpid(pid, &status, 0);
    if (reaped < 0) {
        printf("shell: %s: waitpid failed\n", argv[0]);
        return -1;
    }
    if (status != 0) {
        printf("shell: %s: exit %d\n", argv[0], status);
    }
    return 0;
}

int main(int argc, char **argv) {
    (void)argc;
    (void)argv;

    setvbuf(stdout, NULL, _IONBF, 0);

    char line[LINE_MAX];
    char *tokens[ARGV_MAX];

    for (;;) {
        write(1, "] ", 2);

        int n = readline(line, sizeof(line));
        if (n < 0) {
            write(1, "\r\n", 2);
            return 0;
        }

        int argc2 = tokenize(line, tokens, ARGV_MAX);
        if (argc2 == 0) continue;

        const char *cmd = tokens[0];

        if (strcmp(cmd, "exit") == 0) return 0;
        if (strcmp(cmd, "help") == 0) { print_help(); continue; }
        if (strcmp(cmd, "reboot") == 0) {
            printf("rebooting...\n");
            sys_reboot();
            continue;
        }

        run_external(argc2, tokens);
    }

    return 0;
}
