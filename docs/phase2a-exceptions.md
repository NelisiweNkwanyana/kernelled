# Phase 2a: Exception handling

## What an exception is on AArch64

An exception is any event that makes the CPU stop the current instruction stream and jump to the kernel. AArch64 has four kinds:

| Kind | Cause | Example |
|---|---|---|
| Synchronous | Caused by the instruction being executed | Undefined instruction, bad memory access, `SVC`, `BRK` |
| IRQ | Normal hardware interrupt | Timer tick, UART received a byte (Phase 2b) |
| FIQ | "Fast" interrupt, usually reserved for firmware | Not used by KERNELLED |
| SError | Asynchronous system error | Bus error reported late by the memory system |

## The vector table (`arch/aarch64/vectors.S`)

`VBAR_EL1` holds the address of a 2 KiB table of 16 slots, 128 bytes each. The CPU picks a slot by exception kind and origin:

| Origin | Sync | IRQ | FIQ | SError |
|---|---|---|---|---|
| Current EL, SP_EL0 | 0x000 | 0x080 | 0x100 | 0x180 |
| Current EL, SP_ELx (kernel) | 0x200 | 0x280 | 0x300 | 0x380 |
| Lower EL, AArch64 (user programs, later) | 0x400 | 0x480 | 0x500 | 0x580 |
| Lower EL, AArch32 | 0x600 | 0x680 | 0x700 | 0x780 |

Each slot saves `x0`/`x1`, loads its slot number and branches to `exception_common`, which:

1. saves every general-purpose register plus `ELR_EL1`, `SPSR_EL1`, `ESR_EL1`, `FAR_EL1` as a `struct trap_frame` on the stack,
2. calls `exception_dispatch(frame, slot)` in C,
3. restores everything (including any `ELR_EL1` change the handler made) and executes `eret`.

## Registers the CPU fills in on an exception

| Register | Meaning |
|---|---|
| `ELR_EL1` | Address to return to (for most faults: the faulting instruction) |
| `SPSR_EL1` | Processor state before the exception (flags, interrupt masks, EL) |
| `ESR_EL1` | Why: exception class `EC` in bits 31:26, details `ISS` in bits 24:0 |
| `FAR_EL1` | The memory address involved, for aborts |

## Recoverable vs fatal

| Exception | Handling |
|---|---|
| `SVC #imm` (system call) | `ELR_EL1` already points after the `SVC`: count it and return |
| `BRK #imm` (breakpoint) | `ELR_EL1` points at the `BRK`: add 4 to skip it, then return |
| Anything else | `kernel_panic()` prints a full report and halts with interrupts masked |

## Testing

- Every boot runs `selftest_exceptions()`: one `SVC` and one `BRK` must be handled and the kernel must continue.
- `make run FAULT=<name>` builds a kernel that crashes on purpose:

| `FAULT=` | What it does | Expected class / cause |
|---|---|---|
| `undefined` | Executes `UDF #0` | `0x00` unknown instruction |
| `data-abort` | Reads `0x7f000000`, which is not mapped | `0x25` data abort, translation fault level 2 (it was "synchronous external abort" before the MMU existed) |
| `alignment` | 8-byte `LDR` from `0x40090001` | `0x25` data abort, alignment fault |
| `null-pointer` | Reads address 0 (added in Phase 3) | `0x25` data abort, translation fault |
| `write-rodata` | Writes to a string constant (added in Phase 3) | `0x25` data abort, permission fault level 3, write |

- `make test` runs the boot test and then checks all three crash reports automatically.
