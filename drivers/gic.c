/*
 * drivers/gic.c - GICv2 driver for QEMU virt.
 *
 * Register offsets are from the ARM GICv2 Architecture Specification
 * (IHI 0048B), section 4.
 */
#include <stdint.h>

#include "arch/aarch64/mmio.h"
#include "arch/aarch64/qemu_virt.h"
#include "drivers/gic.h"
#include "kernel/printk.h"

/* Distributor registers (offsets from VIRT_GICD_BASE). */
#define GICD_CTLR        0x000   /* bit 0: forward interrupts to CPU interfaces */
#define GICD_TYPER       0x004   /* bits 4:0: (number of interrupt lines / 32) - 1 */
#define GICD_ISENABLER   0x100   /* write 1 to enable, one bit per INTID */
#define GICD_ICENABLER   0x180   /* write 1 to disable */
#define GICD_ICPENDR     0x280   /* write 1 to clear pending */
#define GICD_IPRIORITYR  0x400   /* one byte per INTID */
#define GICD_ITARGETSR   0x800   /* one byte per INTID: target core mask (SPIs only) */
#define GICD_PIDR2       0xFE8   /* bits 7:4: architecture revision */

/* CPU interface registers (offsets from VIRT_GICC_BASE). */
#define GICC_CTLR        0x000   /* bit 0: signal interrupts to this core */
#define GICC_PMR         0x004   /* priority mask: only priorities below this pass */
#define GICC_BPR         0x008   /* binary point: priority grouping */
#define GICC_IAR         0x00C   /* interrupt acknowledge */
#define GICC_EOIR        0x010   /* end of interrupt */

#define DEFAULT_PRIORITY 0xA0

static inline uint32_t gicd_read(uint32_t offset)
{
    return mmio_read32(VIRT_GICD_BASE + offset);
}

static inline void gicd_write(uint32_t offset, uint32_t value)
{
    mmio_write32(VIRT_GICD_BASE + offset, value);
}

static inline void gicc_write(uint32_t offset, uint32_t value)
{
    mmio_write32(VIRT_GICC_BASE + offset, value);
}

/*
 * Priority and target registers hold one byte per interrupt. We update them
 * with 32-bit read-modify-write so every access is aligned (-mstrict-align).
 */
static void gicd_write_byte(uint32_t base_offset, unsigned int intid, uint8_t value)
{
    uint32_t offset = base_offset + (intid & ~3u);
    unsigned int shift = (intid & 3u) * 8;
    uint32_t word = gicd_read(offset);
    word &= ~(0xFFu << shift);
    word |= (uint32_t)value << shift;
    gicd_write(offset, word);
}

static unsigned int gic_line_count;

int gic_init(void)
{
    /* Check we really are talking to a GICv2 before programming it. */
    unsigned int revision = (gicd_read(GICD_PIDR2) >> 4) & 0xF;
    if (revision != 2) {
        kprintf("[init] GIC: expected GICv2 at %p, found revision %u. Interrupts disabled.\n",
                (void *)VIRT_GICD_BASE, revision);
        return 0;
    }

    gicd_write(GICD_CTLR, 0);                       /* off while configuring */

    gic_line_count = 32 * ((gicd_read(GICD_TYPER) & 0x1F) + 1);
    if (gic_line_count > 1020) {
        gic_line_count = 1020;
    }

    /* Disable and clear every interrupt; enable only what drivers ask for. */
    for (unsigned int intid = 0; intid < gic_line_count; intid += 32) {
        gicd_write(GICD_ICENABLER + intid / 8, 0xFFFFFFFFu);
        gicd_write(GICD_ICPENDR + intid / 8, 0xFFFFFFFFu);
    }
    for (unsigned int intid = 0; intid < gic_line_count; intid++) {
        gicd_write_byte(GICD_IPRIORITYR, intid, DEFAULT_PRIORITY);
        if (intid >= 32) {
            gicd_write_byte(GICD_ITARGETSR, intid, 0x01);   /* SPIs -> core 0 */
        }
    }

    gicd_write(GICD_CTLR, 1);                       /* distributor on */

    gicc_write(GICC_PMR, 0xFF);                     /* let every priority through */
    gicc_write(GICC_BPR, 0);                        /* no priority grouping */
    gicc_write(GICC_CTLR, 1);                       /* CPU interface on */

    kprintf("[init] GICv2 ready: distributor %p, cpu interface %p, %u interrupt lines\n",
            (void *)VIRT_GICD_BASE, (void *)VIRT_GICC_BASE, gic_line_count);
    return 1;
}

void gic_enable_interrupt(unsigned int intid, uint8_t priority)
{
    if (intid >= gic_line_count) {
        kprintf("[gic] cannot enable INTID %u: only %u lines\n", intid, gic_line_count);
        return;
    }
    gicd_write_byte(GICD_IPRIORITYR, intid, priority);
    gicd_write(GICD_ISENABLER + (intid / 32) * 4, 1u << (intid % 32));
}

uint32_t gic_acknowledge(void)
{
    return mmio_read32(VIRT_GICC_BASE + GICC_IAR);
}

void gic_end_of_interrupt(uint32_t iar)
{
    gicc_write(GICC_EOIR, iar);
}
