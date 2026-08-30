/*
 * platform/pico2/bios.c
 * CP/M Neo — Raspberry Pi Pico 2 (RP2350, Hazard3 RISC-V) BIOS
 *
 * Console : UART0 on GPIO0 (TX) / GPIO1 (RX) at 115200 8N1.
 *           '\n' is expanded to CR+LF for the terminal.
 * Disk    : the CP/M disk image lives in the Pico's flash at
 *           PICO2_DISK_OFFSET (right after the 16 KB boot slot).  Reads
 *           go through the XIP window; writes do a read-modify-write of
 *           the containing 4 KB flash sector via the bootrom's flash
 *           routines, so the disk is persistent across power cycles.
 * Time    : TIMER0 running from a 1 MHz tick (milliseconds).
 *
 * Everything runs on a single core with interrupts disabled; the bootrom
 * flash routines therefore need no extra locking.  Pure physical I/O, no
 * volume awareness — translation happens in kernel/disk.c.
 */

#include "bios.h"
#include "kernel_abi.h"
#include "rp2350.h"
#include "platform_boot.h"
#include <stddef.h>

/* ── Bootrom function access ──────────────────────────────────────────── */

typedef void  *(*rom_table_lookup_fn)(uint32_t code, uint32_t mask);
typedef void   (*rom_void_fn)(void);
typedef void   (*rom_flash_erase_fn)(uint32_t offs, uint32_t count,
                                     uint32_t block_size, uint8_t block_cmd);
typedef void   (*rom_flash_program_fn)(uint32_t offs, const uint8_t *data,
                                       uint32_t count);

static void *rom_func(uint16_t code)
{
    rom_table_lookup_fn lookup =
        (rom_table_lookup_fn)(uintptr_t)REG16(ROM_BASE + ROM_TABLE_LOOKUP_ENTRY_OFFSET);
    return lookup(code, RT_FLAG_FUNC_RISCV);
}

/* ── XIP restore stub ───────────────────────────────────────────────────
 * After a normal flash boot the bootrom leaves an XIP setup stub in
 * BOOTRAM.  Copy it once (while XIP is still active) and call it after
 * flash programming to return to accelerated XIP — the same mechanism the
 * Pico SDK's flash driver uses. */

static uint32_t xip_stub[64];
static int      xip_stub_valid;

static void xip_stub_init(void)
{
    if (!xip_stub_valid)
    {
        for (uint32_t i = 0; i < 64u; i++)
            xip_stub[i] = REG32(BOOTRAM_BASE + 4u * i);
        xip_stub_valid = 1;
    }
}

static void xip_stub_enter(void)
{
    /* +1 selects the stub's entry offset, exactly as the SDK does.  On
     * RISC-V the low bit is ignored by JALR, on ARM it is the Thumb bit. */
    ((rom_void_fn)(uintptr_t)((uintptr_t)xip_stub + 1u))();
}

/* ── Hardware init ────────────────────────────────────────────────────── */

static void barrier(void)
{
    __asm__ volatile ("fence iorw, iorw" ::: "memory");
}

static void unreset_wait(uint32_t bits)
{
    REG32(RESETS_BASE + RESETS_RESET_OFFSET) &= ~bits;
    while ((REG32(RESETS_BASE + RESETS_DONE_OFFSET) & bits) != bits)
        ;
}

