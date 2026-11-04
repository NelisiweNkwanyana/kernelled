/*
 * scheduler/scheduler.h - preemptive round-robin CPU scheduling.
 *
 * Round-robin: READY processes take turns in table order. Each turn (time
 * slice) lasts SCHEDULER_SLICE_TICKS timer ticks; when it runs out, the timer
 * interrupt takes the CPU away and gives it to the next READY process. The
 * idle process (pid 0) runs only when nothing else is READY.
 */
#ifndef KERNELLED_SCHEDULER_SCHEDULER_H
#define KERNELLED_SCHEDULER_SCHEDULER_H

#include <stdint.h>

#define SCHEDULER_SLICE_TICKS 5         /* 5 ticks x 10 ms = 50 ms per turn */

/* Written at the lowest word of every process stack. If it has changed when
 * the scheduler looks, the process used more than its stack and overwrote
 * the memory below it, so the kernel halts instead of running on corrupted
 * memory. */
#define SCHEDULER_STACK_CANARY 0x5354414B43414E59UL   /* "STAKCANY" */

/* Start preemption (after process_init). */
void scheduler_start(void);

/* Called on every timer tick, inside the interrupt handler. */
void scheduler_tick(void);

/* Pick the next process and switch to it. IRQs must be masked. */
void schedule(void);

/* Set by scheduler_tick()/process_wake(); checked when an IRQ returns. */
extern volatile int scheduler_need_resched;

/* Number of context switches since boot. */
uint64_t scheduler_switch_count(void);

#endif /* KERNELLED_SCHEDULER_SCHEDULER_H */
