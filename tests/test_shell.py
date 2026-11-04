#!/usr/bin/env python3
"""Type commands into the KERNELLED shell over the serial console and check
the answers. Also checks that 'uptime' follows real time and that the shell
stays responsive while a CPU-bound process runs (preemption).

Usage: python3 tests/test_shell.py [--qemu PATH] [--kernel build/kernelled.elf]
"""
import argparse
import os
import re
import selectors
import subprocess
import sys
import time

PROMPT_RE = re.compile(r"kernelled:[^\n$]*\$ $")


class Console:
    """A running QEMU whose serial port we can type into and read from."""

    def __init__(self, qemu, kernel):
        self.process = subprocess.Popen(
            [qemu, "-machine", "virt", "-cpu", "cortex-a72", "-smp", "1", "-m", "128M",
             "-nographic", "-kernel", kernel],
            stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        self.selector = selectors.DefaultSelector()
        self.selector.register(self.process.stdout, selectors.EVENT_READ)
        self.text = ""

    def read_until_prompt(self, timeout):
        """Read until the output ends with a shell prompt; return what was read."""
        start = len(self.text)
        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            if self.selector.select(timeout=0.1):
                chunk = os.read(self.process.stdout.fileno(), 4096)
                if not chunk:
                    break
                self.text += chunk.decode(errors="replace").replace("\r", "")
                if PROMPT_RE.search(self.text[start:]):
                    return self.text[start:]
        raise TimeoutError("no shell prompt; output so far:\n" + self.text[start:])

    def run(self, command, timeout=5.0):
        """Type a command (slowly, like a person) and return its output."""
        for ch in command + "\r":
            self.process.stdin.write(ch.encode())
            self.process.stdin.flush()
            time.sleep(0.002)
        output = self.read_until_prompt(timeout)
        # Drop the echoed command line and the trailing prompt (but keep any
        # output that shares the prompt's line, like the 'clear' sequence).
        lines = output.split("\n")
        lines[-1] = PROMPT_RE.sub("", lines[-1])
        return "\n".join(lines[1:]).rstrip("\n")

    def close(self):
        self.process.kill()
        self.process.wait()


def uptime_seconds(text):
    match = re.search(r"\(([0-9]+)\.([0-9]+) s, ([0-9]+) ticks\)", text)
    return int(match.group(1)) + int(match.group(2)) / 100 if match else None


CHECKS = [
    # (command, strings that must appear in the output)
    ("help", ["help", "meminfo", "ps", "uptime", "ls [path]", "mkdir", "touch", "cat", "clear", "about"]),
    ("about", ["KERNELLED 1.0.0", "MMU on", "GICv2", "round-robin"]),
    ("meminfo", ["Physical memory", "131072 KiB  (32768 pages of 4 KiB)", "Kernel heap", "File system"]),
    ("ps", ["PID  STATE", "idle", "shell", "running"]),
    ("ls", ["README", "etc/", "home/", "tmp/"]),
    ("cat /etc/motd", ["Welcome to KERNELLED."]),
    ("mkdir /home/nelisiwe", []),
    ("cd /home/nelisiwe", []),
    ("pwd", ["/home/nelisiwe"]),
    ("touch empty.txt", []),
    ("echo Hello from KERNELLED > hello.txt", []),
    ("echo second line >> hello.txt", []),
    ("cat hello.txt", ["Hello from KERNELLED\nsecond line"]),
    ("ls", ["0 bytes  empty.txt", "33 bytes  hello.txt"]),
    ("cat missing.txt", ["cat: missing.txt: no such file or directory"]),
    ("mkdir /home/nelisiwe", ["already exists"]),
    ("cd ..", []),
    ("pwd", ["/home"]),
    ("rm nelisiwe", ["directory not empty"]),
    ("rm nelisiwe/empty.txt nelisiwe/hello.txt nelisiwe", []),
    ("ls", ["(empty)"]),
    ("cd", []),
    ("pwd", ["/"]),
    ("frobnicate", ["frobnicate: command not found"]),
    ("clear", ["\x1b[2J\x1b[H"]),
]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--qemu", default="qemu-system-aarch64")
    parser.add_argument("--kernel", default="build/kernelled.elf")
    args = parser.parse_args()

    console = Console(args.qemu, args.kernel)
    failures = 0
    try:
        banner = console.read_until_prompt(timeout=15)
        if not banner.endswith("kernelled:/$ "):
            raise TimeoutError("prompt at boot should be 'kernelled:/$ ', output:\n" + banner[-200:])
        print("  ok    shell prompt appeared")

        for command, expected in CHECKS:
            output = console.run(command)
            missing = [e for e in expected if e not in output]
            if missing:
                failures += 1
                print(f"  FAIL  {command!r}: missing {missing}\n        output was:\n{output}")
            else:
                print(f"  ok    {command}")

        # uptime must follow real time.
        first = uptime_seconds(console.run("uptime"))
        start = time.monotonic()
        time.sleep(2.0)
        second = uptime_seconds(console.run("uptime"))
        host = time.monotonic() - start
        if first is None or second is None or abs((second - first) - host) > 0.5:
            failures += 1
            print(f"  FAIL  uptime: kernel advanced {second} - {first}, host {host:.2f} s")
        else:
            print(f"  ok    uptime advanced {second - first:.2f} s in {host:.2f} s of real time")

        # Preemption: a process that never yields must not freeze the shell.
        console.run("spawn 3")
        start = time.monotonic()
        ps = console.run("ps")
        reply = time.monotonic() - start
        busy = re.search(r"\s(\d+)\s+(ready|running)\s.*spin", ps)
        if busy and reply < 1.0:
            print(f"  ok    shell answered 'ps' in {reply:.2f} s while 'spin' was using the CPU")
        else:
            failures += 1
            print(f"  FAIL  preemption: reply took {reply:.2f} s, ps was:\n{ps}")
        time.sleep(3.5)
        if "spin" in console.run("ps"):
            failures += 1
            print("  FAIL  spin process did not exit after 3 s")
        else:
            print("  ok    spin process exited and was removed")

        # kill: start a long one, kill it, check it is gone.
        pid = re.search(r"started process (\d+)", console.run("spawn 60"))
        console.run(f"kill {pid.group(1)}")
        time.sleep(0.2)
        if re.search(rf"^\s+{pid.group(1)}\s", console.run("ps"), re.M):
            failures += 1
            print("  FAIL  kill: process still listed")
        else:
            print(f"  ok    kill {pid.group(1)} removed the process")
    except TimeoutError as error:
        failures += 1
        print(f"  FAIL  {error}")
    finally:
        console.close()

    if failures:
        print(f"FAIL: {failures} shell check(s) failed")
        return 1
    print("PASS: the shell answered every command correctly")
    return 0


if __name__ == "__main__":
    sys.exit(main())
