#ifndef TELETEXT_H
#define TELETEXT_H

#include "screen_modes.h"

// Teletext palette
typedef enum {
   TT_BLACK,
   TT_RED,
   TT_GREEN,
   TT_YELLOW,
   TT_BLUE,
   TT_MAGENTA,
   TT_CYAN,
   TT_WHITE
} tt_colour_t;

// Character rounding, TL is set top left etc
typedef enum {
   TT_BLOCK_NONE    = 0,
   TT_BLOCK_TL      = 1,
   TT_BLOCK_TR      = 2,
   TT_BLOCK_BL      = 4,
   TT_BLOCK_BR      = 8,
   TT_BLOCK_ALL     = 15
} tt_block_t;

// Teletext control codes
enum {
   TT_A_RED         = 129,
   TT_A_GREEN       = 130,
   TT_A_YELLOW      = 131,
   TT_A_BLUE        = 132,
   TT_A_MAGENTA     = 133,
   TT_A_CYAN        = 134,
   TT_A_WHITE       = 135,
   TT_FLASH         = 136,
   TT_STEADY        = 137,
   TT_NORMAL        = 140,
   TT_DOUBLE        = 141,
   TT_G_RED         = 145,
   TT_G_GREEN       = 146,
   TT_G_YELLOW      = 147,
   TT_G_BLUE        = 148,
   TT_G_MAGENTA     = 149,
   TT_G_CYAN        = 150,
   TT_G_WHITE       = 151,
   TT_CONCEAL       = 152,
   TT_CONTIGUOUS    = 153,
   TT_SEPARATED     = 154,
   TT_BLACK_BGD     = 156,
   TT_NEW_BGD       = 157,
   TT_HOLD          = 158,
   TT_RELEASE       = 159
};

//void tt_reset();
//void tt_initialise();
//void tt_flash();
//void tt_draw_character(int c, int col, int row);

screen_mode_t *tt_get_screen_mode();

#endif
