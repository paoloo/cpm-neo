/*
 * platform/pico2/platform_boot.h
 * CP/M Neo — pico2 memory map constants shared by the BIOS and docs.
 *
 * CP/M address space (kernel, TPA, apps) lives in RP2350 SRAM starting at
 * SRAM_BASE; the disk image lives in the 4 MB flash after the boot slot.
 */

#ifndef PICO2_PLATFORM_BOOT_H
#define PICO2_PLATFORM_BOOT_H

#include <stdint.h>

#define PICO2_SRAM_BASE      0x20000000u /* CP/M RAM base (__ram_base)  */
#define PICO2_TPA_LOAD_ADDR  0x20000100u /* TPA base  (TPA_LOAD_ADDR)   */
#define PICO2_FLASH_BASE     0x10000000u /* XIP window                  */
#define PICO2_BOOT_SLOT_SIZE 0x00004000u /* 16 KB reserved for boot code */
#define PICO2_DISK_OFFSET    0x00004000u /* CP/M disk image start in flash */
#define PICO2_FLASH_SIZE     0x00400000u /* 4 MB (Pico 2)               */

#endif /* PICO2_PLATFORM_BOOT_H */
