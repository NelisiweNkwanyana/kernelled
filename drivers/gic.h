/*
 * drivers/gic.h - ARM Generic Interrupt Controller, version 2 (GICv2).
 *
 * The GIC collects interrupt signals from every device and forwards the most
 * important pending one to the CPU as an IRQ. It has two parts:
 *   - the distributor (GICD): global settings, which interrupts are enabled,
 *     their priorities and which core receives them;
 *   - the CPU interface (GICC): per-core; the kernel reads it to learn which
 *     interrupt fired (acknowledge) and writes it when done (end of interrupt).
 */
#ifndef KERNELLED_DRIVERS_GIC_H
#define KERNELLED_DRIVERS_GIC_H

#include <stdint.h>

/* INTIDs 1020-1023 are special; 1023 means "nothing pending" (spurious). */
#define GIC_INTID_SPURIOUS 1023u

/* Verify the GIC is a GICv2, disable everything, then switch it on.
 * Returns 1 on success, 0 if no GICv2 was found. */
int gic_init(void);

/* Enable one interrupt with the given priority (0 = highest, 255 = lowest). */
void gic_enable_interrupt(unsigned int intid, uint8_t priority);

/* Read GICC_IAR: returns the value to pass to gic_end_of_interrupt().
 * Its low 10 bits are the INTID of the interrupt now being handled. */
uint32_t gic_acknowledge(void);

/* Write GICC_EOIR: tell the GIC this interrupt has been handled. */
void gic_end_of_interrupt(uint32_t iar);

#endif /* KERNELLED_DRIVERS_GIC_H */
