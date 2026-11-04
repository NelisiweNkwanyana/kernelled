/*
 * filesystem/fs.c - ramfs implementation.
 *
 * The tree is stored as "first child / next sibling" links in a node table:
 *
 *   /      first_child -> README -> etc -> home -> tmp   (next_sibling links)
 *   /etc   first_child -> motd -> version
 *
 * Each node also knows its parent, which makes ".." and fs_path() easy.
 * File data is a heap buffer whose capacity doubles when it runs out.
 */
#include <stddef.h>
#include <stdint.h>

#include "arch/aarch64/cpu.h"
#include "drivers/timer.h"
#include "filesystem/fs.h"
#include "kernel/string.h"
#include "kernel/version.h"
#include "memory/heap.h"

struct node {
    enum fs_type type;
    char name[FS_NAME_MAX];
    int parent;
    int first_child;            /* directories: -1 if empty */
    int next_sibling;           /* -1 if last in its directory */
    char *data;                 /* files: heap buffer, or 0 if empty */
    size_t size;
    size_t capacity;
    uint64_t modified_tick;
};

static struct node nodes[FS_MAX_NODES];

/*
 * The shell and the self-tests are separate processes and can be preempted
 * at any moment, so every operation runs with IRQs masked: a process switch
 * can never see the tree half-updated.
 */
#define LOCK()   uint64_t lock_flags_ = cpu_irq_save()
#define UNLOCK() cpu_irq_restore(lock_flags_)

static int valid(int n)
{
    return n >= 0 && n < FS_MAX_NODES && nodes[n].type != FS_FREE;
}

static int find_child(int directory, const char *name, size_t length)
{
    for (int c = nodes[directory].first_child; c >= 0; c = nodes[c].next_sibling) {
        if (strncmp(nodes[c].name, name, length) == 0 && nodes[c].name[length] == '\0') {
            return c;
        }
    }
    return FS_NOT_FOUND;
}

/*
 * Walk `path` from `cwd`. If `last` is non-null, stop before the final
 * component: return its parent directory and store the final name in `last`.
 */
static int walk(const char *path, int cwd, char *last)
{
    if (path == 0 || path[0] == '\0') {
        return FS_INVALID_NAME;
    }
    if (!valid(cwd) || nodes[cwd].type != FS_DIRECTORY) {
        cwd = FS_ROOT;
    }
    int at = path[0] == '/' ? FS_ROOT : cwd;
    const char *p = path;

    for (;;) {
        while (*p == '/') {
            p++;
        }
        if (*p == '\0') {
            if (last) {
                return FS_INVALID_NAME;     /* "a/" or "/": no final name */
            }
            return at;
        }
        const char *start = p;
        while (*p != '\0' && *p != '/') {
            p++;
        }
        size_t length = (size_t)(p - start);
        const char *rest = p;
        while (*rest == '/') {
            rest++;
        }
        int is_last = *rest == '\0';

        if (nodes[at].type != FS_DIRECTORY) {
            return FS_NOT_DIRECTORY;
        }
        if (is_last && last) {
            if (length >= FS_NAME_MAX || (length == 1 && start[0] == '.') ||
                (length == 2 && start[0] == '.' && start[1] == '.')) {
                return FS_INVALID_NAME;
            }
            memcpy(last, start, length);
            last[length] = '\0';
            return at;
        }
        if (length == 1 && start[0] == '.') {
            continue;
        }
        if (length == 2 && start[0] == '.' && start[1] == '.') {
            at = nodes[at].parent;
            continue;
        }
        int child = find_child(at, start, length);
        if (child < 0) {
            return FS_NOT_FOUND;
        }
        at = child;
    }
}

static int new_node(int parent, const char *name, enum fs_type type)
{
    for (int n = 1; n < FS_MAX_NODES; n++) {
        if (nodes[n].type == FS_FREE) {
            memset(&nodes[n], 0, sizeof(nodes[n]));
            nodes[n].type = type;
            strlcpy(nodes[n].name, name, FS_NAME_MAX);
            nodes[n].parent = parent;
            nodes[n].first_child = -1;
            nodes[n].modified_tick = timer_ticks();
            /* Append at the end of the parent's list, so ls shows creation order. */
            nodes[n].next_sibling = -1;
            int *link = &nodes[parent].first_child;
            while (*link >= 0) {
                link = &nodes[*link].next_sibling;
            }
            *link = n;
            nodes[parent].modified_tick = nodes[n].modified_tick;
            return n;
        }
    }
    return FS_NO_SPACE;
}

