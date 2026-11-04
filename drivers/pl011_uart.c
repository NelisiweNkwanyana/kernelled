/*
 * drivers/pl011_uart.c - driver for the ARM PrimeCell PL011 UART.
 *
 * QEMU's virt machine connects PL011 UART0 to the host terminal when QEMU is
 * started with -nographic, so everything written here appears in Terminal.
 * Register offsets are from the ARM PL011 Technical Reference Manual (DDI 0183).
 */
#include "drivers/uart.h"
#include "arch/aarch64/cpu.h"
#include "arch/aarch64/mmio.h"
#include "arch/aarch64/qemu_virt.h"
#include "drivers/gic.h"
#include "process/process.h"

/* Register offsets */
#define UART_DR     0x00    /* Data register                       */
#define UART_FR     0x18    /* Flag register                       */
#define UART_IBRD   0x24    /* Integer baud-rate divisor           */
#define UART_FBRD   0x28    /* Fractional baud-rate divisor        */
#define UART_LCRH   0x2C    /* Line control                        */
#define UART_CR     0x30    /* Control                             */
#define UART_IMSC   0x38    /* Interrupt mask set/clear            */
#define UART_ICR    0x44    /* Interrupt clear                     */

/* Flag register bits */
#define FR_BUSY     (1u << 3)
#define FR_RXFE     (1u << 4)   /* receive FIFO empty */
#define FR_TXFF     (1u << 5)   /* transmit FIFO full */

/* Interrupt bits (IMSC and ICR) */
#define INT_RX      (1u << 4)   /* receive FIFO reached its trigger level */
#define INT_RT      (1u << 6)   /* receive timeout: bytes waiting, line idle */

/* Line control bits */
#define LCRH_FEN    (1u << 4)   /* enable FIFOs           */
#define LCRH_WLEN_8 (3u << 5)   /* 8 data bits            */

/* Control bits */
#define CR_UARTEN   (1u << 0)
#define CR_TXE      (1u << 8)
#define CR_RXE      (1u << 9)

#define UART_BAUD_RATE 115200u

static inline uintptr_t reg(uint32_t offset)
{
    return VIRT_UART0_BASE + offset;
}

void uart_init(void)
{
    /* Disable the UART while it is being configured. */
    mmio_write32(reg(UART_CR), 0);
    while (mmio_read32(reg(UART_FR)) & FR_BUSY) {
    }

    /* Mask and clear all UART interrupts (interrupts arrive in Phase 2). */
    mmio_write32(reg(UART_IMSC), 0);
    mmio_write32(reg(UART_ICR), 0x7FF);

    /*
     * Baud-rate divisor = clock / (16 * baud) = 24 MHz / 1,843,200 = 13.0208.
     * Integer part 13; fractional part round(0.0208 * 64) = 1.
     * QEMU does not need this, but real PL011 hardware does.
     */
    uint32_t divisor_x64 = (uint32_t)((VIRT_UART0_CLOCK_HZ * 4u) / UART_BAUD_RATE);
    mmio_write32(reg(UART_IBRD), divisor_x64 >> 6);
    mmio_write32(reg(UART_FBRD), divisor_x64 & 0x3F);

    /* 8 data bits, no parity, 1 stop bit, FIFOs on. */
    mmio_write32(reg(UART_LCRH), LCRH_WLEN_8 | LCRH_FEN);

    /* Enable the UART with transmit and receive. */
    mmio_write32(reg(UART_CR), CR_UARTEN | CR_TXE | CR_RXE);
}

void uart_putc(char c)
{
    if (c == '\n') {
        uart_putc('\r');            /* terminals expect CR+LF */
    }
    while (mmio_read32(reg(UART_FR)) & FR_TXFF) {
        /* wait for space in the transmit FIFO */
    }
    mmio_write32(reg(UART_DR), (uint32_t)(unsigned char)c);
}

void uart_puts(const char *text)
{
    while (*text) {
        uart_putc(*text++);
    }
}

void uart_put_hex(uint64_t value)
{
    static const char digits[] = "0123456789abcdef";
    uart_puts("0x");
    for (int shift = 60; shift >= 0; shift -= 4) {
        uart_putc(digits[(value >> shift) & 0xF]);
    }
}

void uart_put_dec(uint64_t value)
{
    char buffer[21];                /* 2^64 has 20 decimal digits */
    int length = 0;
    do {
        buffer[length++] = (char)('0' + (value % 10));
        value /= 10;
    } while (value != 0);
    while (length > 0) {
        uart_putc(buffer[--length]);
    }
}

/* ---- Input ---------------------------------------------------------------- */

#define RX_BUFFER_SIZE 256

static char rx_buffer[RX_BUFFER_SIZE];
static volatile unsigned int rx_head;       /* next slot the interrupt writes */
static volatile unsigned int rx_tail;       /* next slot uart_getc reads */
static struct process *volatile rx_waiter;  /* process blocked in uart_getc */

void uart_enable_rx_interrupt(void)
{
    /* Both "FIFO filling up" and "timeout" are needed: with FIFOs on, a single
     * key press never reaches the trigger level, only the timeout fires. */
    mmio_write32(reg(UART_ICR), INT_RX | INT_RT);
    mmio_write32(reg(UART_IMSC), INT_RX | INT_RT);
    gic_enable_interrupt(VIRT_IRQ_UART0, 0xA0);
}

void uart_handle_irq(void)
{
    while (!(mmio_read32(reg(UART_FR)) & FR_RXFE)) {
        char c = (char)(mmio_read32(reg(UART_DR)) & 0xFF);
        unsigned int next = (rx_head + 1) % RX_BUFFER_SIZE;
        if (next != rx_tail) {          /* buffer full: drop the byte */
            rx_buffer[rx_head] = c;
            rx_head = next;
        }
    }
    mmio_write32(reg(UART_ICR), INT_RX | INT_RT);
    if (rx_waiter) {
        process_wake(rx_waiter);
    }
}

char uart_getc(void)
{
    for (;;) {
        /* Check and sleep with IRQs masked: a byte arriving between the
         * check and process_block() would otherwise never wake us. */
        uint64_t flags = cpu_irq_save();
        if (rx_tail != rx_head) {
            char c = rx_buffer[rx_tail];
            rx_tail = (rx_tail + 1) % RX_BUFFER_SIZE;
            cpu_irq_restore(flags);
            return c;
        }
        rx_waiter = process_current();
        process_block();
        rx_waiter = 0;
        cpu_irq_restore(flags);
    }
}
