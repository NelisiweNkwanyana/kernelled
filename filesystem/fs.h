/*
 * filesystem/fs.h - KERNELLED's in-memory hierarchical file system (ramfs).
 *
 * Files and directories live in RAM: a fixed table of nodes forms the tree,
 * and file contents are stored in kernel heap buffers that grow as needed.
 * Everything is lost when the machine is switched off (there is no disk
 * driver yet). Paths work like Unix: "/", "a/b", "..", ".".
 */
#ifndef KERNELLED_FILESYSTEM_FS_H
#define KERNELLED_FILESYSTEM_FS_H

#include <stddef.h>
#include <stdint.h>

#define FS_MAX_NODES      128
#define FS_NAME_MAX       28            /* bytes, including the final NUL */
#define FS_MAX_FILE_SIZE  (64 * 1024)
#define FS_PATH_MAX       256
#define FS_ROOT           0             /* node number of "/" */

enum fs_type { FS_FREE = 0, FS_FILE, FS_DIRECTORY };

/* Errors are negative numbers; node numbers are >= 0. */
enum fs_error {
    FS_OK            = 0,
    FS_NOT_FOUND     = -1,
    FS_EXISTS        = -2,
    FS_NOT_DIRECTORY = -3,
    FS_IS_DIRECTORY  = -4,
    FS_NO_SPACE      = -5,
    FS_INVALID_NAME  = -6,
    FS_NOT_EMPTY     = -7,
    FS_BUSY          = -8,
    FS_TOO_LARGE     = -9,
};

struct fs_info {
    enum fs_type type;
    char name[FS_NAME_MAX];
    size_t size;                /* bytes for a file, entries for a directory */
    uint64_t modified_tick;     /* timer tick of the last change */
};

/* Create the root directory and the initial files. */
void fs_init(void);

/* Path lookup relative to directory `cwd`. Returns a node number or error. */
int fs_lookup(const char *path, int cwd);

int fs_mkdir(const char *path, int cwd);
/* Create an empty file, or just update the time if it already exists. */
int fs_touch(const char *path, int cwd);
/* Replace (append = 0) or extend (append = 1) a file, creating it if needed. */
int fs_write(const char *path, int cwd, const char *data, size_t length, int append);
/* Copy up to `length` bytes from `offset` of a file. Returns bytes copied or error. */
int fs_read(int node, size_t offset, char *buffer, size_t length);
/* Remove a file or an empty directory; `cwd` and its parents cannot be removed. */
int fs_remove(const char *path, int cwd);

int fs_info(int node, struct fs_info *info);
/* Directory listing: first entry of `directory`, then the next one. -1 at the end. */
int fs_first_child(int directory);
int fs_next_sibling(int node);
/* Absolute path of a node, e.g. "/home/notes". */
void fs_path(int node, char *buffer, size_t size);

/* Usage: nodes in use and bytes of file data stored. */
void fs_usage(int *nodes_used, size_t *bytes_stored);

const char *fs_error_string(int error);

#endif /* KERNELLED_FILESYSTEM_FS_H */
