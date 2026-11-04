/*
 * boot/boot.S - the first instructions KERNELLED executes.
 *
 * QEMU loads build/kernelled.elf at its link address (0x40080000, see
 * linker.ld) and starts every CPU core at _start with the MMU and caches off.
 * Before any C code can run we must:
 *   1. keep only one core running (the others are parked),
 *   2. make sure we are in Exception Level 1 (kernel mode),
 *   3. give C a stack,
 *   4. zero the .bss section (C expects uninitialised globals to be 0),
 *   5. call kernel_main().
 */

    .section ".text.boot", "ax"
    .global _start

_start:
    /* --- 1. Park every core except core 0 -------------------------------- */
    mrs     x0, mpidr_el1           /* Multiprocessor Affinity Register      */
    and     x0, x0, #0xFF           /* Aff0 = core number within the cluster */
    cbnz    x0, park                /* not core 0 -> sleep forever           */

    /* --- 2. Make sure we run at EL1 --------------------------------------- */
    mrs     x0, CurrentEL           /* bits [3:2] hold the exception level   */
    lsr     x0, x0, #2
    cmp     x0, #1
    b.eq    at_el1                  /* QEMU virt's default: already EL1      */
    cmp     x0, #2
    b.ne    park                    /* EL3 is not supported in Phase 1       */

    /* We are at EL2 (e.g. QEMU with -machine virtualization=on): drop to EL1. */
    mov     x0, #(1 << 31)          /* HCR_EL2.RW = 1: EL1 runs in AArch64   */
    msr     hcr_el2, x0
    /* Let EL1 use the physical counter and timer (Phase 2b). QEMU already
     * allows this, but the architecture leaves CNTHCTL_EL2's reset value
     * unknown, so we set the two bits ourselves. */
    mrs     x0, cnthctl_el2
    orr     x0, x0, #0x3            /* EL1PCTEN | EL1PCEN                    */
    msr     cnthctl_el2, x0
    msr     cntvoff_el2, xzr        /* virtual counter = physical counter    */
    mov     x0, #0x3c5              /* SPSR: mask D,A,I,F; M[3:0]=0101 EL1h  */
    msr     spsr_el2, x0
    adr     x0, at_el1
    msr     elr_el2, x0             /* "return" address = at_el1             */
    eret

at_el1:
    /* --- 3. Set up the stack (grows downwards from __stack_top) ----------- */
    ldr     x0, =__stack_top
    mov     sp, x0

    /* --- 4. Zero .bss (8 bytes at a time; linker.ld aligns it to 16) ----- */
    ldr     x1, =__bss_start
    ldr     x2, =__bss_end
zero_bss:
    cmp     x1, x2
    b.hs    bss_done
    str     xzr, [x1], #8
    b       zero_bss
bss_done:

    /* --- 5. Enter the C kernel -------------------------------------------- */
    bl      kernel_main

    /* kernel_main should never return; if it does, fall through and park.   */
park:
    wfe                             /* low-power wait for an event           */
    b       park
