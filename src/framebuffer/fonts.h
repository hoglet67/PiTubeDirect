#ifndef _FONTS_H
#define _FONTS_H

#include "screen_modes.h"

#define DEFAULT_FONT 0

#define MAX_FONT_HEIGHT 32

typedef struct font {
   // The raw font data itself
   char *name;         // (max) 8 character ASCII name of the font
   uint8_t *data;      // pointer to the raw font data
   int bytes_per_char; // Number of bytes of raw data per character
   int num_chars;      // Number of characters in the character set
   int offset;         // Offset (in bytes) to the first row of the character
   int shift;          // Offset (in bits) to the first column of the character
   int width;          // Width (in pixels) of the character
   int height;         // Height(in pixels) of the character

   // These control the way the font is rendered
   int spacing_w;
   int spacing_h;
   int scale_w;
   int scale_h;
   int rounding;

   // The font number
   int number;

   // The working copy of the font data
   uint16_t *buffer;

   void  (*set_spacing_w)(struct font *font, int spacing_w);
   void  (*set_spacing_h)(struct font *font, int spacing_h);
   void    (*set_scale_w)(struct font *font, int scale_w);
   void    (*set_scale_h)(struct font *font, int scale_h);
   void   (*set_rounding)(struct font *font, int rounding);

   char *     (*get_name)(struct font *font);
   int      (*get_number)(struct font *font);
   int   (*get_spacing_w)(struct font *font);
   int   (*get_spacing_h)(struct font *font);
   int     (*get_scale_w)(struct font *font);
   int     (*get_scale_h)(struct font *font);
   int    (*get_rounding)(struct font *font);
   int   (*get_overall_w)(struct font *font);
   int   (*get_overall_h)(struct font *font);

   void     (*write_char)(struct font *font, screen_mode_t *screen, int c, int x, int y, pixel_t fg_col, pixel_t bg_col);
   int      ( *read_char)(struct font *font, screen_mode_t *screen, int x, int y,                        pixel_t bg_col);

} font_t;

char * get_font_name(unsigned int num);

font_t *get_font_by_number(unsigned int num);

font_t *get_font_by_name(char *name);

void define_character(font_t *font, uint8_t c, uint8_t *data);

#endif
