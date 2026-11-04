/*
 * memory/heap.h - kernel heap: variable-sized allocations (kmalloc/kfree).
 *
 * The page allocator only deals in whole 4 KiB pages. The heap takes a fixed
 * region of pages and carves it into blocks of any size.
 */
#ifndef KERNELLED_MEMORY_HEAP_H
#define KERNELLED_MEMORY_HEAP_H

#include <stddef.h>
#include <stdint.h>

/* Take `pages` pages from the page allocator for the heap. */
void heap_init(uint64_t pages);

/* Allocate `size` bytes, 16-byte aligned. Returns 0 if the heap is full. */
void *kmalloc(size_t size);

/* Like kmalloc, but the memory is zeroed. */
void *kzalloc(size_t size);

/* Free memory from kmalloc. kfree(0) does nothing. */
void kfree(void *pointer);

struct heap_stats {
    uint64_t total_bytes;       /* size of the heap region */
    uint64_t used_bytes;        /* bytes handed out (excluding headers) */
    uint64_t free_bytes;        /* bytes available in free blocks */
    uint64_t allocations;       /* blocks currently allocated */
    uint64_t free_blocks;       /* number of free blocks (fragmentation) */
};

void heap_get_stats(struct heap_stats *stats);

#endif /* KERNELLED_MEMORY_HEAP_H */
