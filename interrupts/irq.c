/*
 * interrupts/irq.c - route each IRQ to the driver that raised it.
 *
 * Called from exception_dispatch() with IRQs masked (the CPU masks them on
 * exception entry), so handlers never interrupt each other.
 */
#include <stdint.h>

#include "arch/aarch64/qemu_virt.h"
#include "drivers/gic.h"
#include "drivers/timer.h"
#include "drivers/uart.h"
#include "interrupts/irq.h"
#include "kernel/printk.h"
#include "scheduler/scheduler.h"

volatile uint64_t irq_count;
volatile uint64_t irq_spurious_count;

void irq_handle(void)
{
    uint32_t iar = gic_acknowledge();
    unsigned int intid = iar & 0x3FF;

    if (intid >= 1020) {
        /* The interrupt went away before we read IAR. No EOI for spurious IDs. */
        irq_spurious_count++;
        return;
    }

    irq_count++;
    switch (intid) {
    case VIRT_IRQ_TIMER_EL1_PHYS:
        timer_handle_irq();
        scheduler_tick();
        break;
    case VIRT_IRQ_UART0:
        uart_handle_irq();
        break;
    default:
        kprintf("[irq] no handler for INTID %u, ignored\n", intid);
        break;
    }

    gic_end_of_interrupt(iar);
}
