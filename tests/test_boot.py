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
    "[boot] Phase 1 complete.",
]


def boot_and_capture(qemu, kernel, timeout):
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
                if all(line in text for line in EXPECTED_LINES):
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
        print(("  ok    " if line not in missing else "  MISSING ") + line)
    if missing:
        print(f"FAIL: {len(missing)} expected line(s) missing")
        return 1
    print("PASS: KERNELLED booted and printed the expected messages")
    return 0


if __name__ == "__main__":
    sys.exit(main())
