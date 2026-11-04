/*
 * kernel/printk.c - a small printf for the kernel.
 *
 * There is no C library in the kernel, so this implements the handful of
 * printf conversions KERNELLED needs and writes them to the UART.
 */
#include <stdarg.h>
#include <stdint.h>

#include "drivers/uart.h"
#include "kernel/printk.h"

static int left_align;          /* set by the '-' flag for the current conversion */

static void print_padding(char pad, int count)
{
    while (count-- > 0) {
        uart_putc(pad);
    }
}

/* Print an unsigned number in base 10 or 16, right-aligned in `width`. */
static void print_unsigned(uint64_t value, unsigned int base, int width, char pad)
{
    static const char digits[] = "0123456789abcdef";
    char buffer[24];
    int length = 0;

    do {
        buffer[length++] = digits[value % base];
        value /= base;
    } while (value != 0);

    int padding = width - length;
    if (!left_align) {
        print_padding(pad, padding);
    }
    while (length > 0) {
        uart_putc(buffer[--length]);
    }
    if (left_align) {
        print_padding(' ', padding);
    }
}

void kprintf(const char *format, ...)
{
    va_list args;
    va_start(args, format);

    for (const char *p = format; *p != '\0'; p++) {
        if (*p != '%') {
            uart_putc(*p);
            continue;
        }
        p++;

        char pad = ' ';
        int width = 0;
        int is_long = 0;

        left_align = 0;
        if (*p == '-') {
            left_align = 1;
            p++;
        }
        if (*p == '0') {
            pad = '0';
            p++;
        }
        while (*p >= '0' && *p <= '9') {
            width = width * 10 + (*p - '0');
            p++;
        }
        while (*p == 'l') {
            is_long = 1;
            p++;
        }

        switch (*p) {
        case 's': {
            const char *text = va_arg(args, const char *);
            if (text == 0) {
                text = "(null)";
            }
            int length = 0;
            while (text[length] != '\0') {
                length++;
            }
            if (!left_align) {
                print_padding(' ', width - length);
            }
            uart_puts(text);
            if (left_align) {
                print_padding(' ', width - length);
            }
            break;
        }
        case 'c':
            uart_putc((char)va_arg(args, int));
            break;
        case 'd': {
            int64_t value = is_long ? va_arg(args, int64_t) : va_arg(args, int);
            if (value < 0) {
                uart_putc('-');
                print_unsigned((uint64_t)(-(value + 1)) + 1, 10, width - 1, pad);
            } else {
                print_unsigned((uint64_t)value, 10, width, pad);
            }
            break;
        }
        case 'u':
            print_unsigned(is_long ? va_arg(args, uint64_t) : va_arg(args, unsigned int), 10, width, pad);
            break;
        case 'x':
            print_unsigned(is_long ? va_arg(args, uint64_t) : va_arg(args, unsigned int), 16, width, pad);
            break;
        case 'p':
            uart_puts("0x");
            print_unsigned((uint64_t)(uintptr_t)va_arg(args, void *), 16, 16, '0');
            break;
        case '%':
            uart_putc('%');
            break;
        case '\0':
            p--;                    /* format ended with a lone '%' */
            break;
        default:                    /* unknown conversion: print it as-is */
            uart_putc('%');
            uart_putc(*p);
            break;
        }
    }

    va_end(args);
}
