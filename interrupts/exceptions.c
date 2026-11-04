/*
 * interrupts/exceptions.c - decode and handle exceptions taken to EL1.
 *
 * The Exception Syndrome Register (ESR_EL1) says why an exception happened:
 *   bits [31:26]  EC   exception class (what kind of event)
 *   bit  [25]     IL   instruction length (1 = 32-bit instruction)
 *   bits [24:0]   ISS  class-specific details
 * Encodings are from the Arm Architecture Reference Manual (ARM DDI 0487),
 * section D17.2.37 "ESR_EL1".
 */
#include <stdint.h>

#include "interrupts/exceptions.h"
#include "interrupts/irq.h"
#include "kernel/printk.h"

/* Exception classes KERNELLED recognises. */
#define EC_UNKNOWN           0x00
#define EC_WFX               0x01
#define EC_FP_ACCESS         0x07
#define EC_ILLEGAL_STATE     0x0E
#define EC_SVC64             0x15
#define EC_SYSREG            0x18
#define EC_IABORT_LOWER      0x20
#define EC_IABORT_SAME       0x21
#define EC_PC_ALIGN          0x22
#define EC_DABORT_LOWER      0x24
#define EC_DABORT_SAME       0x25
#define EC_SP_ALIGN          0x26
#define EC_SERROR            0x2F
#define EC_BRK64             0x3C

volatile uint64_t exceptions_svc_count;
volatile uint64_t exceptions_brk_count;

extern char exception_vectors[];

void exceptions_init(void)
{
    __asm__ volatile("msr vbar_el1, %0\n\tisb" : : "r"(exception_vectors) : "memory");
    kprintf("[init] exception vectors installed at %p\n", (void *)exception_vectors);
}

static const char *slot_name(uint64_t slot)
{
    static const char *const kinds[] = { "synchronous", "IRQ", "FIQ", "SError" };
    static const char *const sources[] = {
        "current EL, SP_EL0", "current EL, SP_ELx", "lower EL, AArch64", "lower EL, AArch32",
    };
    static char text[64];
    const char *kind = kinds[slot & 3];
    const char *source = sources[(slot >> 2) & 3];
    int i = 0;
    for (const char *s = kind; *s; s++) text[i++] = *s;
    for (const char *s = " from "; *s; s++) text[i++] = *s;
    for (const char *s = source; *s; s++) text[i++] = *s;
    text[i] = '\0';
    return text;
}

static const char *class_name(unsigned int ec)
{
    switch (ec) {
    case EC_UNKNOWN:       return "unknown or undefined instruction";
    case EC_WFX:           return "trapped WFI/WFE";
    case EC_FP_ACCESS:     return "FP/SIMD access while disabled";
    case EC_ILLEGAL_STATE: return "illegal execution state";
    case EC_SVC64:         return "SVC (system call)";
    case EC_SYSREG:        return "trapped system register access";
    case EC_IABORT_LOWER:  return "instruction abort from lower EL";
    case EC_IABORT_SAME:   return "instruction abort (kernel)";
    case EC_PC_ALIGN:      return "PC alignment fault";
    case EC_DABORT_LOWER:  return "data abort from lower EL";
    case EC_DABORT_SAME:   return "data abort (kernel)";
    case EC_SP_ALIGN:      return "SP alignment fault";
    case EC_SERROR:        return "SError (asynchronous system error)";
    case EC_BRK64:         return "BRK (breakpoint instruction)";
    default:               return "unrecognised exception class";
    }
}

/* For instruction and data aborts: the fault status code in ISS[5:0]. */
static const char *fault_status_name(unsigned int fsc)
{
    switch (fsc & 0x3C) {
    case 0x00: return "address size fault";
    case 0x04: return "translation fault";
    case 0x08: return "access flag fault";
    case 0x0C: return "permission fault";
    default: break;
    }
    switch (fsc) {
    case 0x10: return "synchronous external abort (no device at this address)";
    case 0x21: return "alignment fault (unaligned access)";
    default:   return "other fault status";
    }
}

void kernel_panic(const char *reason, const struct trap_frame *frame, uint64_t slot)
{
    /* Mask all interrupts so nothing interrupts the report. */
    __asm__ volatile("msr daifset, #0xf");

    unsigned int ec = (unsigned int)((frame->esr >> 26) & 0x3F);
    unsigned int iss = (unsigned int)(frame->esr & 0x1FFFFFF);

    kprintf("\n*** KERNEL PANIC: %s ***\n", reason);
    kprintf("  exception : %s\n", slot_name(slot));
    kprintf("  class     : 0x%02x %s\n", ec, class_name(ec));
    if (ec == EC_DABORT_SAME || ec == EC_DABORT_LOWER ||
        ec == EC_IABORT_SAME || ec == EC_IABORT_LOWER) {
        kprintf("  cause     : %s\n", fault_status_name(iss & 0x3F));
        if (ec == EC_DABORT_SAME || ec == EC_DABORT_LOWER) {
            kprintf("  access    : %s\n", (iss & (1u << 6)) ? "write" : "read");
        }
        kprintf("  address   : 0x%016lx  (FAR_EL1)\n", frame->far);
    }
    kprintf("  pc        : 0x%016lx  (ELR_EL1, faulting instruction)\n", frame->elr);
    kprintf("  ESR_EL1   : 0x%016lx\n", frame->esr);
    kprintf("  SPSR_EL1  : 0x%016lx\n", frame->spsr);
    kprintf("  sp        : 0x%016lx\n", frame->sp);
    kprintf("  registers :\n");
    for (int i = 0; i < 31; i++) {
        kprintf("    x%-2d 0x%016lx", i, frame->x[i]);
        if (i % 3 == 2 || i == 30) {
            kprintf("\n");
        }
    }
    kprintf("System halted.\n");

    for (;;) {
        __asm__ volatile("wfe");
    }
}

void exception_dispatch(struct trap_frame *frame, uint64_t slot)
{
    unsigned int ec = (unsigned int)((frame->esr >> 26) & 0x3F);
    unsigned int imm16 = (unsigned int)(frame->esr & 0xFFFF);

    /* Synchronous exceptions from kernel code (slot 4) that we can recover from. */
    if (slot == 4 && ec == EC_SVC64) {
        /* ELR already points at the instruction after SVC: just return. */
        exceptions_svc_count++;
        kprintf("[exception] SVC #0x%x handled, returning to 0x%016lx\n", imm16, frame->elr);
        return;
    }
    if (slot == 4 && ec == EC_BRK64) {
        /* ELR points at the BRK itself: skip it (each instruction is 4 bytes). */
        exceptions_brk_count++;
        kprintf("[exception] BRK #0x%x at 0x%016lx handled, resuming at next instruction\n",
                imm16, frame->elr);
        frame->elr += 4;
        return;
    }

    /* Slot 5: IRQ while the kernel was running (current EL, SP_ELx). */
    if (slot == 5) {
        irq_handle();
        return;
    }
    if ((slot & 3) == 1) {
        kernel_panic("IRQ from an unexpected origin", frame, slot);
    }
    kernel_panic("unhandled exception", frame, slot);
}
