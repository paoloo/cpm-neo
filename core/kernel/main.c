/*
 * kernel/main.c
 * CP/M Neo — kernel C entry point
 */

#include "kernel.h"

static void print_TPA(void)
{
    uintptr_t size = (uintptr_t)__kernel_base - (uintptr_t)__tpa_base;
    uint32_t kb = (uint32_t)(size / 1024);

    char buf[16];
    char *ptr = &buf[15];

    *ptr = '\0';
    *(--ptr) = 'A';
    *(--ptr) = 'P';
    *(--ptr) = 'T';
    *(--ptr) = ' ';
    *(--ptr) = 'K';

    uint32_t v = kb;
    do
    {
        *(--ptr) = '0' + (v % 10);
        v /= 10;
    } while (v > 0);

    puts("");
    puts(ptr);
    puts("");
}

void os_entry(void)
{
    if (kernel_init() != EOK)
    {
        puts(" \nVOL ERR");
        while (1)
            ;
    }

    print_TPA();
    kexec_ccp();
}

void __attribute__((used, noinline)) _start(void)
{
    os_entry();
    for (;;)
        ;
}