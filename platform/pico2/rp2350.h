/*
 * platform/pico2/rp2350.h
 * CP/M Neo — minimal RP2350 register definitions for the pico2 BIOS
 *
 * Only the handful of peripherals the BIOS touches are defined here, so the
 * platform builds with the plain bare-metal RISC-V toolchain (no Pico SDK).
 * Values match the RP2350 datasheet / pico-sdk hardware_regs headers.
 */

#ifndef PICO2_RP2350_H
#define PICO2_RP2350_H

#include <stdint.h>

#define REG32(addr) (*(volatile uint32_t *)(uintptr_t)(addr))
#define REG16(addr) (*(volatile uint16_t *)(uintptr_t)(addr))

/* Base addresses (hardware/regs/addressmap.h) */

#define ROM_BASE     0x00000000u
#define XIP_BASE     0x10000000u
#define SRAM_BASE    0x20000000u
#define SRAM_END     0x20082000u /* 520 KB total */
#define CLOCKS_BASE  0x40010000u
#define RESETS_BASE  0x40020000u
#define IO_BANK0_BASE 0x40028000u
#define XOSC_BASE    0x40048000u
#define PLL_SYS_BASE 0x40050000u
#define UART0_BASE   0x40070000u
#define PADS_BANK0_BASE 0x40038000u
#define TIMER0_BASE  0x400b0000u
#define BOOTRAM_BASE 0x400e0000u
#define TICKS_BASE   0x40108000u

/* Resets (hardware/regs/resets.h) */

#define RESETS_RESET_OFFSET 0x00u
#define RESETS_DONE_OFFSET  0x08u

#define RESET_UART0_BITS      0x04000000u
#define RESET_TIMER0_BITS     0x00800000u
#define RESET_PLL_SYS_BITS    0x00004000u
#define RESET_PADS_BANK0_BITS 0x00000200u
#define RESET_IO_BANK0_BITS   0x00000040u

/* XOSC (hardware/regs/xosc.h) */

#define XOSC_CTRL_OFFSET           0x00u
#define XOSC_STATUS_OFFSET         0x04u
#define XOSC_CTRL_ENABLE_VALUE_ENABLE 0xfabu
#define XOSC_STATUS_STABLE_BITS    0x80000000u

/* PLL (hardware/regs/pll.h) */

#define PLL_CS_OFFSET       0x00u
#define PLL_PWR_OFFSET      0x04u
#define PLL_FBDIV_INT_OFFSET 0x08u
#define PLL_PRIM_OFFSET     0x0cu

#define PLL_CS_LOCK_BITS         0x80000000u
#define PLL_PWR_PD_BITS          0x00000001u
#define PLL_PWR_POSTDIVPD_BITS   0x00000008u
#define PLL_PWR_VCOPD_BITS       0x00000020u
#define PLL_PRIM_POSTDIV1_LSB    16u
#define PLL_PRIM_POSTDIV2_LSB    20u

/* Clocks (hardware/regs/clocks.h) */

#define CLK_SYS_CTRL_OFFSET     0x3cu
#define CLK_SYS_SELECTED_OFFSET 0x44u
#define CLK_PERI_CTRL_OFFSET    0x48u

#define CLK_SYS_CTRL_SRC_LSB              0u
#define CLK_SYS_CTRL_SRC_VALUE_CLK_REF          0x0u
#define CLK_SYS_CTRL_SRC_VALUE_CLKSRC_CLK_SYS_AUX 0x1u
#define CLK_SYS_CTRL_AUXSRC_LSB           5u
#define CLK_SYS_CTRL_AUXSRC_VALUE_CLKSRC_PLL_SYS 0x0u

#define CLK_PERI_CTRL_ENABLE_BITS 0x00000800u
#define CLK_PERI_CTRL_AUXSRC_LSB  5u
#define CLK_PERI_CTRL_AUXSRC_VALUE_CLKSRC_PLL_SYS 0x1u

/* UART0 (hardware/regs/uart.h — PL011-style) */

#define UART_UARTDR_OFFSET   0x00u
#define UART_UARTFR_OFFSET   0x18u
#define UART_UARTIBRD_OFFSET 0x24u
#define UART_UARTFBRD_OFFSET 0x28u
#define UART_UARTLCR_H_OFFSET 0x2cu
#define UART_UARTCR_OFFSET   0x30u
#define UART_UARTICR_OFFSET  0x44u

#define UART_UARTFR_TXFF_BITS 0x00000020u
#define UART_UARTFR_RXFE_BITS 0x00000010u

#define UART_UARTLCR_H_FEN_BITS  0x00000010u
#define UART_UARTLCR_H_WLEN_BITS 0x00000060u /* 8 data bits */

#define UART_UARTCR_UARTEN_BITS 0x00000001u
#define UART_UARTCR_TXE_BITS    0x00000100u
#define UART_UARTCR_RXE_BITS    0x00000200u

/* IO bank 0 GPIO control (hardware/regs/io_bank0.h) */

#define IO_BANK0_GPIO0_CTRL_OFFSET 0x04u
#define IO_BANK0_GPIO1_CTRL_OFFSET 0x0cu
#define IO_BANK0_FUNCSEL_UART0     0x2u

/* Pad control (hardware/regs/pads_bank0.h) — one register per GPIO,
 * GPIO[n] at offset 0x04 + 4*n */

#define PADS_BANK0_GPIO1_OFFSET 0x08u
#define PADS_BANK0_IE_BITS  0x00000040u /* input enable   */
#define PADS_BANK0_PUE_BITS 0x00000008u /* pull-up        */
#define PADS_BANK0_PDE_BITS 0x00000004u /* pull-down      */

/* TIMER0 (hardware/regs/timer.h) — 64-bit free-running microsecond counter */

#define TIMER_TIMELR_OFFSET  0x0cu
#define TIMER_TIMEHR_OFFSET  0x08u

/* TICKS (hardware/regs/ticks.h) — 6 tick generators, 12 bytes each.
 * Slice index 2 drives TIMER0. */

#define TICKS_TIMER0_CTRL_OFFSET   (2u * 12u + 0u)
#define TICKS_TIMER0_CYCLES_OFFSET (2u * 12u + 4u)
#define TICKS_CTRL_ENABLE_BITS     0x00000001u

/* Bootrom function table (boot/bootrom_constants.h, RP2350 + __riscv):
 * the lookup helper is a ROM function whose 16-bit address sits at 0x7df8. */

#define ROM_TABLE_LOOKUP_ENTRY_OFFSET 0x7df8u
#define RT_FLAG_FUNC_RISCV            0x0001u

#define ROM_TABLE_CODE(c1, c2) ((uint16_t)((c1) | ((c2) << 8)))
#define ROM_FUNC_CONNECT_INTERNAL_FLASH ROM_TABLE_CODE('I', 'F')
#define ROM_FUNC_FLASH_EXIT_XIP         ROM_TABLE_CODE('E', 'X')
#define ROM_FUNC_FLASH_RANGE_ERASE      ROM_TABLE_CODE('R', 'E')
#define ROM_FUNC_FLASH_RANGE_PROGRAM    ROM_TABLE_CODE('R', 'P')
#define ROM_FUNC_FLASH_FLUSH_CACHE      ROM_TABLE_CODE('F', 'C')

/* Flash geometry (hardware/flash.h) */

#define FLASH_PAGE_SIZE   256u
#define FLASH_SECTOR_SIZE 4096u
#define FLASH_SECTOR_ERASE_CMD 0x20u

#endif /* PICO2_RP2350_H */
