/*
 * memory/pmm.h - physical memory manager: hands out RAM in 4 KiB pages.
 *
 * One bit per page records whether it is in use. Pages below the end of the
 * kernel image (the device tree and the kernel itself) are reserved at start.
 */
#ifndef KERNELLED_MEMORY_PMM_H
#define KERNELLED_MEMORY_PMM_H

#include <stdint.h>

#define PAGE_SIZE   4096UL
#define PAGE_SHIFT  12

/* Largest RAM the bitmap can describe (and the MMU maps): 1 GiB. */
#define PMM_MAX_RAM (1024UL * 1024 * 1024)

/* Manage RAM [ram_base, ram_base + ram_size); everything below
 * `first_free` is marked as permanently used. */
void pmm_init(uint64_t ram_base, uint64_t ram_size, uint64_t first_free);

/* Allocate `count` physically contiguous pages. Returns the address of the
 * first one, or 0 if no run of that length is free. Pages are zeroed. */
uint64_t pmm_alloc_pages(uint64_t count);

/* Return pages obtained from pmm_alloc_pages(). */
void pmm_free_pages(uint64_t address, uint64_t count);

uint64_t pmm_total_pages(void);
uint64_t pmm_free_page_count(void);
uint64_t pmm_reserved_pages(void);
uint64_t pmm_ram_base(void);
uint64_t pmm_ram_size(void);

#endif /* KERNELLED_MEMORY_PMM_H */
