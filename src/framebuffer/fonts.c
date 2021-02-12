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

static font_t font_catalog[] = {
   {"BBC",      fontbbc,   8, 256, 0, 8,  8, 0},
   {"8X10",     font01,   16, 256, 0, 8, 10, 0},
   {"8X11SNSF", font02,   16, 256, 0, 8, 11, 0},
   {"8X14",     font03,   16, 256, 0, 8, 14, 0},
   {"8X8",      font04,   16, 256, 0, 8, 10, 0},
   {"8X8ITAL",  font05,   16, 256, 0, 8, 10, 0},
   {"9X16",     font06,   16, 256, 0, 8, 14, 1},
   {"BIGSERIF", font07,   16, 256, 0, 8, 14, 1},
   {"BLCKSNSF", font08,   16, 256, 0, 8, 10, 0},
   {"BLOCK",    font09,   16, 256, 0, 8, 14, 1},
   {"BOLD",     font10,   16, 256, 0, 8, 14, 1},
   {"BROADWAY", font11,   16, 256, 0, 8, 14, 1},
   {"COMPUTER", font12,   16, 256, 0, 8, 14, 1},
   {"COURIER",  font13,   16, 256, 0, 8, 14, 1},
   {"FUTURE",   font14,   16, 256, 0, 8, 14, 1},
   {"GREEK",    font15,   16, 256, 0, 8, 14, 1},
   {"HOLLOW",   font16,   16, 256, 0, 8, 14, 1},
   {"ITALICS",  font17,   16, 256, 0, 8, 14, 1},
   {"LCD",      font18,   16, 256, 0, 8, 14, 1},
   {"MEDIEVAL", font19,   16, 256, 0, 8, 14, 1},
   {"NORWAY",   font20,   16, 256, 0, 8, 14, 1},
   {"SANSERIF", font21,   16, 256, 0, 8, 14, 1},
   {"SCRIPT",   font22,   16, 256, 0, 8, 14, 1},
   {"SLANT",    font23,   16, 256, 0, 8, 14, 0},
   {"SMALL",    font24,   16, 256, 0, 6,  8, 0},
   {"STANDARD", font25,   16, 256, 0, 8, 14, 1},
   {"STRETCH",  font26,   16, 256, 0, 8, 14, 1},
   {"SUB",      font27,   16, 256, 0, 8, 14, 0},
   {"SUPER",    font28,   16, 256, 0, 8, 14, 0},
   {"THIN",     font29,   16, 256, 0, 8, 14, 1},
   {"THIN8X8",  font30,   16, 256, 0, 8,  8, 0},
   {"THNSERIF", font31,   16, 256, 0, 8, 14, 0},
   {"SAA5050",  saa5050,  10, 256, 0, 6, 10, 0},
   {"SAA5051",  saa5051,  10, 256, 0, 6, 10, 0},
   {"SAA5052",  saa5052,  10, 256, 0, 6, 10, 0},
   {"SAA5053",  saa5053,  10, 256, 0, 6, 10, 0},
   {"SAA5054",  saa5054,  10, 256, 0, 6, 10, 0},
   {"SAA5055",  saa5055,  10, 256, 0, 6, 10, 0},
   {"SAA5056",  saa5056,  10, 256, 0, 6, 10, 0},
   {"SAA5057",  saa5057,  10, 256, 0, 6, 10, 0},
   {"6847",     font6847, 12, 128, 0, 8, 12, 0}
};

#define NUM_FONTS (sizeof(font_catalog) / sizeof(font_t))

// ==========================================================================
// Static Methods
// ==========================================================================

