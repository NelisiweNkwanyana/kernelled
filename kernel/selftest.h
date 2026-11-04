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

/* Phase 3: page allocator and heap allocate, free and coalesce correctly. */
void selftest_memory(void);

/* Phases 4-5: run inside a process. Three CPU-bound workers must share the
 * CPU equally, sleep must last the requested time, and exited processes'
 * stacks must be returned to the page allocator. */
void selftest_scheduler(void);

/* Phase 6: file system operations, error cases and memory cleanup. */
void selftest_filesystem(void);

#endif /* KERNELLED_KERNEL_SELFTEST_H */