static int create(const char *path, int cwd, enum fs_type type, int exists_ok)
{
    char name[FS_NAME_MAX];
    int parent = walk(path, cwd, name);
    if (parent < 0) {
        return parent;
    }
    int existing = find_child(parent, name, strlen(name));
    if (existing >= 0) {
        if (exists_ok && nodes[existing].type == type) {
            nodes[existing].modified_tick = timer_ticks();
            return existing;
        }
        return FS_EXISTS;
    }
    return new_node(parent, name, type);
}

void fs_init(void)
{
    memset(nodes, 0, sizeof(nodes));
    nodes[FS_ROOT].type = FS_DIRECTORY;
    strlcpy(nodes[FS_ROOT].name, "/", FS_NAME_MAX);
    nodes[FS_ROOT].parent = FS_ROOT;              /* "/.." is "/" */
    nodes[FS_ROOT].first_child = -1;
    nodes[FS_ROOT].next_sibling = -1;

    static const char readme[] =
        "KERNELLED is an educational operating system for 64-bit ARM.\n"
        "It boots on QEMU's virt machine and is written in C and assembly.\n"
        "Type 'help' to see the shell commands.\n";
    static const char motd[] = "Welcome to KERNELLED.\n";

    fs_write("/README", FS_ROOT, readme, sizeof(readme) - 1, 0);
    fs_mkdir("/etc", FS_ROOT);
    fs_write("/etc/motd", FS_ROOT, motd, sizeof(motd) - 1, 0);
    fs_write("/etc/version", FS_ROOT, KERNELLED_VERSION "\n", sizeof(KERNELLED_VERSION), 0);
    fs_mkdir("/home", FS_ROOT);
    fs_mkdir("/tmp", FS_ROOT);
}

int fs_lookup(const char *path, int cwd)
{
    LOCK();
    int result = walk(path, cwd, 0);
    UNLOCK();
    return result;
}

int fs_mkdir(const char *path, int cwd)
{
    LOCK();
    int result = create(path, cwd, FS_DIRECTORY, 0);
    UNLOCK();
    return result;
}

int fs_touch(const char *path, int cwd)
{
    LOCK();
    int result = create(path, cwd, FS_FILE, 1);
    UNLOCK();
    return result;
}

int fs_write(const char *path, int cwd, const char *data, size_t length, int append)
{
    LOCK();
    int n = create(path, cwd, FS_FILE, 1);
    if (n < 0) {
        int existing = walk(path, cwd, 0);
        n = existing >= 0 && nodes[existing].type == FS_DIRECTORY ? FS_IS_DIRECTORY : n;
        UNLOCK();
        return n;
    }
    struct node *file = &nodes[n];
    size_t new_size = (append ? file->size : 0) + length;
    if (new_size > FS_MAX_FILE_SIZE) {
        UNLOCK();
        return FS_TOO_LARGE;
    }
    if (new_size > file->capacity) {
        size_t capacity = file->capacity ? file->capacity : 64;
        while (capacity < new_size) {
            capacity *= 2;
        }
        char *buffer = kmalloc(capacity);
        if (buffer == 0) {
            UNLOCK();
            return FS_NO_SPACE;
        }
        if (append && file->size) {
            memcpy(buffer, file->data, file->size);
        }
        kfree(file->data);
        file->data = buffer;
        file->capacity = capacity;
    }
    memcpy(file->data + (append ? file->size : 0), data, length);
    file->size = new_size;
    file->modified_tick = timer_ticks();
    UNLOCK();
    return n;
}

int fs_read(int node, size_t offset, char *buffer, size_t length)
{
    LOCK();
    int result;
    if (!valid(node)) {
        result = FS_NOT_FOUND;
    } else if (nodes[node].type == FS_DIRECTORY) {
        result = FS_IS_DIRECTORY;
    } else if (offset >= nodes[node].size) {
        result = 0;
    } else {
        size_t available = nodes[node].size - offset;
        size_t count = length < available ? length : available;
        memcpy(buffer, nodes[node].data + offset, count);
        result = (int)count;
    }
    UNLOCK();
    return result;
}

