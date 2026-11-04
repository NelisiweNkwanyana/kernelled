/*
 * shell/shell.c - read a line, split it into words, run the command.
 *
 * The shell is an ordinary process. While it waits for a key it is BLOCKED
 * inside uart_getc(), so it uses no CPU time; the UART interrupt wakes it.
 */
#include <stddef.h>
#include <stdint.h>

#include "arch/aarch64/cpu.h"
#include "drivers/timer.h"
#include "drivers/uart.h"
#include "filesystem/fs.h"
#include "kernel/printk.h"
#include "kernel/string.h"
#include "kernel/version.h"
#include "memory/heap.h"
#include "memory/pmm.h"
#include "process/process.h"
#include "scheduler/scheduler.h"
#include "shell/shell.h"

#define LINE_MAX  200
#define ARGS_MAX  16

static int cwd = FS_ROOT;               /* current directory (a file system node) */

/* ---- Reading a line ------------------------------------------------------ */

/*
 * Read one line with echo. Supports Backspace/Delete, Ctrl-C (discard the
 * line) and Ctrl-U (erase the line). Escape sequences such as arrow keys
 * (ESC [ A) are swallowed so they do not appear as garbage.
 */
static void read_line(char *line, size_t size)
{
    size_t length = 0;
    for (;;) {
        char c = uart_getc();
        if (c == '\r' || c == '\n') {
            kprintf("\n");
            line[length] = '\0';
            return;
        }
        if (c == 0x7F || c == 0x08) {               /* Delete / Backspace */
            if (length > 0) {
                length--;
                kprintf("\b \b");
            }
        } else if (c == 0x03) {                     /* Ctrl-C */
            kprintf("^C\n");
            line[0] = '\0';
            return;
        } else if (c == 0x15) {                     /* Ctrl-U */
            while (length > 0) {
                length--;
                kprintf("\b \b");
            }
        } else if (c == 0x1B) {                     /* ESC [ ... final byte */
            char next = uart_getc();
            if (next == '[') {
                do {
                    next = uart_getc();
                } while (next >= '0' && next <= '?');   /* parameter bytes */
            }
        } else if (c >= ' ' && c < 0x7F && length + 1 < size) {
            line[length++] = c;
            kprintf("%c", c);
        }
    }
}

/* Split into words in place. "double quotes" keep spaces together. */
static int split(char *line, char *argv[], int max)
{
    int argc = 0;
    char *p = line;
    while (*p && argc < max) {
        while (*p == ' ' || *p == '\t') {
            p++;
        }
        if (*p == '\0') {
            break;
        }
        if (*p == '"') {
            argv[argc++] = ++p;
            while (*p && *p != '"') {
                p++;
            }
        } else {
            argv[argc++] = p;
            while (*p && *p != ' ' && *p != '\t') {
                p++;
            }
        }
        if (*p) {
            *p++ = '\0';
        }
    }
    return argc;
}

/* ---- Helpers ------------------------------------------------------------- */

static void print_error(const char *command, const char *what, int error)
{
    kprintf("%s: %s: %s\n", command, what, fs_error_string(error));
}

/* Print ticks (1/100 s) as seconds with two decimals. */
static void print_seconds(uint64_t ticks)
{
    unsigned int hz = timer_tick_hz();
    kprintf("%lu.%02lu", ticks / hz, (ticks % hz) * 100 / hz);
}

static int parse_number(const char *text, uint64_t *value)
{
    uint64_t result = 0;
    if (text == 0 || *text == '\0') {
        return 0;
    }
    for (; *text; text++) {
        if (*text < '0' || *text > '9' || result > 1000000000UL) {
            return 0;
        }
        result = result * 10 + (uint64_t)(*text - '0');
    }
    *value = result;
    return 1;
}

static void prompt(void)
{
    char path[FS_PATH_MAX];
    fs_path(cwd, path, sizeof(path));
    kprintf("kernelled:%s$ ", path);
}

/* ---- Commands ------------------------------------------------------------ */

struct command {
    const char *name;
    const char *usage;
    const char *help;
    void (*run)(int argc, char *argv[]);
};

static void cmd_help(int argc, char *argv[]);

static void cmd_clear(int argc, char *argv[])
{
    (void)argc;
    (void)argv;
    kprintf("\x1b[2J\x1b[H");           /* ANSI: erase screen, cursor to top-left */
}

