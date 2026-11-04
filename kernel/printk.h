/*
 * kernel/printk.h - formatted output to the serial console.
 */
#ifndef KERNELLED_KERNEL_PRINTK_H
#define KERNELLED_KERNEL_PRINTK_H

/*
 * Supported conversions: %s %c %d %u %x %p %%
 * Modifiers: 'l' / 'll' for 64-bit values (e.g. %lx), '0' + width (e.g. %016lx),
 *            '-' + width to left-align (e.g. %-2d).
 */
void kprintf(const char *format, ...) __attribute__((format(printf, 1, 2)));

#endif /* KERNELLED_KERNEL_PRINTK_H */
