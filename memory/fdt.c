/*
 * memory/fdt.c - minimal flattened device tree reader.
 *
 * Format (Devicetree Specification v0.4, chapter 5): a header, then a
 * "structure block" of 32-bit big-endian tokens:
 *   FDT_BEGIN_NODE <name, NUL-terminated, padded to 4 bytes>
 *   FDT_PROP <len> <nameoff> <value, padded to 4 bytes>
 *   FDT_END_NODE, FDT_NOP, FDT_END
 * Property names live in a separate "strings block", referenced by offset.
 * We only need: root #address-cells / #size-cells, and /memory@... reg.
 */
#include <stdint.h>

#include "kernel/string.h"
#include "memory/fdt.h"

#define FDT_MAGIC       0xd00dfeedu
#define FDT_BEGIN_NODE  1u
#define FDT_END_NODE    2u
#define FDT_PROP        3u
#define FDT_NOP         4u
#define FDT_END         9u

/* All FDT fields are big-endian; the CPU runs little-endian. Every token is
 * 4-byte aligned, so a 32-bit aligned load is safe even with the MMU off. */
static uint32_t be32(uintptr_t address)
{
    uint32_t v = *(volatile uint32_t *)address;
    return (v >> 24) | ((v >> 8) & 0xFF00u) | ((v << 8) & 0xFF0000u) | (v << 24);
}

static uint64_t read_cells(uintptr_t address, uint32_t cells)
{
    uint64_t value = 0;
    for (uint32_t i = 0; i < cells; i++) {
        value = (value << 32) | be32(address + 4 * i);
    }
    return value;
}

static uintptr_t align4(uintptr_t value)
{
    return (value + 3) & ~(uintptr_t)3;
}

uint32_t fdt_total_size(uintptr_t blob)
{
    if (be32(blob) != FDT_MAGIC) {
        return 0;
    }
    return be32(blob + 4);
}

int fdt_find_memory(uintptr_t blob, uint64_t *base, uint64_t *size)
{
    if (be32(blob) != FDT_MAGIC) {
        return 0;
    }
    uintptr_t structure = blob + be32(blob + 8);
    uintptr_t strings = blob + be32(blob + 12);
    uintptr_t end = blob + be32(blob + 4);

    /* Defaults from the specification, overridden by the root node. */
    uint32_t address_cells = 2, size_cells = 1;
    int depth = 0;
    int in_memory_node = 0;

    for (uintptr_t p = structure; p < end;) {
        uint32_t token = be32(p);
        p += 4;
        if (token == FDT_BEGIN_NODE) {
            const char *name = (const char *)p;
            depth++;
            /* Direct children of the root are at depth 2. */
            in_memory_node = depth == 2 && strncmp(name, "memory", 6) == 0;
            p = align4(p + strlen(name) + 1);
        } else if (token == FDT_END_NODE) {
            depth--;
            in_memory_node = 0;
        } else if (token == FDT_PROP) {
            uint32_t length = be32(p);
            const char *name = (const char *)(strings + be32(p + 4));
            uintptr_t value = p + 8;
            if (depth == 1 && strcmp(name, "#address-cells") == 0) {
                address_cells = be32(value);
            } else if (depth == 1 && strcmp(name, "#size-cells") == 0) {
                size_cells = be32(value);
            } else if (in_memory_node && strcmp(name, "reg") == 0 &&
                       length >= 4 * (address_cells + size_cells)) {
                *base = read_cells(value, address_cells);
                *size = read_cells(value + 4 * address_cells, size_cells);
                return 1;
            }
            p = align4(value + length);
        } else if (token == FDT_NOP) {
            continue;
        } else {
            break;                  /* FDT_END or corrupt data */
        }
    }
    return 0;
}
