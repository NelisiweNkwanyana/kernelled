/*
 * kernel/string.c - byte-at-a-time implementations. Simple and always
 * aligned, which matters because the kernel is built with -mstrict-align.
 */
#include "kernel/string.h"

void *memset(void *destination, int value, size_t count)
{
    unsigned char *d = destination;
    while (count--) {
        *d++ = (unsigned char)value;
    }
    return destination;
}

void *memcpy(void *destination, const void *source, size_t count)
{
    unsigned char *d = destination;
    const unsigned char *s = source;
    while (count--) {
        *d++ = *s++;
    }
    return destination;
}

void *memmove(void *destination, const void *source, size_t count)
{
    unsigned char *d = destination;
    const unsigned char *s = source;
    if (d < s) {
        while (count--) {
            *d++ = *s++;
        }
    } else {
        while (count--) {
            d[count] = s[count];
        }
    }
    return destination;
}

int memcmp(const void *a, const void *b, size_t count)
{
    const unsigned char *x = a, *y = b;
    for (size_t i = 0; i < count; i++) {
        if (x[i] != y[i]) {
            return x[i] - y[i];
        }
    }
    return 0;
}

size_t strlen(const char *text)
{
    size_t length = 0;
    while (text[length] != '\0') {
        length++;
    }
    return length;
}

int strcmp(const char *a, const char *b)
{
    while (*a != '\0' && *a == *b) {
        a++;
        b++;
    }
    return (unsigned char)*a - (unsigned char)*b;
}

int strncmp(const char *a, const char *b, size_t count)
{
    for (size_t i = 0; i < count; i++) {
        if (a[i] != b[i] || a[i] == '\0') {
            return (unsigned char)a[i] - (unsigned char)b[i];
        }
    }
    return 0;
}

size_t strlcpy(char *destination, const char *source, size_t size)
{
    size_t length = strlen(source);
    if (size > 0) {
        size_t n = length < size - 1 ? length : size - 1;
        memcpy(destination, source, n);
        destination[n] = '\0';
    }
    return length;
}