int fs_remove(const char *path, int cwd)
{
    LOCK();
    int n = walk(path, cwd, 0);
    int result = FS_OK;
    if (n < 0) {
        result = n;
    } else if (n == FS_ROOT) {
        result = FS_BUSY;
    } else {
        for (int a = cwd; valid(a); a = nodes[a].parent) {     /* cwd or a parent of it? */
            if (a == n) {
                result = FS_BUSY;
                break;
            }
            if (a == FS_ROOT) {
                break;
            }
        }
        if (result == FS_OK && nodes[n].type == FS_DIRECTORY && nodes[n].first_child >= 0) {
            result = FS_NOT_EMPTY;
        }
    }
    if (result == FS_OK) {
        int *link = &nodes[nodes[n].parent].first_child;
        while (*link != n) {
            link = &nodes[*link].next_sibling;
        }
        *link = nodes[n].next_sibling;
        nodes[nodes[n].parent].modified_tick = timer_ticks();
        kfree(nodes[n].data);
        memset(&nodes[n], 0, sizeof(nodes[n]));
    }
    UNLOCK();
    return result;
}

int fs_info(int node, struct fs_info *info)
{
    LOCK();
    if (!valid(node)) {
        UNLOCK();
        return FS_NOT_FOUND;
    }
    info->type = nodes[node].type;
    strlcpy(info->name, nodes[node].name, FS_NAME_MAX);
    info->modified_tick = nodes[node].modified_tick;
    info->size = nodes[node].size;
    if (nodes[node].type == FS_DIRECTORY) {
        info->size = 0;
        for (int c = nodes[node].first_child; c >= 0; c = nodes[c].next_sibling) {
            info->size++;
        }
    }
    UNLOCK();
    return FS_OK;
}

int fs_first_child(int directory)
{
    return valid(directory) && nodes[directory].type == FS_DIRECTORY
               ? nodes[directory].first_child : -1;
}

int fs_next_sibling(int node)
{
    return valid(node) ? nodes[node].next_sibling : -1;
}

void fs_path(int node, char *buffer, size_t size)
{
    LOCK();
    /* Collect the chain of parents, then print it root first. */
    int chain[FS_MAX_NODES];
    int depth = 0;
    for (int n = node; valid(n) && n != FS_ROOT && depth < FS_MAX_NODES; n = nodes[n].parent) {
        chain[depth++] = n;
    }
    size_t used = 0;
    buffer[0] = '\0';
    if (depth == 0) {
        strlcpy(buffer, "/", size);
        used = strlen(buffer);
    }
    while (depth-- > 0 && used + 1 < size) {
        buffer[used++] = '/';
        used += strlcpy(buffer + used, nodes[chain[depth]].name, size - used);
        if (used >= size) {
            used = size - 1;
        }
    }
    buffer[used < size ? used : size - 1] = '\0';
    UNLOCK();
}

void fs_usage(int *nodes_used, size_t *bytes_stored)
{
    LOCK();
    *nodes_used = 0;
    *bytes_stored = 0;
    for (int n = 0; n < FS_MAX_NODES; n++) {
        if (nodes[n].type != FS_FREE) {
            (*nodes_used)++;
            *bytes_stored += nodes[n].size;
        }
    }
    UNLOCK();
}

const char *fs_error_string(int error)
{
    switch (error) {
    case FS_OK:            return "ok";
    case FS_NOT_FOUND:     return "no such file or directory";
    case FS_EXISTS:        return "already exists";
    case FS_NOT_DIRECTORY: return "not a directory";
    case FS_IS_DIRECTORY:  return "is a directory";
    case FS_NO_SPACE:      return "file system full";
    case FS_INVALID_NAME:  return "invalid name";
    case FS_NOT_EMPTY:     return "directory not empty";
    case FS_BUSY:          return "is the current directory or one of its parents";
    case FS_TOO_LARGE:     return "file too large (limit 64 KiB)";
    }
    return "unknown error";
}
