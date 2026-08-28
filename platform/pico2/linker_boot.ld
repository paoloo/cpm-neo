/*
 * CP/M Neo — pico2 bootloader linker script (RP2350 flash boot)
 *
 * Flash layout (XIP window):
 *
 *   0x10000000  picobin EXE1 image definition   (platform/pico2/boot_extra.S)
 *   0x10000100  bootloader code                 (arch/riscv32/boot.S + pico2 BIOS)
 *   0x10004000  CP/M disk image                 (PICO2_DISK_OFFSET)
 *
 * The RP2350 bootrom validates the image definition, sets up XIP itself and
 * jumps to the entry point — no second-stage bootloader is needed.
 *
 * Boot-time scratch (512-byte sector-0 buffer + 128-byte stack) sits in the
 * empty TPA area of SRAM; the TPA is only populated once the kernel starts
 * exec'ing programs, long after boot has finished.  The kernel itself is
 * loaded at the physical SRAM address stored in S0_KERN_LOAD (the kernel is
 * linked with __ram_base=0x20000000, so S0 already holds physical addresses).
 */

ENTRY(_boot_start)

MEMORY {
    FLASH (rx) : ORIGIN = 0x10000000, LENGTH = 16K
}

SECTIONS {
    .image_def 0x10000000 : {
        KEEP(*(.image_def))
    } > FLASH

    .boot 0x10000100 : {
        *(.boot)
        *(.text .text.*)
        *(.rodata .rodata.*)
        *(.data .data.*)
        . = ALIGN(4);
        _boot_end = .;
    } > FLASH

    /DISCARD/ : { *(.comment) *(.eh_frame) *(.note*) }
}

/* Boot RAM: TPA is empty while booting, so park the sector-0 scratch buffer
 * and the boot stack just above the TPA load address (0x20000100). */
__boot_scratch   = 0x20000000 + 0x1000;       /* 512-byte S0 buffer */
__boot_stack_top = 0x20000000 + 0x1000 + 0x280; /* 128-byte stack above it */

ASSERT(_boot_end <= 0x10004000,
       "ERROR: pico2 bootloader exceeds the 16 KB boot slot (0x10000100..0x10004000)")
