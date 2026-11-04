#!/bin/sh
# scripts/check_env.sh - verify the KERNELLED development tools are installed.
# Run with: make env   (or: sh scripts/check_env.sh)

status=0

check() {
    name="$1"; shift
    if command -v "$1" >/dev/null 2>&1; then
        version=$("$@" 2>/dev/null | head -n 1)
        printf '  ok       %-22s %s\n' "$name" "$version"
    else
        printf '  MISSING  %-22s install: %s\n' "$name" "$INSTALL_HINT"
        status=1
    fi
}

echo "KERNELLED environment check"
INSTALL_HINT="brew install qemu";                check "QEMU (ARM64 system)" qemu-system-aarch64 --version
INSTALL_HINT="brew install aarch64-elf-gcc";     check "Cross C compiler"    aarch64-elf-gcc --version
INSTALL_HINT="brew install aarch64-elf-binutils"; check "Cross linker"       aarch64-elf-ld --version
INSTALL_HINT="brew install aarch64-elf-gdb";     check "Cross debugger"      aarch64-elf-gdb --version
INSTALL_HINT="xcode-select --install";           check "make"                make --version
INSTALL_HINT="brew install python";              check "python3"             python3 --version
INSTALL_HINT="xcode-select --install";           check "git"                 git --version

if [ "$status" -eq 0 ]; then
    echo "All tools found. Next: make && make run"
else
    echo "Some tools are missing. Install them with the commands shown, then run make env again."
fi
exit "$status"
