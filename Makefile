# KERNELLED - build automation
#
#   make            build build/kernelled.elf and build/kernelled.bin
#   make run        boot KERNELLED in QEMU (quit: Ctrl-A, then X)
#   make test       boot headlessly and check the console output, then check
#                   that every deliberate fault produces a crash report
#   make run FAULT=undefined|data-abort|alignment
#                   boot a kernel that deliberately crashes (Phase 2a)
#   make debug      start QEMU paused, waiting for GDB on port 1234
#   make gdb        attach GDB to a running "make debug" (second terminal)
#   make disasm     write a disassembly to build/kernelled.lst
#   make env        check that the required tools are installed
#   make clean      remove build output
#
# Toolchains:
#   make                    GNU cross toolchain (Homebrew: aarch64-elf-gcc)
#   make TOOLCHAIN=llvm     clang + ld.lld (Homebrew: llvm and lld)

TOOLCHAIN ?= gnu
CROSS     ?= aarch64-elf-

ifeq ($(TOOLCHAIN),llvm)
  CC      := clang --target=aarch64-none-elf
  LD      := ld.lld
  OBJCOPY := llvm-objcopy
  OBJDUMP := llvm-objdump
else
  CC      := $(CROSS)gcc
  LD      := $(CROSS)ld
  OBJCOPY := $(CROSS)objcopy
  OBJDUMP := $(CROSS)objdump
endif

QEMU     ?= qemu-system-aarch64
GDB      ?= aarch64-elf-gdb
PYTHON   ?= python3

# FAULT=<name> builds a separate kernel that crashes on purpose after boot.
FAULTS   := undefined data-abort alignment
FAULT    ?=
ifeq ($(FAULT),)
  BUILD  := build
else
  ifeq ($(filter $(FAULT),$(FAULTS)),)
    $(error Unknown FAULT '$(FAULT)'. Choose one of: $(FAULTS))
  endif
  BUILD  := build/fault-$(FAULT)
  FAULT_DEFINE := -DKERNELLED_FAULT_$(shell echo $(FAULT) | tr 'a-z-' 'A-Z_')
endif

KERNEL   := $(BUILD)/kernelled.elf
IMAGE    := $(BUILD)/kernelled.bin

# Every source file of the kernel. New modules are added here phase by phase.
SOURCES_S := boot/boot.S \
             arch/aarch64/vectors.S
SOURCES_C := kernel/kernel.c \
             kernel/printk.c \
             kernel/selftest.c \
             interrupts/exceptions.c \
             interrupts/irq.c \
             drivers/pl011_uart.c \
             drivers/gic.c \
             drivers/timer.c

OBJECTS := $(SOURCES_S:%.S=$(BUILD)/%.o) $(SOURCES_C:%.c=$(BUILD)/%.o)

# -ffreestanding        no hosted C library; only compiler headers (stdint.h, ...)
# -mgeneral-regs-only   no floating-point/SIMD registers in kernel code
# -mstrict-align        no unaligned accesses: with the MMU off, all memory is
#                       "Device" memory and an unaligned access would fault
# -fno-pic              the kernel runs at its fixed link address
ARCHFLAGS := -mcpu=cortex-a72 -mgeneral-regs-only -mstrict-align
CFLAGS    := $(ARCHFLAGS) -std=c11 -ffreestanding -fno-builtin -fno-stack-protector \
             -fno-pic -fno-common -O2 -g -Wall -Wextra -Werror -I. -MMD -MP $(FAULT_DEFINE)
ASFLAGS   := $(ARCHFLAGS) -g -I. -MMD -MP
LDFLAGS   := -nostdlib -T linker.ld

# QEMU's "virt" board, one Cortex-A72 core, 128 MiB RAM, serial on the terminal.
QEMU_FLAGS := -machine virt -cpu cortex-a72 -smp 1 -m 128M -nographic -kernel $(KERNEL)

.PHONY: all run test debug gdb disasm env clean

all: $(KERNEL) $(IMAGE)

$(KERNEL): $(OBJECTS) linker.ld
	$(LD) $(LDFLAGS) -o $@ $(OBJECTS)
	@echo "Built $@"

$(IMAGE): $(KERNEL)
	$(OBJCOPY) -O binary $< $@

$(BUILD)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/%.o: %.S
	@mkdir -p $(dir $@)
	$(CC) $(ASFLAGS) -c $< -o $@

run: $(KERNEL)
	$(QEMU) $(QEMU_FLAGS)

test: $(KERNEL)
	$(PYTHON) tests/test_boot.py --qemu "$(QEMU)" --kernel $(KERNEL)
	$(PYTHON) tests/test_exceptions.py --qemu "$(QEMU)" --make "$(MAKE) TOOLCHAIN=$(TOOLCHAIN)"

debug: $(KERNEL)
	@echo "QEMU is paused. In a second terminal run: make gdb"
	$(QEMU) $(QEMU_FLAGS) -S -s

gdb: $(KERNEL)
	$(GDB) $(KERNEL) -ex "target remote localhost:1234" -ex "break kernel_main" -ex "continue"

disasm: $(KERNEL)
	$(OBJDUMP) -d $(KERNEL) > $(BUILD)/kernelled.lst
	@echo "Wrote $(BUILD)/kernelled.lst"

env:
	@sh scripts/check_env.sh

clean:
	rm -rf $(BUILD)

-include $(OBJECTS:.o=.d)
