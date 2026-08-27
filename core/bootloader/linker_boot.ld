ENTRY(_boot_start)

__io_size     = 0x0100;
/* __mem_size supplied externally via --defsym=__mem_size=... (see Makefile) */
__io_base     = __mem_size - __io_size;

MEMORY {
    RAM (rx) : ORIGIN = 0x0000, LENGTH = 1024
}

SECTIONS {
    .boot 0x0000 : {
        *(.boot)
        *(.text .text.*)
        . = ALIGN(4);
        _boot_end = .;
    } > RAM

    /DISCARD/ : { *(.comment) *(.eh_frame) *(.note*) }
}

/* Volatile boot RAM packed right after the boot code, as low as possible:
 * a 512-byte sector-0 scratch buffer, then a 128-byte stack just above it.
 * Measured peak boot stack use is 8 bytes, so 128 B leaves a wide margin.
 * Symbolically derived from the code end so the layout hugs the real image
 * size and can never overlap the code.  Both are only used while booting;
 * the region is reused by the TPA once the kernel runs. */
__boot_scratch    = ALIGN(_boot_end, 0x200); /* 512-aligned, 512 B sector-0 buffer */
__boot_stack_top  = __boot_scratch + 0x280;  /* 128 B stack above the scratch      */

/* The volatile boot RAM is placed by symbol arithmetic, not by a section, so
 * the linker would never complain if it ran past the end of RAM.  Enforce the
 * invariant here: it must stay below the MMIO area at the top of memory. */
ASSERT(__boot_stack_top <= __io_base,
       "ERROR: boot scratch/stack reach IO_AREA — image too large for this memory size")