static void cmd_about(int argc, char *argv[])
{
    (void)argc;
    (void)argv;
    uint64_t midr;
    __asm__ volatile("mrs %0, midr_el1" : "=r"(midr));
    kprintf("KERNELLED %s\n", KERNELLED_VERSION);
    kprintf("An educational operating system for 64-bit ARM, written in C and assembly.\n");
    kprintf("  machine     QEMU virt, CPU MIDR_EL1 0x%08lx, running at EL%u\n",
            midr, cpu_current_el());
    kprintf("  memory      %lu MiB RAM, 4 KiB pages, MMU on (identity map, W^X)\n",
            pmm_ram_size() >> 20);
    kprintf("  interrupts  GICv2; timer %lu Hz counter, %u Hz tick\n",
            timer_frequency(), timer_tick_hz());
    kprintf("  scheduling  preemptive round-robin, %u ms time slice, up to %u processes\n",
            (unsigned int)(SCHEDULER_SLICE_TICKS * 1000 / timer_tick_hz()),
            (unsigned int)MAX_PROCESSES);
    kprintf("  files       in-memory file system (contents are lost at power-off)\n");
}

static void cmd_meminfo(int argc, char *argv[])
{
    (void)argc;
    (void)argv;
    uint64_t total = pmm_total_pages();
    uint64_t free = pmm_free_page_count();
    uint64_t reserved = pmm_reserved_pages();
    struct heap_stats heap;
    heap_get_stats(&heap);
    int nodes;
    size_t bytes;
    fs_usage(&nodes, &bytes);

    kprintf("Physical memory   %p - %p\n", (void *)pmm_ram_base(),
            (void *)(pmm_ram_base() + pmm_ram_size()));
    kprintf("  total     %8lu KiB  (%lu pages of 4 KiB)\n", total * 4, total);
    kprintf("  reserved  %8lu KiB  (device tree + kernel image)\n", reserved * 4);
    kprintf("  in use    %8lu KiB  (heap, process stacks)\n", (total - free - reserved) * 4);
    kprintf("  free      %8lu KiB\n", free * 4);
    kprintf("Kernel heap       %lu KiB\n", heap.total_bytes / 1024);
    kprintf("  in use    %8lu bytes in %lu allocations\n", heap.used_bytes, heap.allocations);
    kprintf("  free      %8lu bytes in %lu blocks\n", heap.free_bytes, heap.free_blocks);
    kprintf("File system       %d of %d nodes, %lu bytes of file data\n",
            nodes, FS_MAX_NODES, (uint64_t)bytes);
}

static void cmd_ps(int argc, char *argv[])
{
    (void)argc;
    (void)argv;
    kprintf("  PID  STATE      CPU TIME   RUNS  NAME\n");
    for (int i = 0; i < MAX_PROCESSES; i++) {
        /* Copy the entry with IRQs masked so we print a consistent snapshot. */
        uint64_t flags = cpu_irq_save();
        struct process p = *process_slot(i);
        cpu_irq_restore(flags);
        if (p.state == PROCESS_UNUSED) {
            continue;
        }
        kprintf("  %3d  %-9s  %7lu.%02lus %6lu  %s\n", p.pid, process_state_name(p.state),
                p.ticks_run / timer_tick_hz(), (p.ticks_run % timer_tick_hz()) * 100 / timer_tick_hz(),
                p.times_scheduled, p.name);
    }
    kprintf("Context switches since boot: %lu\n", scheduler_switch_count());
}

static void cmd_uptime(int argc, char *argv[])
{
    (void)argc;
    (void)argv;
    uint64_t ticks = timer_ticks();
    uint64_t seconds = ticks / timer_tick_hz();
    kprintf("up %luh %02lum %02lus (", seconds / 3600, (seconds / 60) % 60, seconds % 60);
    print_seconds(ticks);
    kprintf(" s, %lu ticks)\n", ticks);
}

static void list_entry(int node)
{
    struct fs_info info;
    if (fs_info(node, &info) != FS_OK) {
        return;
    }
    if (info.type == FS_DIRECTORY) {
        kprintf("  %-6s  %8lu items  %s/\n", "dir", (uint64_t)info.size, info.name);
    } else {
        kprintf("  %-6s  %8lu bytes  %s\n", "file", (uint64_t)info.size, info.name);
    }
}

