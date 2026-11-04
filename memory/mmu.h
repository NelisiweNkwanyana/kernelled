/*
 * memory/mmu.h - page tables and the Memory Management Unit.
 *
 * KERNELLED uses an identity map: every virtual address equals its physical
 * address. The MMU is still essential, because each mapping carries
 * attributes:
 *   - memory type: devices must be "Device" memory (no caching, no merging),
 *     RAM is "Normal" cacheable memory (much faster);
 *   - permissions: kernel code is read-only + executable, constants are
 *     read-only, data is read-write but never executable (W^X);
 *   - unmapped addresses (including 0, the NULL pointer) fault immediately.
 */
#ifndef KERNELLED_MEMORY_MMU_H
#define KERNELLED_MEMORY_MMU_H

#include <stdint.h>

/* Build the page tables for RAM [ram_base, ram_base + ram_size) and the
 * devices, then switch the MMU and caches on. */
void mmu_init(uint64_t ram_base, uint64_t ram_size);

/* 1 if the MMU is on (SCTLR_EL1.M). */
int mmu_enabled(void);

#endif /* KERNELLED_MEMORY_MMU_H */
