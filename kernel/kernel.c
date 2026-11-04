/*
 * kernel/kernel.c - KERNELLED's C entry point.
 *
 * boot/boot.S jumps here after setting up the stack and zeroing .bss.
 */
#include <stdint.h>

#include "arch/aarch64/cpu.h"
#include "drivers/gic.h"
#include "drivers/timer.h"
#include "drivers/uart.h"
#include "interrupts/exceptions.h"
#include "kernel/printk.h"
#include "kernel/selftest.h"
#include "kernel/version.h"

/* Symbols defined by linker.ld. Only their addresses are meaningful. */
extern char __kernel_start[];
extern char __kernel_end[];

/* System tick rate: 100 interrupts per second, one every 10 ms. */
#define TICK_HZ 100

void kernel_main(void);

void kernel_main(void)
{
    uart_init();

    kprintf("\n");
    kprintf("KERNELLED OPERATING SYSTEM\n");
    kprintf("Kernel successfully initialised.\n");
    kprintf("Welcome to KERNELLED.\n");
    kprintf("\n");

    /* Boot report: every value below is read from the hardware or linker. */
    kprintf("[boot] version         %s\n", KERNELLED_VERSION);
    kprintf("[boot] exception level EL%u\n", cpu_current_el());
    kprintf("[boot] boot core       %u\n", cpu_core_id());
    kprintf("[boot] cpu (MIDR_EL1)  0x%016lx\n", cpu_midr());
    kprintf("[boot] kernel image    %p - %p (%lu bytes)\n", (void *)__kernel_start,
            (void *)__kernel_end, (uint64_t)(__kernel_end - __kernel_start));

    /* Phase 2a: exceptions. */
    exceptions_init();
    selftest_exceptions();
    selftest_trigger_fault();       /* only crashes in a FAULT=... build */

    /* Phase 2b: interrupt controller, then a 100 Hz tick, then unmask IRQs. */
    if (gic_init()) {
        timer_init(TICK_HZ);
        cpu_irq_enable();
    }
    selftest_timer();

    kprintf("[boot] Phase 2b complete. Printing uptime every second. "
            "Quit QEMU with Ctrl-A, then X.\n");

    /*
     * Idle loop. WFI sleeps until an interrupt arrives; the timer IRQ runs,
     * returns here, and we print once each time a full second has passed.
     */
    uint64_t last_second = 0;
    for (;;) {
        cpu_wait_for_interrupt();
        uint64_t ticks = timer_ticks();
        uint64_t second = ticks / TICK_HZ;
        if (second != last_second) {
            last_second = second;
            kprintf("[timer] uptime %lu s (%lu ticks)\n", second, ticks);
        }
    }
}
