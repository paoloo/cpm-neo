/*
 * sysgen/bios.c
 * FreeCP/M SYSGEN — Host BIOS adapter
 */

#include "bios.h"
#include "kernel_abi.h"
#include "sysgen.h"

#include <stdio.h>
#include <string.h>

static uint8_t *g_disk;
static uint32_t g_disk_size;

int bios_init(void)
{
    return 0;
}

void bios_conout(int c)
{
    fputc(c, stderr);
}

int bios_conin(void)
{
    return 0;
}

int bios_const(void)
{
    return 0;
}

int bios_read(uint16_t lba, uint8_t *buf)
{
    uint32_t off = (uint32_t)lba * DISK_SECTOR_SIZE;
    if (off + DISK_SECTOR_SIZE > g_disk_size)
        return -1;
    memcpy(buf, g_disk + off, DISK_SECTOR_SIZE);
    return 0;
}

int bios_write(uint16_t lba, const uint8_t *buf)
{
    uint32_t off = (uint32_t)lba * DISK_SECTOR_SIZE;
    if (off + DISK_SECTOR_SIZE > g_disk_size)
        return -1;
    memcpy(g_disk + off, buf, DISK_SECTOR_SIZE);
    return 0;
}

void bios_consize(uint8_t *cw, uint8_t *ch)
{
    (void)cw;
    (void)ch;
}

uint32_t bios_time(void)
{
    return 0;
}

void sysgen_set_disk(uint8_t *disk, uint32_t size)
{
    g_disk = disk;
    g_disk_size = size;
}

uint8_t *sysgen_disk(void)
{
    return g_disk;
}

uint32_t sysgen_disk_size(void)
{
    return g_disk_size;
}