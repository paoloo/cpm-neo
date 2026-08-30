ENTRY(_boot_start)

MEMORY {
    RAM (rwx) : ORIGIN = 0x0000, LENGTH = 1024
}

SECTIONS {
    .boot 0x0000 : {
        *(.boot)
        *(.text .text.*)
        . = ALIGN(4);
        _boot_end = .;
    } > RAM

    .sbss (NOLOAD) : {
        *(.sbss .sbss.*)
        *(.scommon)
        . = ALIGN(4);
        _boot_bss_end = .;
    } > RAM

    /DISCARD/ : {
        *(.comment)
        *(.eh_frame)
        *(.note*)
    }
}

__boot_scratch   = ALIGN(_boot_bss_end, 0x200);
__boot_stack_top = __boot_scratch + 0x280;

ASSERT(__boot_stack_top <= __ram_top,
       "ERROR: boot scratch/stack reach top of usable RAM — image too large for this memory size")