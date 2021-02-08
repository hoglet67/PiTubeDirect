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
   TT_A_BLACK       = 0x00, // for colour conversion
   TT_A_RED         = 0x01,
   TT_A_GREEN       = 0x02,
   TT_A_YELLOW      = 0x03,
   TT_A_BLUE        = 0x04,
   TT_A_MAGENTA     = 0x05,
   TT_A_CYAN        = 0x06,
   TT_A_WHITE       = 0x07,
   TT_FLASH         = 0x08,
   TT_STEADY        = 0x09,
   TT_END_BOX       = 0x0A, // not implemented
   TT_START_BOX     = 0x0B, // not implemented
   TT_NORMAL        = 0x0C,
   TT_DOUBLE        = 0x0D,
   TT_S0            = 0x0E, // double width (later ETS spec)
   TT_S1            = 0x0F, // double size (later ETS spec)
   TT_G_BLACK       = 0x10, // for colour conversoon
   TT_G_RED         = 0x11,
   TT_G_GREEN       = 0x12,
   TT_G_YELLOW      = 0x13,
   TT_G_BLUE        = 0x14,
   TT_G_MAGENTA     = 0x15,
   TT_G_CYAN        = 0x16,
   TT_G_WHITE       = 0x17,
   TT_CONCEAL       = 0x18,
   TT_CONTIGUOUS    = 0x19,
   TT_SEPARATED     = 0x1A,
   TT_ESCAPE        = 0x1B, // switches character sets (later ETS spec)
   TT_BLACK_BGD     = 0x1C,
   TT_NEW_BGD       = 0x1D,
   TT_HOLD          = 0x1E,
   TT_RELEASE       = 0x1F,
   TT_SPACE         = 0x20
};

screen_mode_t *tt_get_screen_mode();

#endif
