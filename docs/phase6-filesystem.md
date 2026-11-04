# Phase 6: File system

## Design

KERNELLED's file system (`filesystem/fs.c`) is an in-memory hierarchical file system (*ramfs*):

- a table of 128 **nodes**, each a file or a directory, with a name of up to 27 characters;
- the tree is stored as links: `parent`, `first_child` and `next_sibling`;
- **file contents** live in kernel-heap buffers that double in size when they fill up (64 KiB limit per file).

There is no disk driver yet, so **everything is lost when QEMU exits**. The interface (paths, create, read, write, remove) is the same one a disk-backed file system would provide.

```
/              first_child -> README -> etc -> home -> tmp
/etc           first_child -> motd -> version
```

## Paths

`walk()` resolves Unix-style paths: absolute (`/etc/motd`) or relative to the shell's current directory, with `.`, `..` and repeated slashes. To create something, it walks to the *parent* directory and returns the final name separately.

## Operations

| Function | Shell command |
|---|---|
| `fs_mkdir` | `mkdir` |
| `fs_touch` (create empty, or update time) | `touch` |
| `fs_write` (replace or append) | `echo text > file`, `echo text >> file` |
| `fs_read` | `cat` |
| `fs_remove` (file or empty directory) | `rm` |
| `fs_first_child` / `fs_next_sibling` / `fs_info` | `ls` |
| `fs_path` | `pwd`, the prompt |

Every error is a named code: not found, already exists, not a directory, is a directory, directory not empty, file system full, invalid name, file too large, busy (removing the current directory or one of its parents).

Every operation runs with IRQs masked, so a process switch can never observe the tree half-updated.

## Tests (at boot)

```
[test] file system: create, write, append, read, paths, listing: PASS
[test] file system: error cases rejected: PASS
[test] file system: remove frees nodes and file data: PASS
```

The last test checks that after removing everything it created, the node count, stored bytes and heap usage are exactly what they were before.
