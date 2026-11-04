/*
 * kernel/kernel.c - KERNELLED's C entry point.
 *
 * boot/boot.S jumps here after setting up the stack and zeroing .bss.
 */
#include <stdint.h>

#include "arch/aarch64/cpu.h"
#include "drivers/uart.h"
#include "interrupts/exceptions.h"
#include "kernel/printk.h"
#include "kernel/selftest.h"
#include "kernel/version.h"

/* Symbols defined by linker.ld. Only their addresses are meaningful. */
extern char __kernel_start[];
extern char __kernel_end[];

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

    kprintf("[boot] Phase 2a complete. CPU idle. Quit QEMU with Ctrl-A, then X.\n");
    for (;;) {
        cpu_wait_for_event();
    }
}
