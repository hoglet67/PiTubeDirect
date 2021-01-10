#ifndef _MOUSEPOINTERS_H
#define _MOUSEPOINTERS_H

#include "screen_modes.h"

extern uint8_t mpblack[];
extern uint8_t mpwhite[];
extern uint16_t mp_save_x;
extern uint16_t mp_save_y;

void mp_save(screen_mode_t *screen);
void mp_restore(screen_mode_t *screen);

#endif
