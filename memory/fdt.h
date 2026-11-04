/*
 * memory/fdt.h - read the RAM size from the flattened device tree (FDT).
 *
 * QEMU describes the virtual machine (RAM, devices) in a device tree blob.
 * For a bare-metal ELF kernel it places the blob at the start of RAM.
 */
#ifndef KERNELLED_MEMORY_FDT_H
#define KERNELLED_MEMORY_FDT_H

#include <stdint.h>

/* Find the first /memory node's "reg" property. Returns 1 and fills
 * base/size on success, 0 if there is no valid device tree at `blob`. */
int fdt_find_memory(uintptr_t blob, uint64_t *base, uint64_t *size);

/* Total size of the blob in bytes (0 if `blob` is not a device tree). */
uint32_t fdt_total_size(uintptr_t blob);

#endif /* KERNELLED_MEMORY_FDT_H */
