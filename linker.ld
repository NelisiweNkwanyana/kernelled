/*
 * linker.ld - memory layout of the KERNELLED kernel image.
 *
 * QEMU's "virt" board maps RAM at 0x40000000. For an ELF kernel, QEMU puts
 * the device tree blob (1 MiB on virt) at the start of RAM, but only if it
 * fits below the kernel (hw/arm/boot.c). The kernel is therefore linked
 * 2 MiB into RAM, at 0x40200000, leaving room for the blob. Phase 1 and 2
 * used 0x40080000, where the blob silently did not fit.
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

KERNEL_BASE = 0x40200000;
BOOT_STACK_SIZE = 0x4000;   /* 16 KiB boot stack */

SECTIONS
{
    . = KERNEL_BASE;
    __kernel_start = .;

    __text_start = .;
    .text : {
        KEEP(*(.text.boot))     /* boot.S must come first: it holds _start */
        *(.text .text.*)
    } :text
    __text_end = .;

    . = ALIGN(4096);
    __rodata_start = .;
    .rodata : {
        *(.rodata .rodata.*)
    } :rodata
    __rodata_end = .;

    /* Everything from here to __kernel_end is read-write, never executable. */
    . = ALIGN(4096);
    __data_start = .;
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
