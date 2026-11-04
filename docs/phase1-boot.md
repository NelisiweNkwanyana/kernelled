# Phase 1: How KERNELLED boots

## Target platform

| Choice | Value | Why |
|---|---|---|
| Architecture | AArch64 (ARMv8-A, 64-bit) | Native to the M4; same instruction set as the host |
| Emulated board | QEMU `virt` | Designed for virtual machines; stable, documented memory map; no board quirks |
| CPU model | `cortex-a72` | Well-documented ARMv8-A core; can switch to `-cpu host -accel hvf` later |
| Accelerator | TCG (software emulation) | Deterministic, full GDB support; HVF is optional later |
| Boot method | `-kernel kernelled.elf` | QEMU loads the ELF at its link address and jumps to `_start`; no firmware or bootloader |
| Console | PL011 UART at `0x09000000` | Connected to the terminal by `-nographic` |

## Memory map used in Phase 1

| Address | What |
|---|---|
| `0x09000000` | PL011 UART registers |
| `0x40000000` | Start of RAM (QEMU places the device tree here) |
| `0x40080000` | KERNELLED `.text` (entry point `_start`) |
| after `.text` | `.rodata`, `.data`, `.bss`, 16 KiB boot stack |

## Boot sequence

1. QEMU resets the CPU at EL1 with the MMU and caches off and jumps to `_start` (`boot/boot.S`).
2. `_start` reads `MPIDR_EL1` and parks every core except core 0 in a `wfe` loop.
3. It reads `CurrentEL`. At EL2 it configures `HCR_EL2` and uses `eret` to drop to EL1; at EL3 it parks.
4. It points `sp` at `__stack_top` (from `linker.ld`).
5. It zeroes `.bss` from `__bss_start` to `__bss_end`.
6. It calls `kernel_main()` (`kernel/kernel.c`).
7. `kernel_main()` initialises the PL011 UART, prints the banner and a boot report, then idles in `wfe`.

## Why the compiler flags matter

- `-ffreestanding`: there is no C library; only compiler headers like `stdint.h` are available.
- `-mgeneral-regs-only`: the FP/SIMD unit is not enabled yet, so the compiler must not use it.
- `-mstrict-align`: with the MMU off, all memory is treated as Device memory, where unaligned accesses fault.
- `-fno-pic`: the kernel runs at the fixed address it was linked for.
