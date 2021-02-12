#ifndef _FONTS_H
#define _FONTS_H

#include "screen_modes.h"

#define DEFAULT_FONT 0

#define MAX_FONT_HEIGHT 32

typedef struct font {
   // The raw font data itself
   char *name;
   uint8_t *data;
   int bytes_per_char;
   int num_chars;
   int offset;
   int width;
   int height;

   // These control the way the font is rendered
   int spacing_w;
   int spacing_h;
   int scale_w;
   int scale_h;
   int rounding;

   // The working copy of the font data
   uint16_t *buffer;

   void  (*set_spacing_w)(struct font *font, int spacing_w);
   void  (*set_spacing_h)(struct font *font, int spacing_h);
   void    (*set_scale_w)(struct font *font, int scale_w);
   void    (*set_scale_h)(struct font *font, int scale_h);
   void   (*set_rounding)(struct font *font, int rounding);

   char *     (*get_name)(struct font *font);
   int   (*get_spacing_w)(struct font *font);
   int   (*get_spacing_h)(struct font *font);
   int     (*get_scale_w)(struct font *font);
   int     (*get_scale_h)(struct font *font);
   int    (*get_rounding)(struct font *font);
   int   (*get_overall_w)(struct font *font);
   int   (*get_overall_h)(struct font *font);

   void     (*write_char)(struct font *font, screen_mode_t *screen, int c, int x, int y, pixel_t fg_col, pixel_t bg_col);
   int      ( *read_char)(struct font *font, screen_mode_t *screen, int x, int y);

} font_t;

font_t *get_font_by_number(unsigned int num);

font_t *get_font_by_name(char *name);

void define_character(font_t *font, uint8_t c, uint8_t *data);

#endif
