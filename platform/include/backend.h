#ifndef BACKEND_H
#define BACKEND_H

#include <stdint.h>

/* Optional BIOS-internal block-storage abstraction. */

typedef struct
{
    int (*init)(void);
    int (*read)(uint16_t lba, uint8_t *buf);
    int (*write)(uint16_t lba, const uint8_t *buf);
} Backend;

#endif /* BACKEND_H */