/*
 * process/process.c - process table, creation, exit, sleeping and blocking.
 */
#include <stddef.h>
#include <stdint.h>

#include "arch/aarch64/cpu.h"
#include "drivers/timer.h"
#include "kernel/printk.h"
#include "kernel/string.h"
#include "memory/pmm.h"
#include "process/process.h"
#include "scheduler/scheduler.h"

_Static_assert(offsetof(struct cpu_context, fp) == 80, "context.S expects fp at 80");
_Static_assert(offsetof(struct cpu_context, sp) == 96, "context.S expects sp at 96");

static struct process table[MAX_PROCESSES];
static int next_pid = 1;
struct process *current_process;

extern char __stack_bottom[];
extern void process_trampoline(void);

void process_start(process_entry_t entry, void *argument);

void process_init(void)
{
    struct process *idle = &table[0];
    memset(table, 0, sizeof(table));
    idle->pid = 0;
    idle->state = PROCESS_RUNNING;
    strlcpy(idle->name, "idle", sizeof(idle->name));
    idle->stack_base = (uint64_t)__stack_bottom;   /* the boot stack from linker.ld */
    *(uint64_t *)idle->stack_base = SCHEDULER_STACK_CANARY;
    idle->slice_left = SCHEDULER_SLICE_TICKS;
    current_process = idle;
}

/* First code every new process runs (called from process_trampoline). */
void process_start(process_entry_t entry, void *argument)
{
    /* schedule() switched here with IRQs masked; a running process wants them. */
    cpu_irq_enable();
    entry(argument);
    process_exit();
}

int process_create(const char *name, process_entry_t entry, void *argument)
{
    uint64_t flags = cpu_irq_save();

    struct process *p = 0;
    for (int i = 1; i < MAX_PROCESSES; i++) {
        if (table[i].state == PROCESS_UNUSED) {
            p = &table[i];
            break;
        }
    }
    uint64_t stack = p ? pmm_alloc_pages(PROCESS_STACK_PAGES) : 0;
    if (p == 0 || stack == 0) {
        cpu_irq_restore(flags);
        return -1;
    }

    memset(p, 0, sizeof(*p));
    p->pid = next_pid++;
    strlcpy(p->name, name, sizeof(p->name));
    p->stack_base = stack;
    *(uint64_t *)stack = SCHEDULER_STACK_CANARY;
    p->start_tick = timer_ticks();

    /* Make the first context_switch to this process "return" into the
     * trampoline, on an empty stack, with entry/argument in x19/x20. */
    p->context.sp = stack + PROCESS_STACK_PAGES * PAGE_SIZE;
    p->context.lr = (uint64_t)process_trampoline;
    p->context.x19 = (uint64_t)entry;
    p->context.x20 = (uint64_t)argument;
    p->state = PROCESS_READY;

    int pid = p->pid;
    cpu_irq_restore(flags);
    return pid;
}

void process_exit(void)
{
    cpu_irq_save();
    current_process->state = PROCESS_ZOMBIE;
    schedule();                     /* never comes back: nobody picks a ZOMBIE */
    for (;;) {
    }
}

void process_sleep(uint64_t ticks)
{
    uint64_t flags = cpu_irq_save();
    current_process->wake_tick = timer_ticks() + ticks;
    current_process->state = PROCESS_SLEEPING;
    schedule();
    cpu_irq_restore(flags);
}

void process_yield(void)
{
    uint64_t flags = cpu_irq_save();
    schedule();
    cpu_irq_restore(flags);
}

void process_block(void)
{
    current_process->state = PROCESS_BLOCKED;
    schedule();
}

void process_wake(struct process *process)
{
    uint64_t flags = cpu_irq_save();
    if (process && process->state == PROCESS_BLOCKED) {
        process->state = PROCESS_READY;
        scheduler_need_resched = 1;
    }
    cpu_irq_restore(flags);
}

int process_kill(int pid)
{
    uint64_t flags = cpu_irq_save();
    struct process *p = process_find(pid);
    int result = -1;
    if (p && pid != 0 && p != current_process && p->state != PROCESS_ZOMBIE) {
        p->state = PROCESS_ZOMBIE;  /* never scheduled again; idle frees its stack */
        result = 0;
    }
    cpu_irq_restore(flags);
    return result;
}

void process_reap(void)
{
    uint64_t flags = cpu_irq_save();
    for (int i = 1; i < MAX_PROCESSES; i++) {
        struct process *p = &table[i];
        if (p->state == PROCESS_ZOMBIE && p != current_process) {
            pmm_free_pages(p->stack_base, PROCESS_STACK_PAGES);
            p->state = PROCESS_UNUSED;
        }
    }
    cpu_irq_restore(flags);
}

struct process *process_current(void)
{
    return current_process;
}

struct process *process_find(int pid)
{
    for (int i = 0; i < MAX_PROCESSES; i++) {
        if (table[i].state != PROCESS_UNUSED && table[i].pid == pid) {
            return &table[i];
        }
    }
    return 0;
}

struct process *process_slot(int index)
{
    return &table[index];
}

const char *process_state_name(enum process_state state)
{
    switch (state) {
    case PROCESS_UNUSED:   return "unused";
    case PROCESS_READY:    return "ready";
    case PROCESS_RUNNING:  return "running";
    case PROCESS_SLEEPING: return "sleeping";
    case PROCESS_BLOCKED:  return "blocked";
    case PROCESS_ZOMBIE:   return "zombie";
    }
    return "?";
}
