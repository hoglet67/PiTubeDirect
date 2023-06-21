#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <string.h>

#include "fonts.h"

// These include define the actual font bitmaps
// (they should really be .c files)

#include "fonts/bbc.fnt.h"
#include "fonts/6847.fnt.h"
#include "fonts/8x10.fnt.h"
#include "fonts/8x11snsf.fnt.h"
#include "fonts/8x14.fnt.h"
#include "fonts/8x8.fnt.h"
#include "fonts/8x8ital.fnt.h"
#include "fonts/9x16snsf.fnt.h"
#include "fonts/bigserif.fnt.h"
#include "fonts/blcksnsf.fnt.h"
#include "fonts/block.fnt.h"
#include "fonts/bold.fnt.h"
#include "fonts/broadway.fnt.h"
#include "fonts/computer.fnt.h"
#include "fonts/courier.fnt.h"
#include "fonts/future.fnt.h"
#include "fonts/greek.fnt.h"
#include "fonts/hollow.fnt.h"
#include "fonts/italics.fnt.h"
#include "fonts/lcd.fnt.h"
#include "fonts/medieval.fnt.h"
#include "fonts/norway.fnt.h"
#include "fonts/sanserif.fnt.h"
#include "fonts/script.fnt.h"
#include "fonts/slant.fnt.h"
#include "fonts/small.fnt.h"
#include "fonts/standard.fnt.h"
#include "fonts/stretch.fnt.h"
#include "fonts/sub.fnt.h"
#include "fonts/super.fnt.h"
#include "fonts/thin8x8.fnt.h"
#include "fonts/thin.fnt.h"
#include "fonts/thnserif.fnt.h"

#include "fonts/saa5050.fnt.h"
#include "fonts/saa5051.fnt.h"
#include "fonts/saa5052.fnt.h"
#include "fonts/saa5053.fnt.h"
#include "fonts/saa5054.fnt.h"
#include "fonts/saa5055.fnt.h"
#include "fonts/saa5056.fnt.h"
#include "fonts/saa5057.fnt.h"

// ==========================================================================
// Font Definitions
// ==========================================================================

// This is used to size the character buffer in current_font
#define MAX_HEIGHT 14

