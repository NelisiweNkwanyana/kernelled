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

/* GICv2 interrupt controller (QEMU virt default, gic-version=2). */
#define VIRT_GICD_BASE       0x08000000UL   /* distributor                    */
#define VIRT_GICC_BASE       0x08010000UL   /* CPU interface                  */

/*
 * Interrupt IDs (INTIDs). IDs 16-31 are PPIs, private to each core.
 * The EL1 physical timer is PPI 14, which the GIC numbers 16 + 14 = 30.
 */
#define VIRT_IRQ_TIMER_EL1_PHYS  30

#endif /* KERNELLED_ARCH_QEMU_VIRT_H */