static void cmd_ls(int argc, char *argv[])
{
    const char *path = argc > 1 ? argv[1] : ".";
    int node = fs_lookup(path, cwd);
    if (node < 0) {
        print_error("ls", path, node);
        return;
    }
    struct fs_info info;
    fs_info(node, &info);
    if (info.type == FS_FILE) {
        list_entry(node);
        return;
    }
    if (fs_first_child(node) < 0) {
        kprintf("  (empty)\n");
    }
    for (int child = fs_first_child(node); child >= 0; child = fs_next_sibling(child)) {
        list_entry(child);
    }
}

static void cmd_mkdir(int argc, char *argv[])
{
    if (argc < 2) {
        kprintf("usage: mkdir <directory>...\n");
    }
    for (int i = 1; i < argc; i++) {
        int result = fs_mkdir(argv[i], cwd);
        if (result < 0) {
            print_error("mkdir", argv[i], result);
        }
    }
}

static void cmd_touch(int argc, char *argv[])
{
    if (argc < 2) {
        kprintf("usage: touch <file>...\n");
    }
    for (int i = 1; i < argc; i++) {
        int result = fs_touch(argv[i], cwd);
        if (result < 0) {
            print_error("touch", argv[i], result);
        }
    }
}

static void cmd_cat(int argc, char *argv[])
{
    if (argc < 2) {
        kprintf("usage: cat <file>...\n");
    }
    for (int i = 1; i < argc; i++) {
        int node = fs_lookup(argv[i], cwd);
        if (node < 0) {
            print_error("cat", argv[i], node);
            continue;
        }
        char buffer[129];
        size_t offset = 0;
        char last = '\n';
        int count;
        while ((count = fs_read(node, offset, buffer, sizeof(buffer) - 1)) > 0) {
            buffer[count] = '\0';
            kprintf("%s", buffer);
            last = buffer[count - 1];
            offset += (size_t)count;
        }
        if (count < 0) {
            print_error("cat", argv[i], count);
        } else if (last != '\n') {
            kprintf("\n");              /* keep the prompt on its own line */
        }
    }
}

/* echo words...            print them
 * echo words... > file     replace the file's contents
 * echo words... >> file    append to the file */
static void cmd_echo(int argc, char *argv[])
{
    int redirect = 0, append = 0;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], ">") == 0 || strcmp(argv[i], ">>") == 0) {
            redirect = i;
            append = argv[i][1] == '>';
            break;
        }
    }
    int words_end = redirect ? redirect : argc;
    char text[LINE_MAX + 1];
    size_t length = 0;
    for (int i = 1; i < words_end; i++) {
        if (i > 1) {
            text[length++] = ' ';
        }
        length += strlcpy(text + length, argv[i], sizeof(text) - length - 1);
    }
    text[length++] = '\n';
    text[length] = '\0';

    if (!redirect) {
        kprintf("%s", text);
        return;
    }
    if (redirect + 1 >= argc) {
        kprintf("echo: missing file name after %s\n", argv[redirect]);
        return;
    }
    int result = fs_write(argv[redirect + 1], cwd, text, length, append);
    if (result < 0) {
        print_error("echo", argv[redirect + 1], result);
    }
}

static void cmd_rm(int argc, char *argv[])
{
    if (argc < 2) {
        kprintf("usage: rm <file or empty directory>...\n");
    }
    for (int i = 1; i < argc; i++) {
        int result = fs_remove(argv[i], cwd);
        if (result < 0) {
            print_error("rm", argv[i], result);
        }
    }
}

static void cmd_cd(int argc, char *argv[])
{
    const char *path = argc > 1 ? argv[1] : "/";
    int node = fs_lookup(path, cwd);
    struct fs_info info;
    if (node < 0) {
        print_error("cd", path, node);
    } else if (fs_info(node, &info) == FS_OK && info.type != FS_DIRECTORY) {
        print_error("cd", path, FS_NOT_DIRECTORY);
    } else {
        cwd = node;
    }
}

static void cmd_pwd(int argc, char *argv[])
{
    (void)argc;
    (void)argv;
    char path[FS_PATH_MAX];
    fs_path(cwd, path, sizeof(path));
    kprintf("%s\n", path);
}

/* A CPU-bound process that spins for some seconds, to watch the scheduler. */
static void spin_main(void *argument)
{
    uint64_t end = timer_ticks() + (uint64_t)(uintptr_t)argument * timer_tick_hz();
    while (timer_ticks() < end) {
        /* never sleeps: only preemption lets other processes run */
    }
}

