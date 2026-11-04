/*
 * kernel/string.h - the few C string/memory functions the kernel needs.
 *
 * There is no C library in the kernel. The compiler may also emit calls to
 * memset/memcpy on its own (for example to zero a large struct), so these
 * must exist under their standard names.
 */
#ifndef KERNELLED_KERNEL_STRING_H
#define KERNELLED_KERNEL_STRING_H

#include <stddef.h>

void *memset(void *destination, int value, size_t count);
void *memcpy(void *destination, const void *source, size_t count);
void *memmove(void *destination, const void *source, size_t count);
int memcmp(const void *a, const void *b, size_t count);
size_t strlen(const char *text);
int strcmp(const char *a, const char *b);
int strncmp(const char *a, const char *b, size_t count);

/* Copy at most size-1 characters and always NUL-terminate. Returns strlen(source). */
size_t strlcpy(char *destination, const char *source, size_t size);

#endif /* KERNELLED_KERNEL_STRING_H */
