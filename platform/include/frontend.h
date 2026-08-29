#ifndef FRONTEND_H
#define FRONTEND_H

#include <stdint.h>

/* Optional BIOS-internal console abstraction. */

typedef struct
{
    int (*init)(void);
    void (*conout)(int c);
    int (*conin)(void);
    int (*constat)(void);
    void (*consize)(uint8_t *cw, uint8_t *ch);
} Frontend;

#endif /* FRONTEND_H */