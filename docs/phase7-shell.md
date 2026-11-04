# Phase 7: Shell

## Keyboard input

Typing in the terminal sends bytes to the PL011 UART. In Phase 7 the UART raises an interrupt when bytes arrive (INTID 33, SPI 1 on QEMU virt):

1. `uart_handle_irq()` drains the UART's receive FIFO into a 256-byte ring buffer and wakes the waiting process.
2. `uart_getc()` takes a byte from the buffer. If it is empty, the calling process **blocks**: it uses no CPU until the interrupt wakes it.

Both the "FIFO filling up" and "receive timeout" interrupts are enabled. With FIFOs on, a single key press never reaches the fill level; only the timeout reports it.

## The shell process (`shell/shell.c`)

`init` starts the shell after the boot tests. The loop is: print the prompt (`kernelled:/current/dir$ `), read a line, split it into words (double quotes keep spaces together), and look the first word up in the command table.

Line editing: Backspace/Delete, Ctrl-U (erase line), Ctrl-C (cancel line). Arrow-key escape sequences are ignored instead of printing garbage.

## Commands

| Command | Does |
|---|---|
| `help` | list commands |
| `clear` | clear the screen (ANSI escape `ESC[2J ESC[H`) |
| `about` | version, CPU, memory, interrupt and scheduler configuration (read from the hardware) |
| `meminfo` | physical pages (total, reserved, in use, free), heap usage and fragmentation, file system usage |
| `ps` | every process: pid, state, CPU time, times scheduled, name; total context switches |
| `uptime` | time since boot from the timer tick |
| `ls [path]` | list a directory (or show one file) |
| `mkdir`, `touch`, `cat`, `rm` | directories and files |
| `echo text [> file / >> file]` | print, write or append |
| `cd [dir]`, `pwd` | change / show the current directory |
| `spawn [seconds]` | start a CPU-bound process, to watch preemption with `ps` |
| `kill <pid>` | stop a process (not idle, not the shell itself) |
| `sleep <seconds>` | pause the shell (the process sleeps; the CPU idles) |

## Example session

```
kernelled:/$ mkdir projects
kernelled:/$ cd projects
kernelled:/projects$ echo KERNELLED notes > notes.txt
kernelled:/projects$ cat notes.txt
KERNELLED notes
kernelled:/projects$ spawn 5
started process 6 (spin) for 5 s; try 'ps'
kernelled:/projects$ ps
  PID  STATE      CPU TIME   RUNS  NAME
    0  ready            0.77s    118  idle
    6  ready            0.01s      3  spin
    5  running          0.00s    121  shell
Context switches since boot: 260
```
