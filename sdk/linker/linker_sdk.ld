ENTRY(_entry)

/* Configuration Symbols */
/* __tpa_base: physical TPA origin.  Platforms with CP/M RAM off address 0
 * pass --defsym=__tpa_base=... (see platform/<name>/platform_flags.sh);
 * the default keeps the classic flat layout. */
__tpa_base       = DEFINED(__tpa_base) ? __tpa_base : 0x0100;
__tpa_stack_size = 0x1000;
__tpa_stack_top  = __kernel_base;

MEMORY {
    /* Length = available TPA RAM (from __tpa_base up to the kernel,
     * __kernel_base from kernel.elf via --just-symbols) */
    RAM (rwx) : ORIGIN = __tpa_base, LENGTH = __kernel_base - __tpa_base
}

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
    } > RAM

    .data : {
        . = ALIGN(4);
        _data_start = .;

        *(.sdata .sdata.*)
        *(.data .data.*)
        . = ALIGN(4);
        _data_end = .;
    } > RAM

    .bss (NOLOAD) : {
        . = ALIGN(4);
        _bss_start = .;
        *(.sbss .sbss.*)
        *(.bss .bss.*)
        *(COMMON)
        . = ALIGN(4);
        _bss_end = .;
    } > RAM

    /* Reserve the TPA stack at the TOP of TPA RAM so its address is
     * independent of this program's size and the exec-time load can
     * never reach it.  BSS/Data can never grow upward into it (ASSERT
     * below).  Symbols are absolute (top-level assignments), not tied
     * to any section's location counter.
     */
    _stack_bottom = __tpa_stack_top - __tpa_stack_size;
    _stack_top = __tpa_stack_top;

    /* --- CRITICAL SAFETY CHECK --- */
    ASSERT(ABSOLUTE(_bss_end) <= _stack_bottom, "ERROR: TPA code/BSS reaches TPA stack (Collision!)")

    /DISCARD/ : { *(.comment) *(.eh_frame) *(.note*) }
}
