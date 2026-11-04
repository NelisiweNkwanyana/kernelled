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
    "[init] GICv2 ready",
    "[init] timer: counter",
    "ticks in 500 ms (expected 50): PASS",
    "[boot] Phase 2b complete.",
]

# The two uptime lines must be about 2 seconds apart in real (host) time.
UPTIME_FIRST = "[timer] uptime 1 s"
UPTIME_LATER = "[timer] uptime 3 s"
UPTIME_GAP_SECONDS = 2.0
UPTIME_TOLERANCE = 0.5


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

    seen_at = {}
    output = boot_and_capture(args.qemu, args.kernel, args.timeout,
                              until=EXPECTED_LINES + [UPTIME_FIRST, UPTIME_LATER], seen_at=seen_at)
    print("---- serial console ----")
    print(output.rstrip())
    print("------------------------")

    missing = [line for line in EXPECTED_LINES if line not in output]
    for line in EXPECTED_LINES:
        print(("  ok    " if line not in missing else "  MISSING ") + line)
    if missing:
        print(f"FAIL: {len(missing)} expected line(s) missing")
        return 1

    if UPTIME_FIRST not in seen_at or UPTIME_LATER not in seen_at:
        print(f"FAIL: '{UPTIME_FIRST}' and '{UPTIME_LATER}' did not both appear "
              "(the timer interrupt is not reaching the idle loop)")
        return 1
    gap = seen_at[UPTIME_LATER] - seen_at[UPTIME_FIRST]
    in_range = abs(gap - UPTIME_GAP_SECONDS) <= UPTIME_TOLERANCE
    print(f"  {'ok   ' if in_range else 'FAIL '} uptime 1 s -> 3 s took {gap:.2f} s of real time "
          f"(expected {UPTIME_GAP_SECONDS:.1f} +/- {UPTIME_TOLERANCE})")
    if not in_range:
        print("FAIL: the kernel's clock does not match real time")
        return 1
    print("PASS: KERNELLED booted and printed the expected messages")
    return 0


if __name__ == "__main__":
    sys.exit(main())
