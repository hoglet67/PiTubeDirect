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

// Font Catalog

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
   {"SAA5050",  saa5050,  12, 256, 0, 6, 10, 0},
   {"6847",     font6847, 12, 128, 0, 8, 12, 0}
};

#define NUM_FONTS (sizeof(font_catalog) / sizeof(font_t))

static void initialize_font(font_t * font) {
   size_t size = font->bytes_per_char * font->num_chars;
   if (font->buffer == NULL) {
      font->buffer = (uint8_t *)malloc(size);
   }
   // Copy the font into a local font buffer, so VDU 23 can update it
   memcpy(font->buffer, font->data, size);
   // Set the default scale
   font->scale_w = 1;
   font->scale_h = 1;
}

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

void define_character(font_t *font, uint8_t c, uint8_t *data) {
   uint8_t *p = font->buffer + c * font->bytes_per_char;
   for (int i = 0; i < 8; i++) {
      *p++ = *data++;
   }
}
