ENTRY(_entry)

/* Configuration Symbols
 * __stack_size / __stack_top are owned by the kernel link
 * (core/kernel/linker_kernel.ld) and come in here via --just-symbols. */

MEMORY {
    /* Length = available TPA RAM (from __tpa_base up to the kernel;
     * both supplied by kernel.elf via --just-symbols) */
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

    /* Reserve the stack at the TOP of TPA RAM so its address is
     * independent of this program's size and the exec-time load can
     * never reach it.  BSS/Data can never grow upward into it (ASSERT
     * below).  Symbols are absolute (top-level assignments), not tied
     * to any section's location counter.
     */
    _stack_bottom = __stack_top - __stack_size;
    _stack_top    = __stack_top;

    /* --- CRITICAL SAFETY CHECK --- */
    ASSERT(ABSOLUTE(_bss_end) <= _stack_bottom, "ERROR: TPA code/BSS reaches stack (Collision!)")

    /DISCARD/ : { *(.comment) *(.eh_frame) *(.note*) }
}
