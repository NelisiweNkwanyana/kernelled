# KERNELLED

KERNELLED is an educational operating system for 64-bit ARM (AArch64), written from scratch in C and assembly. It boots on QEMU's `virt` machine and is developed on an Apple Silicon Mac.

It has exception handling with crash reports, interrupts and a timer, memory management with an MMU, preemptive round-robin multitasking, an in-memory file system and an interactive shell.

## Status

| Phase | Area | Status |
|---|---|---|
| 1 | Foundation: toolchain, boot, serial console | Done |
| 2a | Exception vectors, crash reports, SVC/BRK handling | Done |
| 2b | Interrupt controller (GICv2) and 100 Hz timer | Done |
| 3 | Memory: device tree RAM discovery, page allocator, heap, MMU (W^X) | Done |
| 4 | Processes and context switching | Done |
| 5 | Preemptive round-robin scheduling | Done |
| 6 | In-memory hierarchical file system | Done |
| 7 | Interactive shell with keyboard interrupts | Done |
| 8 | Automated tests and documentation | Done |

## Requirements (macOS, Apple Silicon)

```sh
xcode-select --install                     # make, git, clang for the host
brew install qemu aarch64-elf-gcc aarch64-elf-binutils aarch64-elf-gdb python
make env                                   # verify everything is installed
```

## Build, run, test

```sh
make            # build build/kernelled.elf
make run        # boot to the shell; quit QEMU with Ctrl-A, then X
make test       # boot self-tests, 5 crash reports, scripted shell session
make run FAULT=null-pointer   # watch a deliberate crash
                              # (also: undefined, data-abort, alignment, write-rodata)
```

Other toolchains: `make TOOLCHAIN=llvm` (clang + ld.lld), or `make CROSS=aarch64-none-elf-` for a GCC with that prefix.

## What you see

```
[mem] device tree at 0x0000000040000000 (1048576 bytes): RAM 0x0000000040000000 - 0x0000000048000000 (128 MiB)
[init] mmu: on (identity map, caches on)
...
[test] round-robin: CPU ticks a=20 b=20 c=20 of 60: PASS
...
[boot] KERNELLED 1.0.0 is up.

Welcome to KERNELLED.
Type 'help' for a list of commands.
kernelled:/$ ls
  file         166 bytes  README
  dir            2 items  etc/
  dir            0 items  home/
  dir            0 items  tmp/
```

Shell commands: `help`, `clear`, `about`, `meminfo`, `ps`, `uptime`, `ls`, `mkdir`, `touch`, `cat`, `echo` (with `>` and `>>`), `rm`, `cd`, `pwd`, `spawn`, `kill`, `sleep`.

## Honest limits

- All processes run in kernel mode (EL1) and share one address space; there is no user mode or system-call interface yet.
- The file system lives in RAM; files are lost when QEMU exits.
- One CPU core is used; the others are parked at boot.
- Tested on QEMU `virt` only, not on real hardware.

## Debugging

```sh
make debug      # terminal 1: QEMU starts paused, GDB server on port 1234
make gdb        # terminal 2: GDB attaches and stops at kernel_main
make disasm     # disassembly of the kernel
```

## Layout

```
boot/         _start: core parking, EL2 -> EL1, stack, .bss
arch/aarch64/ CPU helpers, MMIO access, QEMU virt memory map, exception vector table
kernel/       kernel_main, kprintf, string functions, boot self-tests
interrupts/   trap frame, exception decoding, crash reports, IRQ routing
drivers/      PL011 UART (output and interrupt-driven input), GICv2, generic timer
memory/       device tree reader, page allocator, kernel heap, MMU page tables
process/      process table, creation/exit/sleep/block, context switch (assembly)
scheduler/    preemptive round-robin scheduler
filesystem/   in-memory hierarchical file system
shell/        interactive command line
tests/        boot, crash-report and shell test suites (Python, drive real QEMU)
scripts/      development utilities
docs/         design notes, one per phase
linker.ld     kernel memory layout (linked at 0x40200000)
Makefile      build automation
```

## Design notes

| Phase | Document |
|---|---|
| 1 | `docs/phase1-boot.md` |
| 2a | `docs/phase2a-exceptions.md` |
| 2b | `docs/phase2b-interrupts.md` |
| 3 | `docs/phase3-memory.md` |
| 4–5 | `docs/phase4-5-processes-scheduling.md` |
| 6 | `docs/phase6-filesystem.md` |
| 7 | `docs/phase7-shell.md` |
| 8 | `docs/phase8-testing.md` |
