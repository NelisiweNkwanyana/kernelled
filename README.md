# KERNELLED

KERNELLED is an educational operating system for 64-bit ARM (AArch64), written from scratch in C and assembly. It boots on QEMU's `virt` machine and is developed on an Apple Silicon Mac.

## Status

| Phase | Area | Status |
|---|---|---|
| 1 | Foundation: toolchain, boot, serial console | Done |
| 2 | Kernel initialisation, exceptions, interrupts, timer | Not started |
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
make test       # boot headlessly and check the console output
```

Expected console output:

```
KERNELLED OPERATING SYSTEM
Kernel successfully initialised.
Welcome to KERNELLED.

[boot] version         0.1.0 (Phase 1: boot)
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
kernel/       kernel_main and core kernel code
arch/aarch64/ CPU helpers, MMIO access, QEMU virt memory map
drivers/      device drivers (PL011 UART)
memory/ interrupts/ process/ scheduler/ filesystem/ shell/   later phases
tests/        automated tests (QEMU boot test)
scripts/      development utilities
docs/         design notes
linker.ld     kernel memory layout
Makefile      build automation
```

See `docs/phase1-boot.md` for how the boot process works.
