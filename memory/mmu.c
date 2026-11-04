/*
 * memory/mmu.c - AArch64 stage 1 translation for EL1, 4 KiB granule.
 *
 * Configuration: 39-bit virtual addresses (T0SZ = 25), so translation starts
 * at level 1. Each table has 512 entries of 8 bytes (exactly one 4 KiB page):
 *
 *   level 1 entry covers 1 GiB   -> points to a level 2 table
 *   level 2 entry covers 2 MiB   -> a 2 MiB "block", or a level 3 table
 *   level 3 entry covers 4 KiB   -> one page
 *
 * Map built here:
 *   0x00000000 - 0x3FFFFFFF  L1[0] -> L2 "devices": only the GIC block
 *                            (0x08000000) and the UART block (0x09000000)
 *                            are mapped; everything else, including address
 *                            0, is left invalid so stray accesses fault.
 *   0x40000000 - RAM end     L1[1] -> L2 "RAM": the 2 MiB block holding the
 *                            kernel image uses an L3 table so each kernel
 *                            section gets its own permissions; the rest of
 *                            RAM (device tree, free memory) is 2 MiB
 *                            read-write, non-executable blocks.
 *
 * Reference: Arm ARM (DDI 0487) chapter D8, "The AArch64 Virtual Memory
 * System Architecture".
 */
#include <stdint.h>

#include "arch/aarch64/qemu_virt.h"
#include "kernel/printk.h"
#include "memory/mmu.h"

/* Descriptor types (bits 1:0). */
#define DESC_BLOCK      0x1UL       /* level 1/2: maps a block */
#define DESC_TABLE      0x3UL       /* level 1/2: points to the next table */
#define DESC_PAGE       0x3UL       /* level 3: maps a page */

/* Lower attributes. */
#define ATTR_INDEX(n)   ((uint64_t)(n) << 2)    /* which MAIR_EL1 byte to use */
#define AP_RW_EL1       (0UL << 6)              /* read-write, kernel only */
#define AP_RO_EL1       (2UL << 6)              /* read-only, kernel only */
#define SH_INNER        (3UL << 8)              /* inner shareable */
#define AF              (1UL << 10)             /* access flag: we set it, so no AF faults */

/* Upper attributes. */
#define PXN             (1UL << 53)             /* not executable at EL1 */
#define UXN             (1UL << 54)             /* not executable at EL0 */

/* MAIR_EL1: attribute 0 = Device-nGnRnE, attribute 1 = Normal write-back. */
#define MAIR_DEVICE     0
#define MAIR_NORMAL     1
#define MAIR_VALUE      ((0x00UL << (8 * MAIR_DEVICE)) | (0xFFUL << (8 * MAIR_NORMAL)))

#define DEVICE_BLOCK    (DESC_BLOCK | ATTR_INDEX(MAIR_DEVICE) | AP_RW_EL1 | AF | PXN | UXN)
#define RAM_BLOCK       (DESC_BLOCK | ATTR_INDEX(MAIR_NORMAL) | AP_RW_EL1 | SH_INNER | AF | PXN | UXN)
#define PAGE_CODE       (DESC_PAGE | ATTR_INDEX(MAIR_NORMAL) | AP_RO_EL1 | SH_INNER | AF | UXN)
#define PAGE_RODATA     (DESC_PAGE | ATTR_INDEX(MAIR_NORMAL) | AP_RO_EL1 | SH_INNER | AF | PXN | UXN)
#define PAGE_DATA       (DESC_PAGE | ATTR_INDEX(MAIR_NORMAL) | AP_RW_EL1 | SH_INNER | AF | PXN | UXN)

/*
 * TCR_EL1:
 *   T0SZ = 25      39-bit address space for TTBR0
 *   IRGN0/ORGN0=1  page-table walks use write-back cacheable memory
 *   SH0 = 3        inner shareable
 *   TG0 = 0        4 KiB granule
 *   EPD1 = 1       no TTBR1 (upper half) translation at all
 *   IPS = 1        36-bit physical addresses (64 GiB)
 */
#define TCR_VALUE       ((25UL << 0) | (1UL << 8) | (1UL << 10) | (3UL << 12) | (0UL << 14) | \
                         (1UL << 23) | (1UL << 32))

#define SCTLR_M         (1UL << 0)      /* MMU enable */
#define SCTLR_A         (1UL << 1)      /* alignment checking: keep unaligned access a fault */
#define SCTLR_C         (1UL << 2)      /* data cache enable */
#define SCTLR_SA        (1UL << 3)      /* stack alignment check */
#define SCTLR_I         (1UL << 12)     /* instruction cache enable */

