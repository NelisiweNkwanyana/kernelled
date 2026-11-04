/*
 * arch/aarch64/qemu_virt.h - physical memory map of QEMU's "virt" machine.
 *
 * These addresses come from QEMU's hw/arm/virt.c (base_memmap) and are fixed
 * for the virt board, so the kernel can use them before it can parse the
 * device tree.
 */
#ifndef KERNELLED_ARCH_QEMU_VIRT_H
#define KERNELLED_ARCH_QEMU_VIRT_H

#define VIRT_UART0_BASE      0x09000000UL   /* PL011 UART, serial console     */
#define VIRT_UART0_CLOCK_HZ  24000000UL     /* PL011 reference clock (24 MHz) */
#define VIRT_RAM_BASE        0x40000000UL   /* start of RAM                   */

#endif /* KERNELLED_ARCH_QEMU_VIRT_H */
