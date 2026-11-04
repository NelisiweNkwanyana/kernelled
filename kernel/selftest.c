/*
 * kernel/selftest.c - boot-time checks that kernel subsystems really work.
 */
#include <stdint.h>

#include "drivers/timer.h"
#include "interrupts/exceptions.h"
#include "kernel/printk.h"
#include "kernel/selftest.h"

void selftest_exceptions(void)
{
    uint64_t svc_before = exceptions_svc_count;
    uint64_t brk_before = exceptions_brk_count;

    __asm__ volatile("svc #0x42" ::: "memory");
    __asm__ volatile("brk #0x7" ::: "memory");

    int ok = exceptions_svc_count == svc_before + 1 && exceptions_brk_count == brk_before + 1;
    kprintf("[test] exception round-trip (SVC + BRK): %s\n", ok ? "PASS" : "FAIL");
}

/*
 * Measure ticks against the hardware counter, not against a guess: busy-wait
 * until the counter has advanced by exactly half a second, then compare how
 * many ticks the interrupt handler counted meanwhile. The wait deliberately
 * does not use WFI, so a broken interrupt path reports "0 ticks: FAIL"
 * instead of sleeping forever.
 */
void selftest_timer(void)
{
    if (timer_tick_hz() == 0) {
        kprintf("[test] timer interrupts: timer not running: FAIL\n");
        return;
    }
    uint64_t frequency = timer_frequency();
    uint64_t expected = timer_tick_hz() / 2;
    uint64_t ticks_before = timer_ticks();
    uint64_t start = timer_counter();

    while (timer_counter() - start < frequency / 2) {
        /* spin: only the IRQ handler can change timer_ticks() */
    }

    uint64_t counted = timer_ticks() - ticks_before;
    /* Allow one tick either side: the window does not start on a tick edge. */
    int ok = expected > 0 && counted + 1 >= expected && counted <= expected + 1;
    kprintf("[test] timer interrupts: %lu ticks in 500 ms (expected %lu): %s\n",
            counted, expected, ok ? "PASS" : "FAIL");
}

/*
 * Deliberate crashes, selected at build time with `make run FAULT=<name>`.
 * Each one must end in a KERNEL PANIC report, never a silent hang.
 */
void selftest_trigger_fault(void)
{
#if defined(KERNELLED_FAULT_UNDEFINED)
    kprintf("[fault] executing an undefined instruction (UDF #0)...\n");
    __asm__ volatile(".word 0x00000000");

#elif defined(KERNELLED_FAULT_DATA_ABORT)
    /* RAM ends at 0x48000000 with -m 128M; nothing is mapped at 0x7f000000. */
    kprintf("[fault] reading from 0x7f000000, where no memory or device exists...\n");
    volatile uint64_t *nowhere = (volatile uint64_t *)0x7f000000UL;
    (void)*nowhere;

#elif defined(KERNELLED_FAULT_ALIGNMENT)
    /* With the MMU off all memory is Device memory, which forbids unaligned access. */
    kprintf("[fault] reading 8 bytes from the unaligned address 0x40090001...\n");
    /*
     * The load is written in assembly on purpose. In C, a compiler that can see
     * the address is misaligned obeys -mstrict-align by splitting the read into
     * eight 1-byte loads, so no fault happens (GCC does this; LLVM does not).
     * A single LDR of 8 bytes is what we want the CPU to reject.
     */
    uint64_t address = 0x40090001UL;
    uint64_t value;
    __asm__ volatile("ldr %0, [%1]" : "=r"(value) : "r"(address) : "memory");
    (void)value;
#endif
}
