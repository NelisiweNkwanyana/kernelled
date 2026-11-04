/*
 * kernel/selftest.h - boot-time checks that kernel subsystems really work.
 */
#ifndef KERNELLED_KERNEL_SELFTEST_H
#define KERNELLED_KERNEL_SELFTEST_H

/* Raise recoverable exceptions (SVC, BRK) and check the kernel survives. */
void selftest_exceptions(void);

/* In a FAULT=... build, deliberately crash. Does nothing in a normal build. */
void selftest_trigger_fault(void);

/* Phase 2b: count timer ticks over 500 ms of the hardware counter and
 * check the number matches the configured tick rate. */
void selftest_timer(void);

#endif /* KERNELLED_KERNEL_SELFTEST_H */
