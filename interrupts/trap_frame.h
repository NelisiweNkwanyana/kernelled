/*
 * interrupts/trap_frame.h - CPU state saved when an exception is taken.
 *
 * arch/aarch64/vectors.S pushes exactly this layout onto the stack, so the
 * offsets here and in the assembly must match. The _Static_asserts below make
 * the build fail if they ever drift apart.
 */
#ifndef KERNELLED_INTERRUPTS_TRAP_FRAME_H
#define KERNELLED_INTERRUPTS_TRAP_FRAME_H

#include <stddef.h>
#include <stdint.h>

#define TRAP_FRAME_SIZE 288         /* bytes; a multiple of 16 for SP alignment */

struct trap_frame {
    uint64_t x[31];                 /* general-purpose registers x0..x30    0  */
    uint64_t sp;                    /* stack pointer before the exception 248 */
    uint64_t elr;                   /* ELR_EL1: where to return to         256 */
    uint64_t spsr;                  /* SPSR_EL1: saved processor state     264 */
    uint64_t esr;                   /* ESR_EL1: why the exception happened 272 */
    uint64_t far;                   /* FAR_EL1: faulting address           280 */
};

_Static_assert(sizeof(struct trap_frame) == TRAP_FRAME_SIZE, "trap_frame size mismatch");
_Static_assert(offsetof(struct trap_frame, sp) == 248, "trap_frame.sp offset mismatch");
_Static_assert(offsetof(struct trap_frame, elr) == 256, "trap_frame.elr offset mismatch");
_Static_assert(offsetof(struct trap_frame, esr) == 272, "trap_frame.esr offset mismatch");

#endif /* KERNELLED_INTERRUPTS_TRAP_FRAME_H */
