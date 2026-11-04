/*
 * memory/pmm.c - bitmap page frame allocator.
 */
#include <stdint.h>

#include "arch/aarch64/cpu.h"
#include "kernel/printk.h"
#include "kernel/string.h"
#include "memory/pmm.h"

#define MAX_PAGES (PMM_MAX_RAM / PAGE_SIZE)

static uint8_t bitmap[MAX_PAGES / 8];   /* bit set = page in use */
static uint64_t base;                   /* physical address of page 0 */
static uint64_t total_pages;
static uint64_t free_pages;
static uint64_t reserved_pages;
static uint64_t search_hint;            /* where the last allocation ended */

static int page_used(uint64_t page)
{
    return (bitmap[page / 8] >> (page % 8)) & 1;
}

static void set_used(uint64_t page, int used)
{
    if (used) {
        bitmap[page / 8] |= (uint8_t)(1u << (page % 8));
    } else {
        bitmap[page / 8] &= (uint8_t)~(1u << (page % 8));
    }
}

void pmm_init(uint64_t ram_base, uint64_t ram_size, uint64_t first_free)
{
    if (ram_size > PMM_MAX_RAM) {
        kprintf("[mem] only the first %lu MiB of RAM will be used\n", PMM_MAX_RAM >> 20);
        ram_size = PMM_MAX_RAM;
    }
    base = ram_base;
    total_pages = ram_size / PAGE_SIZE;

    uint64_t first_free_page = (first_free - ram_base + PAGE_SIZE - 1) / PAGE_SIZE;
    memset(bitmap, 0, sizeof(bitmap));
    for (uint64_t page = 0; page < first_free_page && page < total_pages; page++) {
        set_used(page, 1);
    }
    reserved_pages = first_free_page;
    free_pages = total_pages - reserved_pages;
    search_hint = first_free_page;
}

static uint64_t pmm_alloc_pages_locked(uint64_t count)
{
    if (count == 0 || count > free_pages) {
        return 0;
    }
    /* Next-fit: scan from the hint, wrap around once. */
    for (uint64_t pass = 0; pass < 2; pass++) {
        uint64_t start = pass == 0 ? search_hint : reserved_pages;
        uint64_t limit = pass == 0 ? total_pages : search_hint;
        uint64_t run = 0;
        for (uint64_t page = start; page < limit; page++) {
            run = page_used(page) ? 0 : run + 1;
            if (run == count) {
                uint64_t first = page + 1 - count;
                for (uint64_t i = first; i <= page; i++) {
                    set_used(i, 1);
                }
                free_pages -= count;
                search_hint = page + 1;
                uint64_t address = base + first * PAGE_SIZE;
                memset((void *)address, 0, count * PAGE_SIZE);
                return address;
            }
        }
    }
    return 0;
}

static void pmm_free_pages_locked(uint64_t address, uint64_t count)
{
    if (address < base || (address - base) % PAGE_SIZE != 0) {
        kprintf("[mem] pmm_free_pages: bad address %p\n", (void *)address);
        return;
    }
    uint64_t first = (address - base) / PAGE_SIZE;
    if (first < reserved_pages || first + count > total_pages) {
        kprintf("[mem] pmm_free_pages: %p is not allocatable memory\n", (void *)address);
        return;
    }
    for (uint64_t page = first; page < first + count; page++) {
        if (!page_used(page)) {
            kprintf("[mem] pmm_free_pages: page %p freed twice\n",
                    (void *)(base + page * PAGE_SIZE));
            continue;
        }
        set_used(page, 0);
        free_pages++;
    }
}

uint64_t pmm_total_pages(void)     { return total_pages; }
uint64_t pmm_free_page_count(void) { return free_pages; }
uint64_t pmm_reserved_pages(void)  { return reserved_pages; }
uint64_t pmm_ram_base(void)        { return base; }
uint64_t pmm_ram_size(void)        { return total_pages * PAGE_SIZE; }

/* Public entry point: the allocator state must not change under us. */
uint64_t pmm_alloc_pages(uint64_t count)
{
    uint64_t flags = cpu_irq_save();
    uint64_t result = pmm_alloc_pages_locked(count);
    cpu_irq_restore(flags);
    return result;
}

/* Public entry point: the allocator state must not change under us. */
void pmm_free_pages(uint64_t address, uint64_t count)
{
    uint64_t flags = cpu_irq_save();
    pmm_free_pages_locked(address, count);
    cpu_irq_restore(flags);
}
