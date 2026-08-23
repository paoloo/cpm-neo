#ifndef KERNEL_H
#define KERNEL_H

#include <errno.h>
#include "bdos.h"
#include "bios.h"
#include "kernel_abi.h"
#include <stdio.h>
#include <stdint.h>

extern char __kernel_base[];
extern char __io_base[];

int      kernel_init(void);
void     kexec_ccp(void) __attribute__((noreturn));

#endif /* KERNEL_H */
