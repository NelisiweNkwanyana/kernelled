/*
 * scheduler/scheduler.c - round-robin scheduler.
 */
#include <stdint.h>

#include "drivers/timer.h"
#include "interrupts/exceptions.h"
#include "kernel/printk.h"
#include "process/process.h"
#include "scheduler/scheduler.h"

volatile int scheduler_need_resched;
static int started;
static uint64_t switch_count;

void scheduler_start(void)
{
    started = 1;
    kprintf("[init] scheduler: round-robin, time slice %u ticks (%u ms)\n",
            (unsigned int)SCHEDULER_SLICE_TICKS,
            (unsigned int)(SCHEDULER_SLICE_TICKS * 1000 / timer_tick_hz()));
}

void scheduler_tick(void)
{
    if (!started) {
        return;
    }
    struct process *running = current_process;
    running->ticks_run++;

    /* Wake sleepers whose time has come. */
    uint64_t now = timer_ticks();
    int any_ready = 0;
    for (int i = 1; i < MAX_PROCESSES; i++) {
        struct process *p = process_slot(i);
        if (p->state == PROCESS_SLEEPING && now >= p->wake_tick) {
            p->state = PROCESS_READY;
        }
        if (p->state == PROCESS_READY) {
            any_ready = 1;
        }
    }

    if (running->pid == 0) {
        /* Idle should give way as soon as anything can run. */
        if (any_ready) {
            scheduler_need_resched = 1;
        }
    } else if (--running->slice_left <= 0) {
        scheduler_need_resched = 1;         /* time slice used up: preempt */
    }
}

/*
 * Choose the next READY process after the current one in table order, so
 * every process gets a turn before any process gets a second one.
 */
static struct process *pick_next(struct process *previous)
{
    int start = 0;
    for (int i = 0; i < MAX_PROCESSES; i++) {
        if (process_slot(i) == previous) {
            start = i;
        }
    }
    for (int offset = 1; offset <= MAX_PROCESSES; offset++) {
        struct process *candidate = process_slot((start + offset) % MAX_PROCESSES);
        if (candidate->pid != 0 && candidate->state == PROCESS_READY) {
            return candidate;
        }
    }
    if (previous->state == PROCESS_RUNNING) {
        return previous;                    /* nobody else wants the CPU */
    }
    return process_slot(0);                 /* idle */
}

static void check_stack(const struct process *p)
{
    if (p->stack_base != 0 && *(const uint64_t *)p->stack_base != SCHEDULER_STACK_CANARY) {
        kprintf("process %d (%s) overflowed its stack\n", p->pid, p->name);
        kernel_halt("kernel stack overflow");
    }
}

void schedule(void)
{
    scheduler_need_resched = 0;
    struct process *previous = current_process;
    check_stack(previous);

    struct process *next = pick_next(previous);
    if (previous->state == PROCESS_RUNNING) {
        previous->state = PROCESS_READY;
    }
    next->state = PROCESS_RUNNING;
    next->slice_left = SCHEDULER_SLICE_TICKS;
    next->times_scheduled++;
    if (next == previous) {
        return;
    }
    switch_count++;
    current_process = next;
    context_switch(&previous->context, &next->context);
    /* We are back: some later schedule() switched to this process again. */
}

uint64_t scheduler_switch_count(void)
{
    return switch_count;
}