#define BLOCK_2M        (2UL * 1024 * 1024)
#define ENTRIES         512

static uint64_t l1_table[ENTRIES] __attribute__((aligned(4096)));
static uint64_t l2_devices[ENTRIES] __attribute__((aligned(4096)));
static uint64_t l2_ram[ENTRIES] __attribute__((aligned(4096)));
static uint64_t l3_kernel[ENTRIES] __attribute__((aligned(4096)));

extern char __text_start[], __text_end[];
extern char __rodata_start[], __rodata_end[];
extern char __kernel_end[];

void mmu_init(uint64_t ram_base, uint64_t ram_size)
{
    uint64_t ram_end = ram_base + ram_size;
    uint64_t text_start = (uint64_t)__text_start, text_end = (uint64_t)__text_end;
    uint64_t ro_start = (uint64_t)__rodata_start, ro_end = (uint64_t)__rodata_end;

    /* The kernel must sit inside one 2 MiB block, covered by l3_kernel. */
    uint64_t kernel_block = text_start & ~(BLOCK_2M - 1);
    uint64_t kernel_index = (kernel_block - ram_base) / BLOCK_2M;
    if (ram_base != VIRT_RAM_BASE || (uint64_t)__kernel_end > kernel_block + BLOCK_2M) {
        kprintf("[init] mmu: unexpected memory layout, MMU left off\n");
        return;
    }

    /* Level 1: two 1 GiB regions. */
    l1_table[0] = (uint64_t)l2_devices | DESC_TABLE;
    l1_table[1] = (uint64_t)l2_ram | DESC_TABLE;

    /* Devices: only the 2 MiB blocks that contain the GIC and the UART. */
    l2_devices[VIRT_GICD_BASE / BLOCK_2M] = (VIRT_GICD_BASE & ~(BLOCK_2M - 1)) | DEVICE_BLOCK;
    l2_devices[VIRT_UART0_BASE / BLOCK_2M] = (VIRT_UART0_BASE & ~(BLOCK_2M - 1)) | DEVICE_BLOCK;

    /* RAM: the kernel's 2 MiB block page by page, the rest in 2 MiB blocks. */
    for (uint64_t i = 0; i < ENTRIES && ram_base + i * BLOCK_2M < ram_end; i++) {
        l2_ram[i] = (ram_base + i * BLOCK_2M) | RAM_BLOCK;
    }
    l2_ram[kernel_index] = (uint64_t)l3_kernel | DESC_TABLE;
    for (uint64_t i = 0; i < ENTRIES; i++) {
        uint64_t page = kernel_block + i * 4096;
        if (page >= ram_end) {
            break;
        }
        if (page >= text_start && page < text_end) {
            l3_kernel[i] = page | PAGE_CODE;
        } else if (page >= ro_start && page < ro_end) {
            l3_kernel[i] = page | PAGE_RODATA;
        } else {
            l3_kernel[i] = page | PAGE_DATA;
        }
    }

    /* Program the translation registers, then turn the MMU on. */
    __asm__ volatile(
        "msr mair_el1, %0\n"
        "msr tcr_el1, %1\n"
        "msr ttbr0_el1, %2\n"
        "dsb ish\n"                 /* page-table writes are complete */
        "isb\n"
        "tlbi vmalle1\n"            /* discard any stale translations */
        "dsb ish\n"
        "isb\n"
        :: "r"(MAIR_VALUE), "r"(TCR_VALUE), "r"((uint64_t)l1_table) : "memory");

    uint64_t sctlr;
    __asm__ volatile("mrs %0, sctlr_el1" : "=r"(sctlr));
    sctlr |= SCTLR_M | SCTLR_A | SCTLR_C | SCTLR_SA | SCTLR_I;
    __asm__ volatile("msr sctlr_el1, %0\n\tisb" :: "r"(sctlr) : "memory");

    kprintf("[init] mmu: on (identity map, caches on)\n");
    kprintf("[init] mmu: code   %p - %p  r-x\n", (void *)text_start, (void *)text_end);
    kprintf("[init] mmu: rodata %p - %p  r--\n", (void *)ro_start, (void *)ro_end);
    kprintf("[init] mmu: rest of RAM rw- (never executable); devices: GIC and UART only\n");
}

int mmu_enabled(void)
{
    uint64_t sctlr;
    __asm__ volatile("mrs %0, sctlr_el1" : "=r"(sctlr));
    return (int)(sctlr & SCTLR_M);
}
