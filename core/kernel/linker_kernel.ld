ENTRY(_entry)

/* __KERN_START and __mem_size supplied via --defsym= in the two-pass build.
 * __KERN_START is the physical load address of the kernel; for platforms
 * whose CP/M RAM is not mapped at physical 0, build_disk.sh passes
 * --defsym=__ram_base=<phys base> and computes __KERN_START from the
 * physical io_base.  Default __ram_base=0 preserves the flat model. */
__ram_base     = DEFINED(__ram_base) ? __ram_base : 0;
__io_size      = 0x0100;
__io_base      = __ram_base + __mem_size - __io_size;

MEMORY {
    KRAM    (rwx) : ORIGIN = __KERN_START, LENGTH = __io_base - __KERN_START
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

    /* The kernel and the CCP/apps share a single stack at the top of the
     * TPA (apps resolve _stack_top to __kernel_base via linker_sdk.ld).
     * There is no dedicated kernel stack: syscalls are plain jump-table
     * calls that run on the caller's stack, and there are no interrupt or
     * trap handlers, so a private kernel stack is only ever needed during
     * the initial boot — which runs on this same shared stack (growing
     * down into the then-empty TPA). */
    _stack_top = ORIGIN(KRAM);

    ASSERT(_kend <= __io_base, "ERROR: kernel image reaches IO_AREA")
    /DISCARD/ : { *(.comment) *(.eh_frame) *(.note*) }
}
