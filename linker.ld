/*
 * linker.ld - memory layout of the KERNELLED kernel image.
 *
 * QEMU's "virt" board maps RAM at 0x40000000. QEMU places the device tree
 * blob at the start of RAM, so the kernel is linked 512 KiB higher, at
 * 0x40080000 (the same offset Linux traditionally uses on ARM64).
 */

ENTRY(_start)

/*
 * Program headers: one loadable segment per permission set, so code is never
 * writable and data is never executable (W^X). Without this the linker merges
 * everything into one read-write-execute segment and warns about it.
 * FLAGS: 4 = read, 2 = write, 1 = execute.
 */
PHDRS
{
    text   PT_LOAD FLAGS(5);   /* R-X: code                    */
    rodata PT_LOAD FLAGS(4);   /* R--: constants and strings   */
    data   PT_LOAD FLAGS(6);   /* RW-: variables, .bss, stack  */
}

KERNEL_BASE = 0x40080000;
BOOT_STACK_SIZE = 0x4000;   /* 16 KiB boot stack */

SECTIONS
{
    . = KERNEL_BASE;
    __kernel_start = .;

    .text : {
        KEEP(*(.text.boot))     /* boot.S must come first: it holds _start */
        *(.text .text.*)
    } :text

    . = ALIGN(4096);
    .rodata : {
        *(.rodata .rodata.*)
    } :rodata

    . = ALIGN(4096);
    .data : {
        *(.data .data.*)
    } :data

    . = ALIGN(16);
    .bss (NOLOAD) : {
        __bss_start = .;
        *(.bss .bss.*)
        *(COMMON)
        . = ALIGN(16);
        __bss_end = .;
    }

    .stack (NOLOAD) : {
        . = ALIGN(16);
        __stack_bottom = .;
        . = . + BOOT_STACK_SIZE;
        __stack_top = .;        /* AArch64 requires SP to be 16-byte aligned */
    }

    __kernel_end = .;

    /DISCARD/ : {
        *(.comment)
        *(.note .note.*)
        *(.eh_frame .eh_frame_hdr)
    }
}
