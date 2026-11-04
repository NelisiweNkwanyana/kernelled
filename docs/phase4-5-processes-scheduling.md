# Phases 4 and 5: Processes, context switching, round-robin scheduling

## What a process is in KERNELLED

A process is an independent flow of execution with its own stack and saved registers, described by `struct process` in a 16-entry table (`process/process.c`). Every process runs kernel code at EL1 in one shared address space, so they behave like threads. There is no user mode (EL0) yet; that would need per-process page tables and system calls.

| Field | Meaning |
|---|---|
| `pid`, `name`, `state` | Identity and life-cycle state |
| `context` | Registers saved by the last context switch |
| `stack_base` | Its 16 KiB stack (4 pages from the page allocator) |
| `ticks_run` | CPU time, in 10 ms ticks |
| `wake_tick` | When a sleeping process should wake |

States: `ready` → `running` → (`sleeping` / `blocked`) → `ready` ... → `zombie` (exited) → slot freed.

## Context switching (`process/context.S`)

`context_switch(from, to)` is an ordinary function call. It saves x19–x29, the return address (x30) and `sp` into `from`, loads the same registers from `to`, and executes `ret`. Because it loaded the other process's x30 and `sp`, `ret` continues *the other process*, at the point where it last called `context_switch`. Registers x0–x18 need no saving: the C calling convention already treats them as destroyed by any call.

A new process has never called `context_switch`, so `process_create` builds a fake saved context:
- `sp` is the top of its new stack;
- `x30` is `process_trampoline`;
- `x19`/`x20` hold its entry function and argument.

Its first switch-in "returns" into the trampoline, which calls `process_start()`. That enables interrupts, runs the entry function, and calls `process_exit()` if it returns.

## Preemptive round-robin (`scheduler/scheduler.c`)

- Every timer tick (10 ms) calls `scheduler_tick()` from the interrupt handler. It charges the tick to the running process, wakes sleepers whose time has come, and counts down the running process's time slice (5 ticks = 50 ms).
- When the slice is used up, it sets `scheduler_need_resched`. On the way out of the IRQ, `exception_dispatch` calls `schedule()`.
- `schedule()` picks the next `ready` process *after* the current one in table order (round-robin), so everyone gets a turn before anyone gets two. If nothing else is ready, the current process continues; if the current process cannot run, the idle process (pid 0) runs.

A switch inside an interrupt is safe: the interrupted process's trap frame stays on its own stack. When it is scheduled again, `schedule()` returns into the IRQ path, which restores the frame and `eret`s back into the exact instruction where it was interrupted.

## Process 0, idle

The boot code becomes process 0. It frees exited processes' stacks (`process_reap`) and waits in `wfi`. A process cannot free the stack it is running on, so exit and reaping are separate.

## Blocking and waking

`process_block()` (used by the keyboard driver in Phase 7) and `process_wake()` let a process sleep until an event, using no CPU time. The check-then-sleep is done with IRQs masked so a wake-up cannot be lost in between.

## Stack overflow detection

The lowest word of every stack holds a canary value. `schedule()` checks it on every switch; if it changed, the process overflowed its 16 KiB, and the kernel halts with "kernel stack overflow" instead of running on corrupted memory.

## Tests (inside the `init` process at boot)

```
[test] round-robin: CPU ticks a=20 b=20 c=20 of 60: PASS
[test] process_sleep(60): away for 60 ticks: PASS
[test] process exit and reaping (stacks returned to page allocator): PASS
```

Three workers spin forever without ever yielding. The only way they can share the CPU is preemption, and each gets an equal share (within one time slice). In the shell, `spawn` starts the same kind of process; `make test` checks that the shell still answers `ps` instantly while it runs.
