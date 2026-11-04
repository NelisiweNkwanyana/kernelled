/*
 * interrupts/irq.h - hardware interrupt (IRQ) handling.
 */
#ifndef KERNELLED_INTERRUPTS_IRQ_H
#define KERNELLED_INTERRUPTS_IRQ_H

#include <stdint.h>

/* Handle one IRQ: ask the GIC which device fired, run its handler, signal EOI. */
void irq_handle(void);

/* IRQs handled so far, and spurious ones (GIC had nothing pending). */
extern volatile uint64_t irq_count;
extern volatile uint64_t irq_spurious_count;

#endif /* KERNELLED_INTERRUPTS_IRQ_H */
