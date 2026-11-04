# Phase 8: Testing

`make test` runs three automated suites against a real QEMU. Each one fails loudly; none of them only checks that "something was printed".

## 1. Boot test (`tests/test_boot.py`)

Boots the kernel and requires every boot line and every in-kernel self-test:

| Self-test | Proves |
|---|---|
| exception round-trip | `SVC` and `BRK` are handled and execution resumes |
| page allocator | contiguous, zeroed pages; all returned; oversize requests refused |
| kernel heap | 64 allocations keep their data; freeing in any order coalesces back to one block |
| mmu enabled | `SCTLR_EL1.M` is set |
| timer interrupts | 50 ± 1 ticks in exactly 500 ms of hardware counter time |
| round-robin | three never-yielding workers each get an equal share of 60 ticks (± one slice) |
| process_sleep | a 60-tick sleep lasts 60 ticks (at most one slice late) |
| exit and reaping | exited processes' stacks go back to the page allocator |
| file system (3 tests) | operations, every error case, and complete cleanup |

Any `[test] ... FAIL` line fails the suite, and so does a missing shell prompt.

## 2. Fault test (`tests/test_exceptions.py`)

Builds five kernels that crash on purpose (`make FAULT=...`) and checks each crash report names the right exception class, cause, access type and address: `undefined`, `data-abort`, `alignment`, `null-pointer`, `write-rodata`.

## 3. Shell test (`tests/test_shell.py`)

Types 25 commands into the serial console like a person and checks the answers, including error messages. It also checks:

- **real time:** `uptime` advances 2.0 ± 0.5 s while the host waits 2 s;
- **preemption:** with `spawn` running a process that never yields, the shell still answers `ps` in under a second;
- **exit:** that process disappears after its time is up;
- **kill:** a killed process disappears from `ps`.

## Negative checks done during development

A test that cannot fail proves nothing. These were checked by breaking the kernel on purpose:

| Broken on purpose | Result |
|---|---|
| IRQs never unmasked | `[test] timer interrupts: 0 ticks ... FAIL`, boot test fails |
| Root path printed wrongly (a real bug) | boot test failed on the prompt `kernelled:$` |
| GCC splits the unaligned load (a real bug) | the alignment fault test failed until the load was written in assembly |

## Configurations tested

| Configuration | Result |
|---|---|
| GCC 15 (xPack `aarch64-none-elf`) + QEMU 9.1 | all suites pass |
| LLVM/clang + ld.lld + QEMU 9.1 | all suites pass |
| GCC 16 + QEMU 11 on Apple Silicon (your Mac) | Phases 1–2a verified; run `make test` to verify the rest |
| `-machine virtualization=on` (boot at EL2) | boot and shell suites pass |
| `-smp 4` (extra cores parked) | boot and shell suites pass |
