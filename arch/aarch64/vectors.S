/*
 * arch/aarch64/vectors.S - the EL1 exception vector table.
 *
 * When an exception is taken to EL1 the CPU jumps to VBAR_EL1 + offset, where
 * the offset depends on the exception type and where it came from:
 *
 *                               Sync    IRQ     FIQ     SError
 *   current EL, using SP_EL0    0x000   0x080   0x100   0x180
 *   current EL, using SP_ELx    0x200   0x280   0x300   0x380   <- the kernel
 *   lower EL, AArch64           0x400   0x480   0x500   0x580   <- user mode, later
 *   lower EL, AArch32           0x600   0x680   0x700   0x780
 *
 * Each slot is only 0x80 bytes (32 instructions), so every entry saves x0/x1,
 * puts its slot number (0-15) in x1 and branches to one shared routine that
 * saves the rest of the CPU state as a struct trap_frame (trap_frame.h).
 */

#define TRAP_FRAME_SIZE 288

    .macro vector_entry slot
    .balign 0x80
    sub     sp, sp, #TRAP_FRAME_SIZE
    stp     x0, x1, [sp, #0]
    mov     x1, #\slot
    b       exception_common
    .endm

    .section ".text.vectors", "ax"
    .balign 0x800                   /* VBAR_EL1 requires 2 KiB alignment */
    .global exception_vectors
exception_vectors:
    vector_entry 0                  /* current EL, SP_EL0: sync   */
    vector_entry 1                  /*                     irq    */
    vector_entry 2                  /*                     fiq    */
    vector_entry 3                  /*                     serror */
    vector_entry 4                  /* current EL, SP_ELx: sync   */
    vector_entry 5                  /*                     irq    */
    vector_entry 6                  /*                     fiq    */
    vector_entry 7                  /*                     serror */
    vector_entry 8                  /* lower EL, AArch64:  sync   */
    vector_entry 9                  /*                     irq    */
    vector_entry 10                 /*                     fiq    */
    vector_entry 11                 /*                     serror */
    vector_entry 12                 /* lower EL, AArch32:  sync   */
    vector_entry 13                 /*                     irq    */
    vector_entry 14                 /*                     fiq    */
    vector_entry 15                 /*                     serror */

/*
 * Shared entry path. On arrival: sp points at a fresh trap frame with x0/x1
 * already stored, and x1 = slot number.
 */
exception_common:
    stp     x2, x3,   [sp, #16]
    stp     x4, x5,   [sp, #32]
    stp     x6, x7,   [sp, #48]
    stp     x8, x9,   [sp, #64]
    stp     x10, x11, [sp, #80]
    stp     x12, x13, [sp, #96]
    stp     x14, x15, [sp, #112]
    stp     x16, x17, [sp, #128]
    stp     x18, x19, [sp, #144]
    stp     x20, x21, [sp, #160]
    stp     x22, x23, [sp, #176]
    stp     x24, x25, [sp, #192]
    stp     x26, x27, [sp, #208]
    stp     x28, x29, [sp, #224]
    add     x2, sp, #TRAP_FRAME_SIZE        /* sp as it was before the trap */
    stp     x30, x2,  [sp, #240]
    mrs     x2, elr_el1
    mrs     x3, spsr_el1
    stp     x2, x3,   [sp, #256]
    mrs     x2, esr_el1
    mrs     x3, far_el1
    stp     x2, x3,   [sp, #272]

    mov     x0, sp                          /* arg 0: struct trap_frame *   */
    bl      exception_dispatch              /* arg 1: slot number (x1)      */

    /* The handler may have changed elr/spsr (e.g. to skip an instruction). */
    ldp     x2, x3,   [sp, #256]
    msr     elr_el1, x2
    msr     spsr_el1, x3

    ldp     x0, x1,   [sp, #0]
    ldp     x2, x3,   [sp, #16]
    ldp     x4, x5,   [sp, #32]
    ldp     x6, x7,   [sp, #48]
    ldp     x8, x9,   [sp, #64]
    ldp     x10, x11, [sp, #80]
    ldp     x12, x13, [sp, #96]
    ldp     x14, x15, [sp, #112]
    ldp     x16, x17, [sp, #128]
    ldp     x18, x19, [sp, #144]
    ldp     x20, x21, [sp, #160]
    ldp     x22, x23, [sp, #176]
    ldp     x24, x25, [sp, #192]
    ldp     x26, x27, [sp, #208]
    ldp     x28, x29, [sp, #224]
    ldr     x30,      [sp, #240]
    add     sp, sp, #TRAP_FRAME_SIZE
    eret                                    /* resume at ELR_EL1 */
