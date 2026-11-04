/*
 * process/process.h - processes: independent flows of execution.
 *
 * Every KERNELLED process runs kernel code at EL1 on its own 16 KiB stack.
 * (There is no user mode yet: all processes share one address space, like
 * threads.) A process is described by a struct process in a fixed table.
 *
 * Life cycle:
 *   process_create() -> READY <-> RUNNING -> (exit) ZOMBIE -> (reaped) UNUSED
 *                                  |   ^
 *              process_sleep() /   |   | timer tick / process_wake()
 *              process_block() v   |
 *                          SLEEPING / BLOCKED
 */
#ifndef KERNELLED_PROCESS_PROCESS_H
#define KERNELLED_PROCESS_PROCESS_H

#include <stdint.h>

#define MAX_PROCESSES        16
#define PROCESS_NAME_LENGTH  16
#define PROCESS_STACK_PAGES  4          /* 16 KiB kernel stack per process */

enum process_state {
    PROCESS_UNUSED = 0,
    PROCESS_READY,          /* runnable, waiting for the CPU */
    PROCESS_RUNNING,        /* on the CPU right now */
    PROCESS_SLEEPING,       /* waiting for a tick count (process_sleep) */
    PROCESS_BLOCKED,        /* waiting for an event (process_block) */
    PROCESS_ZOMBIE,         /* exited; stack not yet freed */
};

/*
 * Registers saved by context_switch(). Only the callee-saved registers are
 * needed: context_switch is an ordinary function call, so the compiler has
 * already saved x0-x18 if it still needs them. Offsets are used by context.S.
 */
struct cpu_context {
    uint64_t x19, x20, x21, x22, x23, x24, x25, x26, x27, x28;
    uint64_t fp;            /* x29 */
    uint64_t lr;            /* x30: where context_switch "returns" to */
    uint64_t sp;
};

struct process {
    struct cpu_context context;
    int pid;
    enum process_state state;
    char name[PROCESS_NAME_LENGTH];
    uint64_t stack_base;            /* lowest address of the stack (0 = boot stack) */
    uint64_t wake_tick;             /* SLEEPING: wake when timer_ticks() >= this */
    uint64_t ticks_run;             /* timer ticks spent RUNNING (CPU time) */
    uint64_t start_tick;            /* timer_ticks() at creation */
    uint64_t times_scheduled;       /* how often the scheduler picked it */
    int slice_left;                 /* ticks left in the current time slice */
};

typedef void (*process_entry_t)(void *argument);

/* Turn the running boot code into process 0, "idle". Call once. */
void process_init(void);

/* Create a READY process that will run entry(argument). Returns its pid,
 * or -1 if the table is full or there is no memory for its stack. */
int process_create(const char *name, process_entry_t entry, void *argument);

/* End the calling process. Never returns. */
void process_exit(void) __attribute__((noreturn));

/* Give up the CPU for at least `ticks` timer ticks. */
void process_sleep(uint64_t ticks);

/* Give up the rest of the time slice voluntarily. */
void process_yield(void);

/* Wait until process_wake(). Must be called with IRQs masked, so the event
 * cannot happen between the caller's check and going to sleep. */
void process_block(void);

/* Make a BLOCKED process READY again (safe from interrupt handlers). */
void process_wake(struct process *process);

/* Stop another process. Returns 0, or -1 for pid 0, the caller, or no such pid. */
int process_kill(int pid);

/* Free the stacks of exited processes. Called by the idle process. */
void process_reap(void);

struct process *process_current(void);
struct process *process_find(int pid);          /* 0 if no such pid */
struct process *process_slot(int index);        /* table entry, may be UNUSED */
const char *process_state_name(enum process_state state);

/* Used by the scheduler. */
extern struct process *current_process;
void context_switch(struct cpu_context *from, struct cpu_context *to);

#endif /* KERNELLED_PROCESS_PROCESS_H */
