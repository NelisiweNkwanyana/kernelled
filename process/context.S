/*
 * process/context.S - switch the CPU from one process to another.
 *
 * void context_switch(struct cpu_context *from, struct cpu_context *to);
 *
 * Saves the callee-saved registers, frame pointer, return address and stack
 * pointer of the running process into *from, loads those of the next
 * process from *to, and returns - but on the other process's stack, to the
 * place where *that* process last called context_switch. Field offsets
 * match struct cpu_context in process.h.
 */
    .text
    .global context_switch
    .global process_trampoline

context_switch:
    stp     x19, x20, [x0, #0]
    stp     x21, x22, [x0, #16]
    stp     x23, x24, [x0, #32]
    stp     x25, x26, [x0, #48]
    stp     x27, x28, [x0, #64]
    stp     x29, x30, [x0, #80]
    mov     x9, sp
    str     x9, [x0, #96]

    ldp     x19, x20, [x1, #0]
    ldp     x21, x22, [x1, #16]
    ldp     x23, x24, [x1, #32]
    ldp     x25, x26, [x1, #48]
    ldp     x27, x28, [x1, #64]
    ldp     x29, x30, [x1, #80]
    ldr     x9, [x1, #96]
    mov     sp, x9
    ret                             /* jumps to the new process's x30 */

/*
 * A brand-new process has never called context_switch, so process_create
 * points its saved x30 here, with the entry function in x19 and its argument
 * in x20. This passes them to process_start() in C.
 */
process_trampoline:
    mov     x0, x19
    mov     x1, x20
    bl      process_start
    b       .                       /* process_start never returns */
