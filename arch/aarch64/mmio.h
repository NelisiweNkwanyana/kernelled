/*
 * arch/aarch64/mmio.h - memory-mapped I/O register access.
 *
 * Device registers must be accessed through volatile pointers so the
 * compiler never caches, merges or removes the reads and writes.
 */
#ifndef KERNELLED_ARCH_MMIO_H
#define KERNELLED_ARCH_MMIO_H

#include <stdint.h>

static inline void mmio_write32(uintptr_t address, uint32_t value)
{
    *(volatile uint32_t *)address = value;
}

static inline uint32_t mmio_read32(uintptr_t address)
{
    return *(volatile uint32_t *)address;
}

#endif /* KERNELLED_ARCH_MMIO_H */
