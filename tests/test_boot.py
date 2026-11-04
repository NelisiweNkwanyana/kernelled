#!/usr/bin/env python3
"""Boot KERNELLED in QEMU without a window and check the serial console output.

Exit status 0 means every expected line appeared before the timeout.
Usage: python3 tests/test_boot.py [--qemu PATH] [--kernel build/kernelled.elf]
"""
import argparse
import os
import selectors
import subprocess
import sys
import time

EXPECTED_LINES = [
    "KERNELLED OPERATING SYSTEM",
    "Kernel successfully initialised.",
    "Welcome to KERNELLED.",
    "[boot] exception level EL1",
    "[init] exception vectors installed at",
    "[test] exception round-trip (SVC + BRK): PASS",
    "[mem] device tree at 0x0000000040000000",
    "RAM 0x0000000040000000 - 0x0000000048000000 (128 MiB)",
    "[init] mmu: on",
    "[test] page allocator (alloc, zero, free, refuse oversize): PASS",
    "[test] kernel heap (64 blocks, data intact, fully coalesced): PASS",
    "[test] mmu enabled: PASS",
    "[init] GICv2 ready",
    "[init] timer: counter",
    "[init] scheduler: round-robin",
    "ticks in 500 ms (expected 50): PASS",
    "[init] ramfs:",
    "[test] round-robin: CPU ticks",
    "[test] process_sleep(60): away for",
    "[test] process exit and reaping (stacks returned to page allocator): PASS",
    "[test] file system: create, write, append, read, paths, listing: PASS",
    "[test] file system: error cases rejected: PASS",
    "[test] file system: remove frees nodes and file data: PASS",
    "[boot] KERNELLED 1.0.0 is up.",
    "kernelled:/$ ",
]


def boot_and_capture(qemu, kernel, timeout, until=EXPECTED_LINES, seen_at=None):
    """Run QEMU, return the console text once every string in `until` has appeared
    (or the timeout expires). If `seen_at` is a dict, it receives the host time
    (time.monotonic()) at which each string in `until` first appeared."""
    command = [
        qemu, "-machine", "virt", "-cpu", "cortex-a72", "-smp", "1", "-m", "128M",
        "-nographic", "-kernel", kernel,
    ]
    process = subprocess.Popen(
        command, stdin=subprocess.DEVNULL, stdout=subprocess.PIPE, stderr=subprocess.STDOUT
    )
    selector = selectors.DefaultSelector()
    selector.register(process.stdout, selectors.EVENT_READ)
    output = b""
    deadline = time.monotonic() + timeout
    try:
        while time.monotonic() < deadline:
            if selector.select(timeout=0.2):
                chunk = os.read(process.stdout.fileno(), 4096)
                if not chunk:
                    break               # QEMU exited
                output += chunk
                text = output.decode(errors="replace")
                if seen_at is not None:
                    now = time.monotonic()
                    for line in until:
                        if line not in seen_at and line in text:
                            seen_at[line] = now
                if all(line in text for line in until):
                    break
    finally:
        process.kill()
        process.wait()
    return output.decode(errors="replace").replace("\r", "")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--qemu", default="qemu-system-aarch64")
    parser.add_argument("--kernel", default="build/kernelled.elf")
    parser.add_argument("--timeout", type=float, default=10.0)
    args = parser.parse_args()

    if not os.path.exists(args.kernel):
        print(f"FAIL: {args.kernel} not found. Run 'make' first.")
        return 1

    output = boot_and_capture(args.qemu, args.kernel, args.timeout)
    print("---- serial console ----")
    print(output.rstrip())
    print("------------------------")

    missing = [line for line in EXPECTED_LINES if line not in output]
    for line in EXPECTED_LINES:
        print(("  ok    " if line not in missing else "  MISSING ") + line.rstrip())
    failed_tests = [line for line in output.splitlines() if line.startswith("[test]") and "FAIL" in line]
    for line in failed_tests:
        print("  FAILED " + line)
    if missing or failed_tests:
        print(f"FAIL: {len(missing)} expected line(s) missing, {len(failed_tests)} self-test(s) failed")
        return 1
    print("PASS: KERNELLED booted, every self-test passed, the shell is waiting")
    return 0


if __name__ == "__main__":
    sys.exit(main())
