# Phase 2b: Interrupts and the system timer

## Goal

Hardware can now interrupt the CPU. The ARM generic timer raises an interrupt 100 times per second (one *tick* every 10 ms) and the kernel counts the ticks. This tick is what the scheduler (Phase 5) will use to switch processes, and what `uptime` (Phase 7) will report.

## The path of one tick

```
generic timer          GICv2                      CPU
CNTPCT >= CVAL  --->  distributor (INTID 30)  --->  IRQ exception
                      cpu interface                 vector slot 0x280 (IRQ, current EL, SP_ELx)
                                                    exception_common saves registers
                                                    exception_dispatch -> irq_handle()
                                                       gic_acknowledge()   read GICC_IAR -> INTID 30
                                                       timer_handle_irq()  ticks++, CVAL += interval
                                                       scheduler_tick()    (Phase 5: time slices)
                                                       gic_end_of_interrupt() write GICC_EOIR
                                                    schedule() if a switch is due (Phase 5)
                                                    restore registers, eret
```

## Generic timer (`drivers/timer.c`)

| Register | Meaning |
|---|---|
| `CNTFRQ_EL0` | Counter frequency (62.5 MHz on QEMU virt with cortex-a72) |
| `CNTPCT_EL0` | 64-bit counter, always counting up |
| `CNTP_CVAL_EL0` | Deadline: the timer fires when `CNTPCT >= CVAL` |
| `CNTP_CTL_EL0` | Bit 0 enable, bit 1 mask, bit 2 status |

Each tick sets the next deadline to *previous deadline + interval*, not *now + interval*, so a late interrupt never makes the clock drift; the tick count catches up instead.

## GICv2 (`drivers/gic.c`)

| Part | Address | Used for |
|---|---|---|
| Distributor (GICD) | `0x08000000` | Enable interrupts, set priority, route SPIs to core 0 |
| CPU interface (GICC) | `0x08010000` | `IAR` (which interrupt?), `EOIR` (done), `PMR` (priority mask) |

Interrupt IDs: 0–15 SGIs (software, between cores), 16–31 PPIs (private per core, e.g. timers), 32+ SPIs (shared devices such as the UART). The EL1 physical timer is PPI 14 = INTID 30.

`gic_init()` first reads `GICD_PIDR2` and refuses to continue unless it reports GICv2, so a mismatched machine is reported instead of misprogrammed.

## Order at boot

1. `gic_init()`: everything disabled and cleared, then the GIC is switched on.
2. `timer_init(100)`: first deadline set, timer enabled, INTID 30 enabled in the GIC.
3. `cpu_irq_enable()`: clear the I bit in `DAIF`; from now on interrupts are delivered.
4. `selftest_timer()`: busy-waits exactly 500 ms of counter time and checks that 50 ± 1 ticks arrived.
5. Idle loop: `wfi` sleeps until the next interrupt. (In Phase 2b it printed uptime every second; from Phase 4 this loop is the idle process, and uptime is the shell's `uptime` command.)

## Testing

| Check | Where | Proves |
|---|---|---|
| `50 ticks in 500 ms` | kernel self-test | tick rate matches the hardware counter |
| `uptime` advances 2.0 ± 0.5 s while the host waits 2 s | `tests/test_shell.py` (was `test_boot.py` in Phase 2b) | the kernel's clock matches real time |
| IRQs left masked → `0 ticks ... FAIL` | manual negative test | the test catches a broken interrupt path |
| deliberate faults still panic correctly | `tests/test_exceptions.py` | Phase 2a still works |
