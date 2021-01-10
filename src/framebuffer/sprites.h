#ifndef _SPRITES_H
#define _SPRITES_H

#include "screen_modes.h"

extern uint8_t sprites[];

void sp_save(screen_mode_t *screen, int sprite, int sx, int sy);
void sp_restore(screen_mode_t *screen, int sprite, int sx, int sy);
void sp_set(screen_mode_t *screen, int sprite, int sx, int sy);

#endif
