#!/usr/bin/env python3
"""Build each deliberate-fault kernel and check it ends in a correct crash report.

For every fault, the kernel must print a KERNEL PANIC naming the right exception
class and cause, then halt. A silent hang or a wrong diagnosis is a failure.
Usage: python3 tests/test_exceptions.py [--qemu PATH] [--make "make TOOLCHAIN=gnu"]
"""
import argparse
import shlex
import subprocess
import sys

from test_boot import boot_and_capture

# fault name -> strings the crash report must contain
CASES = {
    "undefined": [
        "KERNEL PANIC: unhandled exception",
        "class     : 0x00 unknown or undefined instruction",
        "System halted.",
    ],
    "data-abort": [
        "KERNEL PANIC: unhandled exception",
        "class     : 0x25 data abort (kernel)",
        "cause     : synchronous external abort",
        "access    : read",
        "address   : 0x000000007f000000",
        "System halted.",
    ],
    "alignment": [
        "KERNEL PANIC: unhandled exception",
        "class     : 0x25 data abort (kernel)",
        "cause     : alignment fault",
        "address   : 0x0000000040090001",
        "System halted.",
    ],
}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--qemu", default="qemu-system-aarch64")
    parser.add_argument("--make", default="make")
    parser.add_argument("--timeout", type=float, default=10.0)
    args = parser.parse_args()

    failures = 0
    for fault, expected in CASES.items():
        build = subprocess.run(shlex.split(args.make) + [f"FAULT={fault}"],
                               capture_output=True, text=True)
        if build.returncode != 0:
            print(f"FAIL  {fault}: build failed\n{build.stdout}{build.stderr}")
            failures += 1
            continue

        kernel = f"build/fault-{fault}/kernelled.elf"
        output = boot_and_capture(args.qemu, kernel, args.timeout, until=["System halted."])
        missing = [line for line in expected if line not in output]
        if missing:
            failures += 1
            print(f"FAIL  {fault}: crash report is missing:")
            for line in missing:
                print(f"        {line}")
            print("      console output was:\n" + output)
        else:
            print(f"PASS  {fault}: correct crash report, CPU halted")

    if failures:
        print(f"FAIL: {failures} of {len(CASES)} fault tests failed")
        return 1
    print(f"PASS: all {len(CASES)} deliberate faults produced correct crash reports")
    return 0


if __name__ == "__main__":
    sys.exit(main())
