/*
 * arch/aarch64/cpu.h - small helpers for reading CPU state.
 */
#ifndef KERNELLED_ARCH_CPU_H
#define KERNELLED_ARCH_CPU_H

#include <stdint.h>

/* Current exception level (0-3). CurrentEL keeps the level in bits [3:2]. */
static inline unsigned int cpu_current_el(void)
{
    uint64_t value;
    __asm__ volatile("mrs %0, CurrentEL" : "=r"(value));
    return (unsigned int)((value >> 2) & 0x3);
}

/* Core number within the cluster (MPIDR_EL1.Aff0). */
static inline unsigned int cpu_core_id(void)
{
    uint64_t value;
    __asm__ volatile("mrs %0, mpidr_el1" : "=r"(value));
    return (unsigned int)(value & 0xFF);
}

/* Main ID register: identifies the CPU implementer and part number. */
static inline uint64_t cpu_midr(void)
{
    uint64_t value;
    __asm__ volatile("mrs %0, midr_el1" : "=r"(value));
    return value;
}

/* Sleep until an event arrives. Used for the idle loop. */
static inline void cpu_wait_for_event(void)
{
    __asm__ volatile("wfe");
}

/* Sleep until an interrupt is pending. Used once interrupts are enabled. */
static inline void cpu_wait_for_interrupt(void)
{
    __asm__ volatile("wfi" ::: "memory");
}

/* Unmask / mask IRQs (the I bit of PSTATE.DAIF). */
static inline void cpu_irq_enable(void)
{
    __asm__ volatile("msr daifclr, #2" ::: "memory");
}

static inline void cpu_irq_disable(void)
{
    __asm__ volatile("msr daifset, #2" ::: "memory");
}

/*
 * Mask IRQs and return the previous mask state, for short critical sections:
 *     uint64_t flags = cpu_irq_save(); ... cpu_irq_restore(flags);
 * On a single core this is enough to stop an interrupt (or a preemptive
 * process switch) from running in the middle of the protected code.
 */
static inline uint64_t cpu_irq_save(void)
{
    uint64_t flags;
    __asm__ volatile("mrs %0, daif\n\tmsr daifset, #2" : "=r"(flags) :: "memory");
    return flags;
}

static inline void cpu_irq_restore(uint64_t flags)
{
    __asm__ volatile("msr daif, %0" :: "r"(flags) : "memory");
}

#endif /* KERNELLED_ARCH_CPU_H */
