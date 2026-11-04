/*
 * interrupts/exceptions.h - EL1 exception handling.
 */
#ifndef KERNELLED_INTERRUPTS_EXCEPTIONS_H
#define KERNELLED_INTERRUPTS_EXCEPTIONS_H

#include "interrupts/trap_frame.h"

/* Point VBAR_EL1 at the vector table. Call once, early in kernel_main. */
void exceptions_init(void);

/* Called from arch/aarch64/vectors.S for every exception. */
void exception_dispatch(struct trap_frame *frame, uint64_t slot);

/* Print a full crash report for `frame` and stop the CPU. Never returns. */
void kernel_panic(const char *reason, const struct trap_frame *frame, uint64_t slot)
    __attribute__((noreturn));

/* Stop the kernel with a message when there is no exception frame to show. */
void kernel_halt(const char *reason) __attribute__((noreturn));

/* Counters, used by the boot self-test to prove handlers ran. */
extern volatile uint64_t exceptions_svc_count;
extern volatile uint64_t exceptions_brk_count;

#endif /* KERNELLED_INTERRUPTS_EXCEPTIONS_H */
