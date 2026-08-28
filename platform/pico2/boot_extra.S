/*
 * platform/pico2/boot_extra.S
 * CP/M Neo — RP2350 picobin EXE1 image definition for the pico2 bootloader.
 *
 * The RP2350 bootrom requires an image definition block at the start of
 * flash.  This one declares a non-secure RISC-V executable for RP2350 with
 * an explicit entry point (the bootrom's RISC-V default entry is the start
 * of the image, but our code begins one 256-byte slot later).
 *
 * Layout follows the SDK's crt0 embedded_start_block.inc.S:
 *
 *   .word  BLOCK_MARKER_START (0xffffded3)
 *   .byte  ITEM_IMAGE_TYPE(0x42), size 1 word
 *   .hword EXE(0x1) | CPU_RISCV(0x100) | CHIP_RP2350(0x1000)
 *   .byte  ITEM_ENTRY_POINT(0x44), size 3 words, pad, pad
 *   .word  entry point address
 *   .word  initial stack pointer (SRAM_END)
 *   .byte  ITEM_LAST(0xff), size (block_words - 4)
 *   .hword ...
 *   .byte  0
 *   .word  0                  — single-block loop (offset back to self)
 *   .word  BLOCK_MARKER_END (0xab123579)
 */

.section .image_def, "a"
.align 2
.global pico2_image_def

pico2_image_def:
    .word 0xffffded3                 /* PICOBIN_BLOCK_MARKER_START */

    .byte 0x42                       /* PICOBIN_BLOCK_ITEM_1BS_IMAGE_TYPE */
    .byte 0x1                        /* 1 word */
    .hword 0x1101                    /* EXE | EXE_CPU_RISCV | EXE_CHIP_RP2350 */

    .byte 0x44                       /* PICOBIN_BLOCK_ITEM_1BS_ENTRY_POINT */
    .byte 0x3                        /* 3 words */
    .byte 0                          /* pad */
    .byte 0                          /* pad */
    .word _boot_start                /* entry point  */
    .word 0x20082000                 /* initial sp = SRAM_END */

    .byte 0xff                       /* PICOBIN_BLOCK_ITEM_2BS_LAST */
    .hword (pico2_image_def_end - pico2_image_def - 16) / 4
    .byte 0
    .word 0                          /* no further blocks */
    .word 0xab123579                 /* PICOBIN_BLOCK_MARKER_END */

pico2_image_def_end:
