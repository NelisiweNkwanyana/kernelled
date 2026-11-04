/*
 * drivers/uart.h - serial console interface (PL011 UART on QEMU virt).
 */
#ifndef KERNELLED_DRIVERS_UART_H
#define KERNELLED_DRIVERS_UART_H

#include <stdint.h>

void uart_init(void);
void uart_putc(char c);
void uart_puts(const char *text);
void uart_put_hex(uint64_t value);
void uart_put_dec(uint64_t value);

#endif /* KERNELLED_DRIVERS_UART_H */
