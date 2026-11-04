/*
 * kernel/kernel.c - KERNELLED's C entry point.
 *
 * boot/boot.S jumps here after setting up the stack and zeroing .bss.
 */
#include <stdint.h>

#include "arch/aarch64/cpu.h"
#include "arch/aarch64/qemu_virt.h"
#include "drivers/gic.h"
#include "drivers/timer.h"
#include "drivers/uart.h"
#include "filesystem/fs.h"
#include "interrupts/exceptions.h"
#include "kernel/printk.h"
#include "kernel/selftest.h"
#include "kernel/version.h"
#include "memory/fdt.h"
#include "memory/heap.h"
#include "memory/mmu.h"
#include "memory/pmm.h"
#include "process/process.h"
#include "scheduler/scheduler.h"
#include "shell/shell.h"

/* Symbols defined by linker.ld. Only their addresses are meaningful. */
extern char __kernel_start[];
extern char __kernel_end[];

/* System tick rate: 100 interrupts per second, one every 10 ms. */
#define TICK_HZ 100

/* Kernel heap size: 256 pages = 1 MiB. */
#define HEAP_PAGES 256

void kernel_main(void);

/*
 * Process 1. Runs the tests that need several processes, then starts the
 * rest of the system.
 */
static void init_main(void *argument)
{
    (void)argument;
    selftest_scheduler();
    selftest_filesystem();
    kprintf("[boot] KERNELLED " KERNELLED_VERSION " is up.\n");

    if (process_create("shell", shell_main, 0) < 0) {
        kprintf("[boot] could not start the shell\n");
    }
    /* init's work is done: returning ends the process (process_exit). */
}

/*
 * Find out how much RAM there is (from the device tree), hand it to the page
 * allocator, turn the MMU on, and create the heap.
 */
static void memory_init(void)
{
    uint64_t ram_base = VIRT_RAM_BASE;
    uint64_t ram_size = 0;
    uint32_t dtb_size = fdt_total_size(VIRT_RAM_BASE);

    if (fdt_find_memory(VIRT_RAM_BASE, &ram_base, &ram_size)) {
        kprintf("[mem] device tree at %p (%u bytes): RAM %p - %p (%lu MiB)\n",
                (void *)VIRT_RAM_BASE, dtb_size, (void *)ram_base,
                (void *)(ram_base + ram_size), ram_size >> 20);
    } else {
        ram_size = 128UL * 1024 * 1024;
        kprintf("[mem] no device tree found; assuming 128 MiB (the -m value in the Makefile)\n");
    }

    pmm_init(ram_base, ram_size, (uint64_t)__kernel_end);
    mmu_init(ram_base, pmm_ram_size());
    heap_init(HEAP_PAGES);

    kprintf("[mem] pages: %lu total, %lu reserved (device tree + kernel), %lu free; heap %lu KiB\n",
            pmm_total_pages(), pmm_reserved_pages(), pmm_free_page_count(),
            (HEAP_PAGES * PAGE_SIZE) >> 10);
}

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

    /* Phase 3: memory. */
    memory_init();
    selftest_memory();
    selftest_trigger_fault();       /* only crashes in a FAULT=... build */

    /* Phase 2b: interrupt controller and a 100 Hz tick. */
    if (gic_init()) {
        timer_init(TICK_HZ);
    }

    /* Phases 4 and 5: this boot code becomes process 0 (idle). */
    process_init();
    scheduler_start();
    cpu_irq_enable();
    selftest_timer();

    /* Phase 6: file system. */
    fs_init();
    {
        int nodes;
        size_t bytes;
        fs_usage(&nodes, &bytes);
        kprintf("[init] ramfs: %d files and directories, %lu bytes\n", nodes, (uint64_t)bytes);
    }

    if (process_create("init", init_main, 0) < 0) {
        kernel_halt("could not create the init process");
    }

    /*
     * The idle process. It runs only when no other process is READY. It frees
     * the stacks of exited processes, then sleeps until the next interrupt.
     * The timer interrupt switches away from idle as soon as anything is READY.
     */
    for (;;) {
        process_reap();
        cpu_wait_for_interrupt();
    }
}
