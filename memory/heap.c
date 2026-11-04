/*
 * memory/heap.c - first-fit allocator with block splitting and coalescing.
 *
 * The heap is one contiguous region divided into blocks laid end to end:
 *
 *   [header|payload......][header|payload..][header|payload.........]
 *
 * Each header records the payload size and whether the block is free.
 * kmalloc walks the blocks and takes the first free one that is big enough,
 * splitting off the remainder. kfree marks the block free and merges it with
 * free neighbours so the heap does not fragment into tiny pieces.
 */
#include <stddef.h>
#include <stdint.h>

#include "arch/aarch64/cpu.h"
#include "kernel/printk.h"
#include "kernel/string.h"
#include "memory/heap.h"
#include "memory/pmm.h"

#define BLOCK_MAGIC  0x4B4D454Du    /* "KMEM": detects corrupted headers and bad frees */
#define ALIGNMENT    16
#define MIN_SPLIT    32             /* don't create free blocks smaller than this */

struct block {
    uint32_t magic;
    uint32_t free;
    uint64_t size;                  /* payload bytes after this 16-byte header */
};

_Static_assert(sizeof(struct block) == 16, "header must keep payloads 16-byte aligned");

static uint8_t *heap_start;
static uint8_t *heap_end;

static struct block *next_block(struct block *block)
{
    uint8_t *next = (uint8_t *)block + sizeof(struct block) + block->size;
    return next < heap_end ? (struct block *)next : 0;
}

void heap_init(uint64_t pages)
{
    uint64_t address = pmm_alloc_pages(pages);
    if (address == 0) {
        kprintf("[init] heap: could not get %lu pages\n", pages);
        return;
    }
    heap_start = (uint8_t *)address;
    heap_end = heap_start + pages * PAGE_SIZE;

    struct block *first = (struct block *)heap_start;
    first->magic = BLOCK_MAGIC;
    first->free = 1;
    first->size = pages * PAGE_SIZE - sizeof(struct block);
}

static void *kmalloc_locked(size_t size)
{
    if (size == 0 || heap_start == 0) {
        return 0;
    }
    size = (size + ALIGNMENT - 1) & ~(size_t)(ALIGNMENT - 1);

    for (struct block *block = (struct block *)heap_start; block; block = next_block(block)) {
        if (block->magic != BLOCK_MAGIC) {
            kprintf("[mem] heap corrupted at %p\n", (void *)block);
            return 0;
        }
        if (!block->free || block->size < size) {
            continue;
        }
        if (block->size >= size + sizeof(struct block) + MIN_SPLIT) {
            struct block *rest = (struct block *)((uint8_t *)block + sizeof(struct block) + size);
            rest->magic = BLOCK_MAGIC;
            rest->free = 1;
            rest->size = block->size - size - sizeof(struct block);
            block->size = size;
        }
        block->free = 0;
        return (uint8_t *)block + sizeof(struct block);
    }
    return 0;
}

void *kzalloc(size_t size)
{
    void *pointer = kmalloc(size);
    if (pointer) {
        memset(pointer, 0, size);
    }
    return pointer;
}

static void kfree_locked(void *pointer)
{
    if (pointer == 0) {
        return;
    }
    struct block *block = (struct block *)((uint8_t *)pointer - sizeof(struct block));
    if ((uint8_t *)block < heap_start || (uint8_t *)block >= heap_end ||
        block->magic != BLOCK_MAGIC) {
        kprintf("[mem] kfree: %p was not allocated by kmalloc\n", pointer);
        return;
    }
    if (block->free) {
        kprintf("[mem] kfree: %p freed twice\n", pointer);
        return;
    }
    block->free = 1;

    /* Coalesce: merge every run of adjacent free blocks into one. */
    for (struct block *b = (struct block *)heap_start; b; b = next_block(b)) {
        while (b->free) {
            struct block *next = next_block(b);
            if (next == 0 || !next->free) {
                break;
            }
            b->size += sizeof(struct block) + next->size;
            next->magic = 0;
        }
    }
}

void heap_get_stats(struct heap_stats *stats)
{
    uint64_t flags = cpu_irq_save();
    memset(stats, 0, sizeof(*stats));
    stats->total_bytes = (uint64_t)(heap_end - heap_start);
    for (struct block *b = (struct block *)heap_start; b && heap_start; b = next_block(b)) {
        if (b->free) {
            stats->free_bytes += b->size;
            stats->free_blocks++;
        } else {
            stats->used_bytes += b->size;
            stats->allocations++;
        }
    }
    cpu_irq_restore(flags);
}

/* Public entry point: the allocator state must not change under us. */
void *kmalloc(size_t size)
{
    uint64_t flags = cpu_irq_save();
    void *result = kmalloc_locked(size);
    cpu_irq_restore(flags);
    return result;
}

/* Public entry point: the allocator state must not change under us. */
void kfree(void *pointer)
{
    uint64_t flags = cpu_irq_save();
    kfree_locked(pointer);
    cpu_irq_restore(flags);
}