static const font_catalog_t font_catalog[] = {
   //                                              scale_h
   //                                           scale_w  |
   //                                      spacing_h  |  |
   //                                    spacing_w |  |  |
   //                                   height  |  |  |  |
   //                                width   |  |  |  |  |
   //                             shift  |   |  |  |  |  |
   //                         offset  |  |   |  |  |  |  |
   //                   num_chars  |  |  |   |  |  |  |  |
   //         bytes_per_char    |  |  |  |   |  |  |  |  |
   // Name      data       |    |  |  |  |   |  |  |  |  |
   {"BBC",      fontbbc,   8, 256, 0, 0, 8,  8, 0, 0, 1, 1},
   {"8X10",     font01,   16, 256, 0, 0, 8, 10, 0, 0, 1, 1},
   {"8X11SNSF", font02,   16, 256, 0, 0, 8, 11, 0, 0, 1, 1},
   {"8X14",     font03,   16, 256, 0, 0, 8, 14, 0, 0, 1, 1},
   {"8X8",      font04,   16, 256, 0, 0, 8, 10, 0, 0, 1, 1},
   {"8X8ITAL",  font05,   16, 256, 0, 0, 8, 10, 0, 0, 1, 1},
   {"9X16",     font06,   16, 256, 0, 0, 8, 14, 1, 0, 1, 1},
   {"BIGSERIF", font07,   16, 256, 0, 0, 8, 14, 1, 0, 1, 1},
   {"BLCKSNSF", font08,   16, 256, 0, 0, 8, 10, 0, 0, 1, 1},
   {"BLOCK",    font09,   16, 256, 0, 0, 8, 14, 1, 0, 1, 1},
   {"BOLD",     font10,   16, 256, 0, 0, 8, 14, 1, 0, 1, 1},
   {"BROADWAY", font11,   16, 256, 0, 0, 8, 14, 1, 0, 1, 1},
   {"COMPUTER", font12,   16, 256, 0, 0, 8, 14, 1, 0, 1, 1},
   {"COURIER",  font13,   16, 256, 0, 0, 8, 14, 1, 0, 1, 1},
   {"FUTURE",   font14,   16, 256, 0, 0, 8, 14, 1, 0, 1, 1},
   {"GREEK",    font15,   16, 256, 0, 0, 8, 14, 1, 0, 1, 1},
   {"HOLLOW",   font16,   16, 256, 0, 0, 8, 14, 1, 0, 1, 1},
   {"ITALICS",  font17,   16, 256, 0, 0, 8, 14, 1, 0, 1, 1},
   {"LCD",      font18,   16, 256, 0, 0, 8, 14, 1, 0, 1, 1},
   {"MEDIEVAL", font19,   16, 256, 0, 0, 8, 14, 1, 0, 1, 1},
   {"NORWAY",   font20,   16, 256, 0, 0, 8, 14, 1, 0, 1, 1},
   {"SANSERIF", font21,   16, 256, 0, 0, 8, 14, 1, 0, 1, 1},
   {"SCRIPT",   font22,   16, 256, 0, 0, 8, 14, 1, 0, 1, 1},
   {"SLANT",    font23,   16, 256, 0, 0, 8, 14, 0, 0, 1, 1},
   {"SMALL",    font24,   16, 256, 1, 2, 6,  8, 0, 0, 1, 1},
   {"STANDARD", font25,   16, 256, 0, 0, 8, 14, 1, 0, 1, 1},
   {"STRETCH",  font26,   16, 256, 0, 0, 8, 14, 1, 0, 1, 1},
   {"SUB",      font27,   16, 256, 0, 0, 8, 14, 0, 0, 1, 1},
   {"SUPER",    font28,   16, 256, 0, 0, 8, 14, 0, 0, 1, 1},
   {"THIN",     font29,   16, 256, 0, 0, 8, 14, 1, 0, 1, 1},
   {"THIN8X8",  font30,   16, 256, 0, 0, 8,  8, 0, 0, 1, 1},
   {"THNSERIF", font31,   16, 256, 0, 0, 8, 14, 0, 0, 1, 1},
   {"SAA5050",  saa5050,  10, 256, 0, 0, 6, 10, 0, 0, 1, 1},
   {"SAA5051",  saa5051,  10, 256, 0, 0, 6, 10, 0, 0, 1, 1},
   {"SAA5052",  saa5052,  10, 256, 0, 0, 6, 10, 0, 0, 1, 1},
   {"SAA5053",  saa5053,  10, 256, 0, 0, 6, 10, 0, 0, 1, 1},
   {"SAA5054",  saa5054,  10, 256, 0, 0, 6, 10, 0, 0, 1, 1},
   {"SAA5055",  saa5055,  10, 256, 0, 0, 6, 10, 0, 0, 1, 1},
   {"SAA5056",  saa5056,  10, 256, 0, 0, 6, 10, 0, 0, 1, 1},
   {"SAA5057",  saa5057,  10, 256, 0, 0, 6, 10, 0, 0, 1, 1},
   {"6847",     font6847, 12, 128, 0, 0, 8, 12, 0, 0, 1, 1}
};

#define NUM_FONTS (sizeof(font_catalog) / sizeof(font_catalog_t))

#define MAX_CHARACTERS 256u

// ==========================================================================
// Static Methods
// ==========================================================================

// a is the 12 pixel row we wish to apply rounding to
// b is the 12 pixel row we are using as a reference (either one above or one below)
static inline uint16_t combine_rows(uint16_t a, uint16_t b) {
    return (uint16_t)(a | ((a >> 1) & b & ~(b >> 1)) | ((a << 1) & b & ~(b << 1)));
}

