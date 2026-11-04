/*
 * kernel/selftest.c - boot-time checks that kernel subsystems really work.
 */
#include <stdint.h>

#include "drivers/timer.h"
#include "interrupts/exceptions.h"
#include "filesystem/fs.h"
#include "kernel/printk.h"
#include "kernel/selftest.h"
#include "kernel/string.h"
#include "memory/heap.h"
#include "memory/mmu.h"
#include "memory/pmm.h"
#include "process/process.h"
#include "scheduler/scheduler.h"

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

static void report(const char *name, int ok)
{
    kprintf("[test] %s: %s\n", name, ok ? "PASS" : "FAIL");
}

void selftest_memory(void)
{
    /* Page allocator: contiguous, zeroed, distinct, and fully returned. */
    uint64_t free_before = pmm_free_page_count();
    uint64_t a = pmm_alloc_pages(1);
    uint64_t b = pmm_alloc_pages(3);
    int ok = a != 0 && b != 0 && a != b && a % PAGE_SIZE == 0 && b % PAGE_SIZE == 0 &&
             pmm_free_page_count() == free_before - 4;
    const uint8_t *bytes = (const uint8_t *)b;
    for (uint64_t i = 0; ok && i < 3 * PAGE_SIZE; i++) {
        ok = bytes[i] == 0;
    }
    pmm_free_pages(a, 1);
    pmm_free_pages(b, 3);
    ok = ok && pmm_free_page_count() == free_before;
    ok = ok && pmm_alloc_pages(pmm_total_pages() + 1) == 0;     /* impossible request */
    report("page allocator (alloc, zero, free, refuse oversize)", ok);

    /* Heap: many sizes, patterns survive, everything coalesces back. */
    struct heap_stats before, after;
    heap_get_stats(&before);
    enum { COUNT = 64 };
    uint8_t *blocks[COUNT];
    ok = 1;
    for (int i = 0; i < COUNT; i++) {
        size_t size = 8 + (size_t)i * 37;
        blocks[i] = kmalloc(size);
        if (blocks[i] == 0 || ((uintptr_t)blocks[i] & 15) != 0) {
            ok = 0;
            break;
        }
        memset(blocks[i], i, size);
    }
    for (int i = 0; ok && i < COUNT; i++) {
        size_t size = 8 + (size_t)i * 37;
        for (size_t j = 0; j < size; j++) {
            if (blocks[i][j] != (uint8_t)i) {
                ok = 0;
                break;
            }
        }
    }
    for (int i = 1; i < COUNT; i += 2) {     /* free odd ones first: holes */
        kfree(blocks[i]);
    }
    for (int i = 0; i < COUNT; i += 2) {
        kfree(blocks[i]);
    }
    heap_get_stats(&after);
    ok = ok && after.allocations == before.allocations && after.free_blocks == before.free_blocks &&
         after.free_bytes == before.free_bytes;
    report("kernel heap (64 blocks, data intact, fully coalesced)", ok);

    report("mmu enabled", mmu_enabled());
}

static volatile int workers_stop;
static volatile uint64_t worker_loops[3];

static void worker_main(void *argument)
{
    volatile uint64_t *loops = argument;
    while (!workers_stop) {
        (*loops)++;                     /* pure CPU work: never sleeps or yields */
    }
}

void selftest_scheduler(void)
{
    enum { WORKERS = 3, WINDOW_TICKS = 60 };
    static const char *const names[WORKERS] = { "worker-a", "worker-b", "worker-c" };
    uint64_t free_pages_before = pmm_free_page_count();
    int pids[WORKERS];

    workers_stop = 0;
    for (int i = 0; i < WORKERS; i++) {
        worker_loops[i] = 0;
        pids[i] = process_create(names[i], worker_main, (void *)&worker_loops[i]);
    }

    /* Sleep while the workers compete. Measure how long we were away. */
    uint64_t sleep_start = timer_ticks();
    process_sleep(WINDOW_TICKS);
    uint64_t slept = timer_ticks() - sleep_start;

    uint64_t ticks[WORKERS];
    uint64_t total = 0;
    for (int i = 0; i < WORKERS; i++) {
        struct process *p = process_find(pids[i]);
        ticks[i] = p ? p->ticks_run : 0;
        total += ticks[i];
    }
    workers_stop = 1;

    /* Fair round-robin: every worker within one time slice of an equal share. */
    int fair = pids[0] > 0 && pids[1] > 0 && pids[2] > 0;
    for (int i = 0; i < WORKERS; i++) {
        uint64_t share = total / WORKERS;
        uint64_t diff = ticks[i] > share ? ticks[i] - share : share - ticks[i];
        fair = fair && ticks[i] > 0 && diff <= SCHEDULER_SLICE_TICKS + 1;
    }
    kprintf("[test] round-robin: CPU ticks a=%lu b=%lu c=%lu of %lu: %s\n",
            ticks[0], ticks[1], ticks[2], total, fair ? "PASS" : "FAIL");

    /* We wake on the tick, then wait at most one slice for the CPU. */
    int slept_ok = slept >= WINDOW_TICKS && slept <= WINDOW_TICKS + SCHEDULER_SLICE_TICKS + 1;
    kprintf("[test] process_sleep(%u): away for %lu ticks: %s\n",
            (unsigned int)WINDOW_TICKS, slept, slept_ok ? "PASS" : "FAIL");

    /* Workers see the flag, return, exit; idle frees their stacks. */
    int gone = 0;
    for (int attempt = 0; attempt < 50 && !gone; attempt++) {
        process_sleep(1);
        gone = 1;
        for (int i = 0; i < WORKERS; i++) {
            gone = gone && process_find(pids[i]) == 0;
        }
    }
    int memory_back = pmm_free_page_count() == free_pages_before;
    kprintf("[test] process exit and reaping (stacks returned to page allocator): %s\n",
            gone && memory_back ? "PASS" : "FAIL");
}