/* 12 MHz XOSC → PLL_SYS 125 MHz → clk_sys and clk_peri (UART clock). */
static void clock_init(void)
{
    REG32(XOSC_BASE + XOSC_CTRL_OFFSET) = XOSC_CTRL_ENABLE_VALUE_ENABLE;
    while (!(REG32(XOSC_BASE + XOSC_STATUS_OFFSET) & XOSC_STATUS_STABLE_BITS))
        ;

    /* 12 MHz / 1 * 125 = 1500 MHz VCO, / 6 / 2 = 125 MHz */
    REG32(PLL_SYS_BASE + PLL_PWR_OFFSET) = PLL_PWR_PD_BITS | PLL_PWR_VCOPD_BITS;
    REG32(PLL_SYS_BASE + PLL_CS_OFFSET) = 1u; /* REFDIV = 1 */
    REG32(PLL_SYS_BASE + PLL_FBDIV_INT_OFFSET) = 125u;
    REG32(PLL_SYS_BASE + PLL_PRIM_OFFSET) = (6u << PLL_PRIM_POSTDIV1_LSB)
                                          | (2u << PLL_PRIM_POSTDIV2_LSB);
    REG32(PLL_SYS_BASE + PLL_PWR_OFFSET) = 0u;
    while (!(REG32(PLL_SYS_BASE + PLL_CS_OFFSET) & PLL_CS_LOCK_BITS))
        ;

    REG32(CLOCKS_BASE + CLK_PERI_CTRL_OFFSET) =
        CLK_PERI_CTRL_ENABLE_BITS
        | (CLK_PERI_CTRL_AUXSRC_VALUE_CLKSRC_PLL_SYS << CLK_PERI_CTRL_AUXSRC_LSB);

    REG32(CLOCKS_BASE + CLK_SYS_CTRL_OFFSET) =
        (CLK_SYS_CTRL_AUXSRC_VALUE_CLKSRC_PLL_SYS << CLK_SYS_CTRL_AUXSRC_LSB)
        | CLK_SYS_CTRL_SRC_VALUE_CLK_REF;
    REG32(CLOCKS_BASE + CLK_SYS_CTRL_OFFSET) =
        (CLK_SYS_CTRL_AUXSRC_VALUE_CLKSRC_PLL_SYS << CLK_SYS_CTRL_AUXSRC_LSB)
        | CLK_SYS_CTRL_SRC_VALUE_CLKSRC_CLK_SYS_AUX;
    while (REG32(CLOCKS_BASE + CLK_SYS_SELECTED_OFFSET)
           != CLK_SYS_CTRL_SRC_VALUE_CLKSRC_CLK_SYS_AUX)
        ;
}

static void uart_init(void)
{
    REG32(IO_BANK0_BASE + IO_BANK0_GPIO0_CTRL_OFFSET) = IO_BANK0_FUNCSEL_UART0;
    REG32(IO_BANK0_BASE + IO_BANK0_GPIO1_CTRL_OFFSET) = IO_BANK0_FUNCSEL_UART0;

    /* RX pad: reset state is input-disabled with pull-down (line would idle
     * low = break).  Enable the input with a pull-up so an unconnected RX
     * idles high. */
    REG32(PADS_BANK0_BASE + PADS_BANK0_GPIO1_OFFSET) =
        PADS_BANK0_IE_BITS | PADS_BANK0_PUE_BITS;

    REG32(UART0_BASE + UART_UARTCR_OFFSET) = 0u;      /* disable while configuring */
    REG32(UART0_BASE + UART_UARTICR_OFFSET) = 0x7ffu; /* clear pending interrupts  */

    /* 125 MHz / (16 * 115200) = 67.8168 → IBRD 67, FBRD 52 */
    REG32(UART0_BASE + UART_UARTIBRD_OFFSET) = 67u;
    REG32(UART0_BASE + UART_UARTFBRD_OFFSET) = 52u;
    REG32(UART0_BASE + UART_UARTLCR_H_OFFSET) = UART_UARTLCR_H_FEN_BITS
                                              | UART_UARTLCR_H_WLEN_BITS;
    REG32(UART0_BASE + UART_UARTCR_OFFSET) = UART_UARTCR_UARTEN_BITS
                                           | UART_UARTCR_TXE_BITS
                                           | UART_UARTCR_RXE_BITS;
}

/* TIMER0 tick: clk_sys 125 MHz / 125 = 1 MHz → TIMER0 counts microseconds. */
static void tick_init(void)
{
    REG32(TICKS_BASE + TICKS_TIMER0_CTRL_OFFSET) = 0u;
    REG32(TICKS_BASE + TICKS_TIMER0_CYCLES_OFFSET) = 124u;
    REG32(TICKS_BASE + TICKS_TIMER0_CTRL_OFFSET) = TICKS_CTRL_ENABLE_BITS;
}

int bios_init(void)
{
    unreset_wait(RESET_PLL_SYS_BITS | RESET_UART0_BITS | RESET_TIMER0_BITS
                 | RESET_IO_BANK0_BITS | RESET_PADS_BANK0_BITS);
    clock_init();
    uart_init();
    tick_init();
    return 0;
}