static void copy_font_character(const font_t *font, const  uint8_t *src, int c, int is_graphics) {
   if (c > font->num_chars) {
      return;
   }
   uint16_t *dst = font->buffer + c * (font->height << font->rounding);
   // Skip any padding bytes
   src += font->offset;
   if (font->rounding) {
      // Stage 1: expand each pixel to 2x2 pixels
      // Copy the defined part of the font
      for (int i = 0; i < font->height; i++) {
         uint8_t data = (*src++) >> font->shift;
         uint16_t expanded = 0;
         for (int j = 0; j < 8; j++) {
            expanded <<= 2;
            if (data & 0x80) {
               expanded |= 3;
            }
            data <<= 1;
         }
         // Insert two copies of the expanded row
         *dst++ = expanded;
         *dst++ = expanded;
      }
      // Stage 2: perform rounding
      if (!is_graphics) {
         dst -= font->height * 2;
         for (int i = 0; i < font->height - 1; i++) {
            uint16_t o_row = dst[i * 2 + 1];
            uint16_t e_row = dst[i * 2 + 2];
            dst[i * 2 + 1] = combine_rows(o_row, e_row);
            dst[i * 2 + 2] = combine_rows(e_row, o_row);
         }
      }
   } else {
      // Copy the defined part of the font
      for (int j = 0; j < font->height; j++) {
         *dst++ = (*src++) >> font->shift;
      }
   }
}

// ==========================================================================
// Default handlers
// ==========================================================================

static void default_set_spacing_w(font_t *font, int spacing_w) {
   font->spacing_w = spacing_w;
}

static void default_set_spacing_h(font_t *font, int spacing_h) {
   font->spacing_h = spacing_h;
}

static void default_set_scale_w(font_t *font, int scale_w) {
   font->scale_w = scale_w;
}

static void default_set_scale_h(font_t *font, int scale_h) {
   font->scale_h = scale_h;
}

static void default_set_rounding(font_t *font, int rounding) {
   font->rounding = rounding & 1;
   const uint8_t *src = font->data;
   // Special case the SAA fonts to avoid rounding the graphics
   int num = strncmp(font->name, "SAA505", 6) ? font->num_chars : 128;
   for (int c = 0; c < font->num_chars; c++) {
      // Copy the font into the local buffer, expanding as necessary if rounding is on
      copy_font_character(font, src, c, c >= num);
      // Skip any padding between characters
      src += font->bytes_per_char;
   }
}

static const char *default_get_name(font_t *font) {
   return font->name;
}

static uint32_t default_get_number(font_t *font) {
   return font->number;
}

static int default_get_spacing_w(font_t *font) {
   return font->spacing_w;
}

static int default_get_spacing_h(font_t *font) {
   return font->spacing_h;
}

static int default_get_scale_w(font_t *font) {
   return font->scale_w;
}

static int default_get_scale_h(font_t *font) {
   return font->scale_h;
}

static int default_get_rounding(font_t *font) {
   return font->rounding;
}

static int default_get_overall_w(font_t *font) {
   return ((font->width + font->spacing_w) << font->rounding) * font->scale_w;
}

static int default_get_overall_h(font_t *font) {
   return ((font->height + font->spacing_h) << font->rounding) * font->scale_h;
}

static void default_write_char(font_t *font, screen_mode_t *screen, int c, int x, int y, pixel_t fg_col, pixel_t bg_col) {
   int x_pos = x;
   int width  = font->width  << font->rounding;
   int height = font->height << font->rounding;
   int p      = c * height;
   int mask = 1 << (width - 1);
   for (int i = 0; i < height; i++) {
      int data = font->buffer[p++];
      for (int j = 0; j < width; j++) {
         pixel_t col = (data & mask) ? fg_col : bg_col;
         for (int sx = 0; sx < font->scale_w; sx++) {
            for (int sy = 0; sy < font->scale_h; sy++) {
               screen->set_pixel(screen, x + sx, y + sy, col);
            }
         }
         x += font->scale_w;
         data <<= 1;
      }
      x = x_pos;
      y -= font->scale_h;
   }
}

