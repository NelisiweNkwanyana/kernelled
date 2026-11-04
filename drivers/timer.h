/*
 * drivers/timer.h - ARM generic timer (EL1 physical timer) as the system tick.
 *
 * Every ARMv8 core has a 64-bit counter (CNTPCT_EL0) that counts up at a
 * fixed rate (CNTFRQ_EL0). The EL1 physical timer raises an interrupt when
 * the counter reaches a deadline written to CNTP_CVAL_EL0. Each interrupt is
 * one "tick"; the handler sets the next deadline one tick period later.
 */
#ifndef KERNELLED_DRIVERS_TIMER_H
#define KERNELLED_DRIVERS_TIMER_H

#include <stdint.h>

/* Start ticking `hz` times per second and enable the timer interrupt. */
void timer_init(unsigned int hz);

/* Called by the IRQ handler when the timer interrupt fires. */
void timer_handle_irq(void);

/* Number of ticks since timer_init(). Updated only by the interrupt. */
uint64_t timer_ticks(void);

/* Tick rate passed to timer_init(). */
unsigned int timer_tick_hz(void);

/* Raw hardware counter and its frequency in Hz. */
uint64_t timer_counter(void);
uint64_t timer_frequency(void);

#endif /* KERNELLED_DRIVERS_TIMER_H */