/* ── Console ──────────────────────────────────────────────────────────── */

static void uart_putc_raw(int c)
{
    while (REG32(UART0_BASE + UART_UARTFR_OFFSET) & UART_UARTFR_TXFF_BITS)
        ;
    REG32(UART0_BASE + UART_UARTDR_OFFSET) = (uint32_t)(c & 0xffu);
}

void bios_conout(int c)
{
    if (c == '\n')
        uart_putc_raw('\r');
    uart_putc_raw(c);
}

int bios_constat(void)
{
    return (REG32(UART0_BASE + UART_UARTFR_OFFSET) & UART_UARTFR_RXFE_BITS) ? 0 : 0xFF;
}

int bios_conin(void)
{
    while (REG32(UART0_BASE + UART_UARTFR_OFFSET) & UART_UARTFR_RXFE_BITS)
        ;
    return (int)(REG32(UART0_BASE + UART_UARTDR_OFFSET) & 0xffu);
}

void bios_consize(uint8_t *cw, uint8_t *ch)
{
    *cw = 80;
    *ch = 24;
}

/* ── Time ─────────────────────────────────────────────────────────────── */

uint32_t bios_time(void)
{
    /* TIMELR latches TIMEHR, giving a glitch-free 64-bit microsecond read. */
    uint32_t lo = REG32(TIMER0_BASE + TIMER_TIMELR_OFFSET);
    uint32_t hi = REG32(TIMER0_BASE + TIMER_TIMEHR_OFFSET);
    uint64_t us = ((uint64_t)hi << 32) | (uint64_t)lo;
    return (uint32_t)(us / 1000u); /* milliseconds */
}

/* ── Disk (flash-backed) ──────────────────────────────────────────────── */

int bios_read(uint16_t lba, uint8_t *buf)
{
    if (buf == NULL)
        return -1;

    const uint8_t *src = (const uint8_t *)(uintptr_t)
        (PICO2_FLASH_BASE + PICO2_DISK_OFFSET + (uint32_t)lba * DISK_SECTOR_SIZE);

    for (uint32_t i = 0; i < DISK_SECTOR_SIZE; i++)
        buf[i] = src[i];

    return 0;
}

/* Scratch buffer for the read-modify-write of one 4 KB flash sector.
 * Only referenced by bios_write, so it is garbage-collected out of the
 * bootloader build (which never writes). */
static uint8_t sector_buf[FLASH_SECTOR_SIZE];

int bios_write(uint16_t lba, const uint8_t *buf)
{
    if (buf == NULL)
        return -1;

    uint32_t off = PICO2_DISK_OFFSET + (uint32_t)lba * DISK_SECTOR_SIZE;
    uint32_t sec = off & ~(FLASH_SECTOR_SIZE - 1u);

    /* Preserve the surrounding 4 KB sector, patch in the new 512 B sector. */
    const uint8_t *src = (const uint8_t *)(uintptr_t)(PICO2_FLASH_BASE + sec);
    for (uint32_t i = 0; i < FLASH_SECTOR_SIZE; i++)
        sector_buf[i] = src[i];
    for (uint32_t i = 0; i < DISK_SECTOR_SIZE; i++)
        sector_buf[(off - sec) + i] = buf[i];

    xip_stub_init();
    barrier();

    /* All of the following execute from ROM/SRAM; XIP may be down. */
    ((rom_void_fn)rom_func(ROM_FUNC_CONNECT_INTERNAL_FLASH))();
    ((rom_void_fn)rom_func(ROM_FUNC_FLASH_EXIT_XIP))();
    ((rom_flash_erase_fn)rom_func(ROM_FUNC_FLASH_RANGE_ERASE))
        (sec, FLASH_SECTOR_SIZE, FLASH_SECTOR_SIZE, FLASH_SECTOR_ERASE_CMD);
    ((rom_flash_program_fn)rom_func(ROM_FUNC_FLASH_RANGE_PROGRAM))
        (sec, sector_buf, FLASH_SECTOR_SIZE);
    ((rom_void_fn)rom_func(ROM_FUNC_FLASH_FLUSH_CACHE))();

    xip_stub_enter();
    barrier();

    return 0;
}