static int default_read_char(font_t *font, screen_mode_t *screen, int x, int y, pixel_t bg_col) {
   int screendata[MAX_FONT_HEIGHT];
   // Read the character from screen memory
   int *dp = screendata;
   int width  = font->width  << font->rounding;
   int height = font->height << font->rounding;
   int h = 0;
   for (int i = 0; i < height ; i +=1) {
      int row = 0;
      for (int j = 0; j < width * font->scale_w; j += font->scale_w) {
         row <<= 1;
         if (screen->get_pixel(screen, x + j, y - h) != bg_col) {
            row |= 1;
         }
      }
      h += font->scale_h;
      *dp++ = row;
   }
   // Match against font
   for (int c = 0x20; c < font->num_chars; c++) {
      for (y = 0; y < height; y++) {
         if (font->buffer[c * height + y] != screendata[y]) {
            break;
         }
      }
      if (y == height) {
         return c;
      }
   }
   return 0;
}

// ==========================================================================
// More Static Methods
// ==========================================================================

static void initialize_font(const font_catalog_t * catalog, uint32_t num, font_t *font) {

   // Copy the default metrics etc from the catalog
   memcpy(font, catalog, sizeof(font_catalog_t));

   // The factor of 2 allows for character rounding to be enabled
   size_t size = (size_t)((size_t)MAX_HEIGHT * MAX_CHARACTERS * 2 * sizeof(uint16_t));
   if (font->buffer == NULL) {
      font->buffer = (uint16_t *)calloc(size, 1);
   }

   // Default implementation of setters
   font->set_spacing_w = default_set_spacing_w;
   font->set_spacing_h = default_set_spacing_h;
   font->set_scale_w   = default_set_scale_w;
   font->set_scale_h   = default_set_scale_h;
   font->set_rounding  = default_set_rounding;

   // Default implementation of getters
   font->get_name      = default_get_name;
   font->get_number    = default_get_number;
   font->get_spacing_w = default_get_spacing_w;
   font->get_spacing_h = default_get_spacing_h;
   font->get_scale_w   = default_get_scale_w;
   font->get_scale_h   = default_get_scale_h;
   font->get_rounding  = default_get_rounding;
   font->get_overall_w = default_get_overall_w;
   font->get_overall_h = default_get_overall_h;

   // Default implementation of read/write
   font->read_char     = default_read_char;
   font->write_char    = default_write_char;

   // Copy the font into a local font buffer, so VDU 23 can update it
   // and so that character rounding can be applied if necessary
   font->set_rounding(font, 0);

   // Record the font number
   font->number = num;
}


// ==========================================================================
// Public Methods
// ==========================================================================

const char * get_font_name(uint32_t num) {
   const font_catalog_t *font = &font_catalog[DEFAULT_FONT];
   if (num < NUM_FONTS) {
      font = &font_catalog[num];
   }
   return font->name;
}

void initialize_font_by_number(uint32_t num, font_t *font) {
   const font_catalog_t *font_cat = &font_catalog[DEFAULT_FONT];
   if (num < NUM_FONTS) {
      font_cat = &font_catalog[num];
   }
   initialize_font(font_cat, num, font);
}

void initialize_font_by_name(const char *name, font_t *font) {
   uint32_t num = DEFAULT_FONT;
   for (uint32_t i = 0; i < NUM_FONTS; i++) {
      if (!strncasecmp(name, font_catalog[i].name, 8)) {
         num = i;
         break;
      }
   }
   const font_catalog_t *font_cat = font_catalog + num;
   initialize_font(font_cat, num, font);
}

void define_character(font_t *font, uint8_t c, const  uint8_t *data) {
   copy_font_character(font, data, c, 0);
}
