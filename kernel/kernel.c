/*
 * kernel/kernel.c - KERNELLED's C entry point.
 *
 * boot/boot.S jumps here after setting up the stack and zeroing .bss.
 */
#include <stdint.h>

#include "arch/aarch64/cpu.h"
#include "drivers/uart.h"
#include "kernel/version.h"

/* Symbols defined by linker.ld. Only their addresses are meaningful. */
extern char __kernel_start[];
extern char __kernel_end[];

void kernel_main(void);

void kernel_main(void)
{
    uart_init();

    uart_puts("\n");
    uart_puts("KERNELLED OPERATING SYSTEM\n");
    uart_puts("Kernel successfully initialised.\n");
    uart_puts("Welcome to KERNELLED.\n");
    uart_puts("\n");

    /* Boot report: every value below is read from the hardware or linker. */
    uart_puts("[boot] version         " KERNELLED_VERSION "\n");
    uart_puts("[boot] exception level EL");
    uart_put_dec(cpu_current_el());
    uart_puts("\n[boot] boot core       ");
    uart_put_dec(cpu_core_id());
    uart_puts("\n[boot] cpu (MIDR_EL1)  ");
    uart_put_hex(cpu_midr());
    uart_puts("\n[boot] kernel image    ");
    uart_put_hex((uint64_t)(uintptr_t)__kernel_start);
    uart_puts(" - ");
    uart_put_hex((uint64_t)(uintptr_t)__kernel_end);
    uart_puts(" (");
    uart_put_dec((uint64_t)(__kernel_end - __kernel_start));
    uart_puts(" bytes)\n");
    uart_puts("[boot] Phase 1 complete. CPU idle. Quit QEMU with Ctrl-A, then X.\n");

    for (;;) {
        cpu_wait_for_event();
    }
}
