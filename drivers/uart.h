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

/*
 * Input (Phase 7). Received bytes are collected by the UART interrupt into a
 * 256-byte buffer; uart_getc() takes one, putting the calling process to
 * sleep (BLOCKED) until a byte arrives, so waiting costs no CPU time.
 */
void uart_enable_rx_interrupt(void);
void uart_handle_irq(void);
char uart_getc(void);

#endif /* KERNELLED_DRIVERS_UART_H */