// a is the 12 pixel row we wish to apply rounding to
// b is the 12 pixel row we are using as a reference (either one above or one below)
static inline uint16_t combine_rows(uint16_t a, uint16_t b) {
    return a | ((a >> 1) & b & ~(b >> 1)) | ((a << 1) & b & ~(b << 1));
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
   uint8_t  *src = font->data;
   uint16_t *dst = font->buffer;
   if (rounding) {
      // Stage 1: expand each pixel to 2x2 pixels
      for (int i = 0; i < font->num_chars; i++) {
         // Copy the defined part of the font
         for (int j = 0; j < font->height; j++) {
            uint8_t data = *src++;
            uint16_t expanded = 0;
            for (int k = 0; k < 8; k++) {
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
         // Skip any padding between characters
         src += font->bytes_per_char - font->height;
      }
      // Stage 2: perform rounding
      dst = font->buffer;
      // Special case the SAA fonts to avoid rounding the graphics
      int num = strncmp(font->name, "SAA505", 6) ? font->num_chars : 128;
      for (int i = 0; i < num; i++) {
         for (int j = 0; j < font->height - 1; j++) {
            uint16_t o_row = dst[j * 2 + 1];
            uint16_t e_row = dst[j * 2 + 2];
            dst[j * 2 + 1] = combine_rows(o_row, e_row);
            dst[j * 2 + 2] = combine_rows(e_row, o_row);
         }
         dst += font->height * 2;
      }
   } else {
      for (int i = 0; i < font->num_chars; i++) {
         // Copy the defined part of the font
         for (int j = 0; j < font->height; j++) {
            *dst++ = *src++;
         }
         // Skip any padding between character
         src += font->bytes_per_char - font->height;
      }
   }
}

static char *default_get_name(font_t *font) {
   return font->name;
}

static int default_get_spacing_w(font_t *font) {
   return font->spacing_w;
}

static int default_get_spacing_h(font_t *font) {
   return font->spacing_w;
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
   return (font->width << font->rounding) * font->scale_w + font->spacing_w;

}

static int default_get_overall_h(font_t *font) {
   return (font->height << font->rounding) * font->scale_h + font->spacing_h;
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
         int col = (data & mask) ? fg_col : bg_col;
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

static int default_read_char(font_t *font, screen_mode_t *screen, int x, int y) {
   int screendata[MAX_FONT_HEIGHT];
   // Read the character from screen memory
   int *dp = screendata;
   int width  = font->width  << font->rounding;
   int height = font->height << font->rounding;
   for (int i = 0; i < height * font->scale_h; i += font->scale_h) {
      int row = 0;
      for (int j = 0; j < width * font->scale_w; j += font->scale_w) {
         row <<= 1;
         if (screen->get_pixel(screen, x + j, y - i)) {
            row |= 1;
         }
      }
      *dp++ = row;
   }
   // Match against font
   int all_zeros = 0;
   int all_ones  = (1 << width) - 1;
   for (int c = 0x20; c < font->num_chars; c++) {
      for (y = 0; y < height; y++) {
         int xor = font->buffer[c * height + y] ^ screendata[y];
         if (xor != all_zeros && xor != all_ones) {
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

static void initialize_font(font_t * font) {

   // The factor of 4 allows for character rounding to be enabled
   size_t size = font->height * font->num_chars * 4;
   if (font->buffer == NULL) {
      font->buffer = (uint16_t *)malloc(size);
   }

   // Set the default metrics
   font->scale_w   = 1;
   font->scale_h   = 1;
   font->spacing_w = 0;
   font->spacing_h = 0;
   font->rounding  = 0;

   // Default implementation of setters
   font->set_spacing_w = default_set_spacing_w;
   font->set_spacing_h = default_set_spacing_h;
   font->set_scale_w   = default_set_scale_w;
   font->set_scale_h   = default_set_scale_h;
   font->set_rounding  = default_set_rounding;

   // Default implementation of getters
   font->get_name      = default_get_name;
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
}

// ==========================================================================
// Public Methods
// ==========================================================================

font_t *get_font_by_number(unsigned int num) {
   font_t *font = &font_catalog[DEFAULT_FONT];
   if (num < NUM_FONTS) {
      font = &font_catalog[num];
   }
   initialize_font(font);
   return font;
}

font_t *get_font_by_name(char *name) {
   font_t *font = &font_catalog[DEFAULT_FONT];
   for (unsigned int num = 0; num < NUM_FONTS; num++) {
      if (!strcasecmp(name, font_catalog[num].name)) {
         font = &font_catalog[num];
         break;
      }
   }
   initialize_font(font);
   return font;
}

// TODO: handle rounding being on
void define_character(font_t *font, uint8_t c, uint8_t *data) {
   uint16_t *p = font->buffer + c * font->height;
   for (int i = 0; i < 8; i++) {
      *p++ = *data++;
   }
}
