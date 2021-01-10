#ifndef _FONTS_H
#define _FONTS_H

#include "screen_modes.h"

#define DEFAULT_FONT "6847"

#define MAX_FONT_HEIGHT 32

typedef struct font {
   // The font data itself
   char *name;
   uint8_t *data;
   int bytes_per_char;
   int num_chars;
   int offset;
   // These control the way the font is rendered
   int width;
   int height;
   int scale_w;
   int scale_h;
   int spacing;
   // Functions
   void  (*draw_character)(struct font *font, screen_mode_t *screen, int c, int x_pos, int y_pos, pixel_t fg_col, pixel_t bg_col);
   int   (*read_character)(struct font *font, screen_mode_t *screen,        int x_pos, int y_pos);
} font_t;


font_t *get_font(char *name);

#endif