void selftest_filesystem(void)
{
    int nodes_before;
    size_t bytes_before;
    struct heap_stats heap_before, heap_after;
    fs_usage(&nodes_before, &bytes_before);
    heap_get_stats(&heap_before);

    char buffer[64];
    int ok = 1;

    /* Create a small tree and write, append and read a file. */
    ok = ok && fs_mkdir("/tmp/fstest", FS_ROOT) >= 0;
    int dir = fs_lookup("/tmp/fstest", FS_ROOT);
    ok = ok && dir >= 0 && fs_mkdir("sub", dir) >= 0;
    ok = ok && fs_write("sub/../note.txt", dir, "hello", 5, 0) >= 0;      /* relative + .. */
    ok = ok && fs_write("/tmp/fstest/note.txt", FS_ROOT, ", world", 7, 1) >= 0;
    int file = fs_lookup("/tmp/./fstest//note.txt", FS_ROOT);
    int count = file >= 0 ? fs_read(file, 0, buffer, sizeof(buffer)) : -1;
    ok = ok && count == 12 && memcmp(buffer, "hello, world", 12) == 0;
    ok = ok && fs_read(file, 7, buffer, sizeof(buffer)) == 5 && memcmp(buffer, "world", 5) == 0;

    /* A larger file forces the data buffer to grow several times. */
    for (int i = 0; ok && i < 100; i++) {
        ok = fs_write("/tmp/fstest/big", FS_ROOT, "0123456789", 10, 1) >= 0;
    }
    struct fs_info info;
    int big = fs_lookup("/tmp/fstest/big", FS_ROOT);
    ok = ok && fs_info(big, &info) == FS_OK && info.size == 1000 && info.type == FS_FILE;
    ok = ok && fs_read(big, 995, buffer, sizeof(buffer)) == 5 && memcmp(buffer, "56789", 5) == 0;

    /* Directory listing order and fs_path. */
    int first = fs_first_child(dir);
    ok = ok && fs_info(first, &info) == FS_OK && strcmp(info.name, "sub") == 0;
    fs_path(file, buffer, sizeof(buffer));
    ok = ok && strcmp(buffer, "/tmp/fstest/note.txt") == 0;
    fs_path(FS_ROOT, buffer, sizeof(buffer));
    ok = ok && strcmp(buffer, "/") == 0;
    report("file system: create, write, append, read, paths, listing", ok);

    /* Every error case must be reported, not ignored. */
    int errors_ok = fs_mkdir("/tmp/fstest", FS_ROOT) == FS_EXISTS &&
                    fs_lookup("/nope/x", FS_ROOT) == FS_NOT_FOUND &&
                    fs_read(dir, 0, buffer, 1) == FS_IS_DIRECTORY &&
                    fs_mkdir("/README/x", FS_ROOT) == FS_NOT_DIRECTORY &&
                    fs_remove("/tmp/fstest", FS_ROOT) == FS_NOT_EMPTY &&
                    fs_remove("/tmp", dir) == FS_BUSY &&
                    fs_mkdir("/tmp/a-name-that-is-far-too-long-for-it", FS_ROOT) == FS_INVALID_NAME &&
                    fs_write("/tmp/fstest", FS_ROOT, "x", 1, 0) == FS_IS_DIRECTORY;
    report("file system: error cases rejected", errors_ok);

    /* Clean up: every node and every heap byte must come back. */
    int removed = fs_remove("/tmp/fstest/big", FS_ROOT) == FS_OK &&
                  fs_remove("/tmp/fstest/note.txt", FS_ROOT) == FS_OK &&
                  fs_remove("/tmp/fstest/sub", FS_ROOT) == FS_OK &&
                  fs_remove("/tmp/fstest", FS_ROOT) == FS_OK;
    int nodes_after;
    size_t bytes_after;
    fs_usage(&nodes_after, &bytes_after);
    heap_get_stats(&heap_after);
    report("file system: remove frees nodes and file data",
           removed && nodes_after == nodes_before && bytes_after == bytes_before &&
           heap_after.used_bytes == heap_before.used_bytes);
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
    /* RAM ends at 0x48000000 with -m 128M; the MMU maps nothing at 0x7f000000. */
    kprintf("[fault] reading from 0x7f000000, which is not mapped...\n");
    volatile uint64_t *nowhere = (volatile uint64_t *)0x7f000000UL;
    (void)*nowhere;

#elif defined(KERNELLED_FAULT_ALIGNMENT)
    /* SCTLR_EL1.A = 1 (set in memory/mmu.c) makes every unaligned access fault. */
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

#elif defined(KERNELLED_FAULT_NULL_POINTER)
    /* Address 0 is deliberately unmapped, so NULL dereferences are caught. */
    kprintf("[fault] reading through a NULL pointer...\n");
    volatile uint64_t *null_pointer = (volatile uint64_t *)0;
    (void)*null_pointer;

#elif defined(KERNELLED_FAULT_WRITE_RODATA)
    /* String literals live in .rodata, which the MMU maps read-only. */
    static const char message[] = "constants are read-only";
    kprintf("[fault] writing to read-only data at %p...\n", (const void *)message);
    *(volatile char *)message = 'X';
#endif
}
