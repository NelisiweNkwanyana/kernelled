# KERNELLED

KERNELLED is an educational operating system for 64-bit ARM (AArch64), written from scratch in C and assembly. It boots on QEMU's `virt` machine and is developed on an Apple Silicon Mac.

## Status

| Phase | Area | Status |
|---|---|---|
| 1 | Foundation: toolchain, boot, serial console | Done |
| 2a | Exception vectors, crash reports, SVC/BRK handling | Done |
| 2b | Interrupt controller (GIC) and timer | Not started |
| 3 | Memory management | Not started |
| 4 | Process management, context switching | Not started |
| 5 | Round-robin CPU scheduling | Not started |
| 6 | File system | Not started |
| 7 | Command-line shell | Not started |
| 8 | Testing and documentation | Ongoing |

Folders for later phases exist but are empty until their phase is built.

## Requirements (macOS, Apple Silicon)

```sh
xcode-select --install                     # make, git, clang for the host
brew install qemu aarch64-elf-gcc aarch64-elf-binutils aarch64-elf-gdb python
make env                                   # verify everything is installed
```

## Build and run

```sh
make            # build build/kernelled.elf
make run        # boot in QEMU; quit with Ctrl-A, then X
make test       # boot test + checks that every deliberate fault is reported
make run FAULT=data-abort   # watch a deliberate crash (also: undefined, alignment)
```

Expected console output:

```
KERNELLED OPERATING SYSTEM
Kernel successfully initialised.
Welcome to KERNELLED.

[boot] version         0.2.0 (Phase 2a: exceptions)
[boot] exception level EL1
[boot] boot core       0
...
```

## Debugging

```sh
make debug      # terminal 1: QEMU starts paused, GDB server on port 1234
make gdb        # terminal 2: GDB attaches and stops at kernel_main
```

## Layout

```
boot/         first instructions (_start), stack and .bss setup
kernel/       kernel_main, kprintf, boot self-tests
arch/aarch64/ CPU helpers, MMIO access, QEMU virt memory map, exception vector table
drivers/      device drivers (PL011 UART)
interrupts/   trap frame, exception decoding, kernel panic report
memory/ process/ scheduler/ filesystem/ shell/   later phases
tests/        automated tests (QEMU boot test)
scripts/      development utilities
docs/         design notes
linker.ld     kernel memory layout
Makefile      build automation
```

Design notes: `docs/phase1-boot.md` (boot process), `docs/phase2a-exceptions.md` (exceptions).
