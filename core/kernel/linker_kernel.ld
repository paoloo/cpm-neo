ENTRY(_entry)

/* __KERN_START, __io_base, __ram_top and __tpa_base are supplied via
 * --defsym= in the two-pass build.  They are derived by build_disk.sh from
 * the platform config: __io_base is the MMIO window base, __ram_top is the
 * top of usable RAM (min(RAM_BASE + RAM_SIZE, IO_BASE)) and __tpa_base is the
 * CP/M TPA load address (RAM_BASE + 0x100). */

MEMORY {
    KRAM    (rwx) : ORIGIN = __KERN_START, LENGTH = __ram_top - __KERN_START
}

__kernel_base = ORIGIN(KRAM);

SECTIONS {
    .text : {
        . = ALIGN(4);
        _text_start = .;
        *(.text.entry)
        *(.text .text.*)
        *(.rodata .rodata.*)
        *(.srodata .srodata.*)
        . = ALIGN(4);
        _text_end = .;
    } > KRAM

    .data : {
        . = ALIGN(4);
        _data_start = .;

        *(.sdata .sdata.*)
        *(.data .data.*)
        . = ALIGN(4);
        _data_end = .;
    } > KRAM

    .bss (NOLOAD) : {
        . = ALIGN(4);
        _bss_start = .;
        *(.sbss .sbss.*)
        *(.bss .bss.*)
        *(COMMON)
        . = ALIGN(4);
        _bss_end = .;
    } > KRAM

    /* End of the kernel image (aligned). */
    _kend = ALIGN(16);

    __kernel_total = ABSOLUTE(_kend) - ORIGIN(KRAM);

    /* The kernel, the CCP, and user programs share a single stack at the
     * top of the TPA (the top slice of the TPA region, at __kernel_base).
     * There is no dedicated kernel stack: syscalls are plain jump-table
     * calls that run on the caller's stack, and there are no interrupt or
     * trap handlers, so a private kernel stack is only ever needed during
     * the initial boot — which runs on this same stack (growing down into
     * the then-empty TPA).  These symbols are owned here, in the kernel
     * link, and flow to the CCP/apps via --just-symbols. */
    __stack_size = 0x1000;
    __stack_top  = ORIGIN(KRAM);
    _stack_top   = __stack_top;

    ASSERT(_kend <= __ram_top, "ERROR: kernel image reaches top of usable RAM")
    /DISCARD/ : { *(.comment) *(.eh_frame) *(.note*) }
}