static void cmd_spawn(int argc, char *argv[])
{
    uint64_t seconds = 10;
    if (argc > 1 && (!parse_number(argv[1], &seconds) || seconds == 0 || seconds > 3600)) {
        kprintf("usage: spawn [seconds 1-3600]\n");
        return;
    }
    int pid = process_create("spin", spin_main, (void *)(uintptr_t)seconds);
    if (pid < 0) {
        kprintf("spawn: process table full or out of memory\n");
        return;
    }
    kprintf("started process %d (spin) for %lu s; try 'ps'\n", pid, seconds);
}

static void cmd_kill(int argc, char *argv[])
{
    uint64_t pid;
    if (argc != 2 || !parse_number(argv[1], &pid)) {
        kprintf("usage: kill <pid>\n");
        return;
    }
    if (process_kill((int)pid) < 0) {
        kprintf("kill: cannot kill %lu (no such process, idle, or the shell itself)\n", pid);
    }
}

static void cmd_sleep(int argc, char *argv[])
{
    uint64_t seconds;
    if (argc != 2 || !parse_number(argv[1], &seconds) || seconds > 3600) {
        kprintf("usage: sleep <seconds>\n");
        return;
    }
    process_sleep(seconds * timer_tick_hz());
}

static const struct command commands[] = {
    { "help",    "help",                  "show this list",                          cmd_help },
    { "clear",   "clear",                 "clear the screen",                        cmd_clear },
    { "about",   "about",                 "about KERNELLED and this machine",        cmd_about },
    { "meminfo", "meminfo",               "physical memory, heap and file system use", cmd_meminfo },
    { "ps",      "ps",                    "list processes",                          cmd_ps },
    { "uptime",  "uptime",                "time since boot",                         cmd_uptime },
    { "ls",      "ls [path]",             "list a directory",                        cmd_ls },
    { "mkdir",   "mkdir <dir>...",        "create directories",                      cmd_mkdir },
    { "touch",   "touch <file>...",       "create empty files",                      cmd_touch },
    { "cat",     "cat <file>...",         "print files",                             cmd_cat },
    { "echo",    "echo <text> [> file]",  "print text, or write (>) / append (>>) it", cmd_echo },
    { "rm",      "rm <path>...",          "remove files or empty directories",       cmd_rm },
    { "cd",      "cd [dir]",              "change directory (default /)",            cmd_cd },
    { "pwd",     "pwd",                   "print the current directory",             cmd_pwd },
    { "spawn",   "spawn [seconds]",       "start a CPU-bound process (default 10 s)", cmd_spawn },
    { "kill",    "kill <pid>",            "stop a process",                          cmd_kill },
    { "sleep",   "sleep <seconds>",       "pause the shell",                         cmd_sleep },
    { 0, 0, 0, 0 },
};

static void cmd_help(int argc, char *argv[])
{
    (void)argc;
    (void)argv;
    kprintf("KERNELLED shell commands:\n");
    for (const struct command *c = commands; c->name; c++) {
        kprintf("  %-22s %s\n", c->usage, c->help);
    }
    kprintf("Keys: Backspace, Ctrl-U (erase line), Ctrl-C (cancel line). Quit QEMU: Ctrl-A, then X.\n");
}

/* ---- Main loop ----------------------------------------------------------- */

void shell_main(void *argument)
{
    (void)argument;
    uart_enable_rx_interrupt();

    int motd = fs_lookup("/etc/motd", FS_ROOT);
    char buffer[128];
    int count = motd >= 0 ? fs_read(motd, 0, buffer, sizeof(buffer) - 1) : 0;
    if (count > 0) {
        buffer[count] = '\0';
        kprintf("\n%s", buffer);
    }
    kprintf("Type 'help' for a list of commands.\n");

    char line[LINE_MAX];
    char *argv[ARGS_MAX];
    for (;;) {
        prompt();
        read_line(line, sizeof(line));
        int argc = split(line, argv, ARGS_MAX);
        if (argc == 0) {
            continue;
        }
        const struct command *c = commands;
        while (c->name && strcmp(c->name, argv[0]) != 0) {
            c++;
        }
        if (c->name) {
            c->run(argc, argv);
        } else {
            kprintf("%s: command not found (type 'help')\n", argv[0]);
        }
    }
}
