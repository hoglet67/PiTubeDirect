#ifndef _VDU23_H
#define _VDU23_H

#include <inttypes.h>

#include "screen_modes.h"
#include "fonts.h"
#include "vdu_state.h"

vdu_state_t do_vdu23(screen_mode_t *screen, font_t **fontp, uint8_t c);

#endif
