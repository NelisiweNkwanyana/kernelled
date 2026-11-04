/*
 * shell/shell.h - KERNELLED's interactive command line.
 */
#ifndef KERNELLED_SHELL_SHELL_H
#define KERNELLED_SHELL_SHELL_H

/* Entry point of the "shell" process. Reads commands from the serial console
 * forever. */
void shell_main(void *argument);

#endif /* KERNELLED_SHELL_SHELL_H */
