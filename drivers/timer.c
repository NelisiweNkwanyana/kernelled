/*
 * drivers/timer.c - system tick from the ARM generic timer.
 *
 * Registers (ARMv8 Architecture Reference Manual, chapter D11):
 *   CNTFRQ_EL0     counter frequency in Hz (set by firmware / QEMU)
 *   CNTPCT_EL0     current counter value
 *   CNTP_CVAL_EL0  deadline: the timer fires when CNTPCT >= CVAL
 *   CNTP_CTL_EL0   bit 0 ENABLE, bit 1 IMASK (1 = masked), bit 2 ISTATUS (fired)
 */
#include <stdint.h>

#include "arch/aarch64/qemu_virt.h"
#include "drivers/gic.h"
#include "drivers/timer.h"
#include "kernel/printk.h"

#define CNTP_CTL_ENABLE  (1u << 0)

static uint64_t frequency;          /* counter ticks per second */
static uint64_t interval;           /* counter ticks per system tick */
static uint64_t next_deadline;      /* counter value of the next tick */
static unsigned int tick_hz;
static volatile uint64_t ticks;     /* written only by timer_handle_irq() */

uint64_t timer_frequency(void)
{
    uint64_t value;
    __asm__ volatile("mrs %0, cntfrq_el0" : "=r"(value));
    return value;
}

uint64_t timer_counter(void)
{
    uint64_t value;
    /* isb: make sure the counter is read now, not earlier, by the CPU. */
    __asm__ volatile("isb; mrs %0, cntpct_el0" : "=r"(value) :: "memory");
    return value;
}

static void set_deadline(uint64_t deadline)
{
    __asm__ volatile("msr cntp_cval_el0, %0; isb" :: "r"(deadline) : "memory");
}

void timer_init(unsigned int hz)
{
    frequency = timer_frequency();
    if (frequency == 0 || hz == 0) {
        kprintf("[init] timer: CNTFRQ_EL0 is 0, cannot start the tick\n");
        return;
    }
    tick_hz = hz;
    interval = frequency / hz;

    next_deadline = timer_counter() + interval;
    set_deadline(next_deadline);
    __asm__ volatile("msr cntp_ctl_el0, %0; isb" :: "r"((uint64_t)CNTP_CTL_ENABLE) : "memory");

    gic_enable_interrupt(VIRT_IRQ_TIMER_EL1_PHYS, 0x80);

    kprintf("[init] timer: counter %lu Hz, tick %u Hz (every %lu counts), INTID %u\n",
            frequency, hz, interval, (unsigned int)VIRT_IRQ_TIMER_EL1_PHYS);
}

void timer_handle_irq(void)
{
    ticks++;

    /*
     * Schedule the next tick relative to the previous deadline, not to "now",
     * so the tick rate does not drift when an interrupt is handled late. If we
     * fell behind, the new deadline is already past and the timer fires again
     * at once, so the tick count catches up with real time.
     */
    next_deadline += interval;
    set_deadline(next_deadline);
}

uint64_t timer_ticks(void)
{
    return ticks;
}

unsigned int timer_tick_hz(void)
{
    return tick_hz;
}
