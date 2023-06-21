#ifndef _FONTS_H
#define _FONTS_H

#include "screen_modes.h"

#define DEFAULT_FONT 0

#define MAX_FONT_HEIGHT 32

typedef struct font_cat {
   // The raw font data itself
   const char *name;   // (max) 8 character ASCII name of the font
   const uint8_t *data;      // pointer to the raw font data
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
} font_catalog_t;

typedef struct font {
   // The raw font data itself
   const char *name;   // (max) 8 character ASCII name of the font
   const uint8_t *data;      // pointer to the raw font data
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
   uint32_t number;

   // The working copy of the font data
   uint16_t *buffer;

   void  (*set_spacing_w)(struct font *font, int spacing_w);
   void  (*set_spacing_h)(struct font *font, int spacing_h);
   void    (*set_scale_w)(struct font *font, int scale_w);
   void    (*set_scale_h)(struct font *font, int scale_h);
   void   (*set_rounding)(struct font *font, int rounding);

   const char * (*get_name)(struct font *font);
   uint32_t  (*get_number)(struct font *font);
   int    (*get_spacing_w)(struct font *font);
   int    (*get_spacing_h)(struct font *font);
   int      (*get_scale_w)(struct font *font);
   int      (*get_scale_h)(struct font *font);
   int     (*get_rounding)(struct font *font);
   int    (*get_overall_w)(struct font *font);
   int    (*get_overall_h)(struct font *font);

   void     (*write_char)(struct font *font, screen_mode_t *screen, int c, int x, int y, pixel_t fg_col, pixel_t bg_col);
   int      ( *read_char)(struct font *font, screen_mode_t *screen, int x, int y,                        pixel_t bg_col);

} font_t;

const char * get_font_name(uint32_t num);

void initialize_font_by_number(uint32_t num, font_t *font);

void initialize_font_by_name(const char *name, font_t *font);

void define_character(font_t *font, uint8_t c, const uint8_t *data);

#endif
