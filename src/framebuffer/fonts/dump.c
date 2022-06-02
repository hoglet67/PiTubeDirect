#include <stdio.h>
#include <inttypes.h>

#include "bbc.fnt.h"
#include "6847.fnt.h"
#include "8x10.fnt.h"
#include "8x11snsf.fnt.h"
#include "8x14.fnt.h"
#include "8x8.fnt.h"
#include "8x8ital.fnt.h"
#include "9x16snsf.fnt.h"
#include "bigserif.fnt.h"
#include "blcksnsf.fnt.h"
#include "block.fnt.h"
#include "bold.fnt.h"
#include "broadway.fnt.h"
#include "computer.fnt.h"
#include "courier.fnt.h"
#include "future.fnt.h"
#include "greek.fnt.h"
#include "hollow.fnt.h"
#include "italics.fnt.h"
#include "lcd.fnt.h"
#include "medieval.fnt.h"
#include "norway.fnt.h"
#include "sanserif.fnt.h"
#include "script.fnt.h"
#include "slant.fnt.h"
#include "small.fnt.h"
#include "standard.fnt.h"
#include "stretch.fnt.h"
#include "sub.fnt.h"
#include "super.fnt.h"
#include "thin8x8.fnt.h"
#include "thin.fnt.h"
#include "thnserif.fnt.h"

typedef struct font {
   // The font data itself
   const char *name;
   const uint8_t *data;
   int bytes_per_char;
   int num_chars;
} font_t;

static font_t font_catalog[] = {
   {"BBC",      fontbbc,   8, 128},
   {"8X10",     font01,   16, 256},
   {"8X11SNSF", font02,   16, 256},
   {"8X14",     font03,   16, 256},
   {"8X8",      font04,   16, 256},
   {"8X8ITAL",  font05,   16, 256},
   {"9X16",     font06,   16, 256},
   {"BIGSERIF", font07,   16, 256},
   {"BLCKSNSF", font08,   16, 256},
   {"BLOCK",    font09,   16, 256},
   {"BOLD",     font10,   16, 256},
   {"BROADWAY", font11,   16, 256},
   {"COMPUTER", font12,   16, 256},
   {"COURIER",  font13,   16, 256},
   {"FUTURE",   font14,   16, 256},
   {"GREEK",    font15,   16, 256},
   {"HOLLOW",   font16,   16, 256},
   {"ITALICS",  font17,   16, 256},
   {"LCD",      font18,   16, 256},
   {"MEDIEVAL", font19,   16, 256},
   {"NORWAY",   font20,   16, 256},
   {"SANSERIF", font21,   16, 256},
   {"SCRIPT",   font22,   16, 256},
   {"SLANT",    font23,   16, 256},
   {"SMALL",    font24,   16, 256},
   {"STANDARD", font25,   16, 256},
   {"STRETCH",  font26,   16, 256},
   {"SUB",      font27,   16, 256},
   {"SUPER",    font28,   16, 256},
   {"THIN",     font29,   16, 256},
   {"THIN8X8",  font30,   16, 256},
   {"THNSERIF", font31,   16, 256},
   {"6847",     font6847, 12, 128},
};

#define NUM_FONTS (sizeof(font_catalog) / sizeof(font_t))

void main() {

   int stats[8 * 16];

   for (int i = 0; i < NUM_FONTS; i++) {

      font_t *font = &font_catalog[i];

      printf("\n%d = %s\n\n", i, font->name);

      for (int j = 0; j < 8 * 16; j++) {
         stats[j] = 0;
      }

      for (int c = 0; c < font->num_chars; c++) {
         for (int r = 0; r < font->bytes_per_char; r++) {
            int line = font->data[c * font->bytes_per_char + r];
            int mask = 128;
            for (int b = 0; b < 8; b++) {
               if (line & mask) {
                  stats[r * 8 + b]++;
               }
               mask >>= 1;
            }
         }
      }


      for (int r = 0; r < 16; r++) {
         printf("%2d : ", r);
         for (int b = 0; b < 8; b++) {
            printf("%4d", stats[r * 8 + b]);
         }
         printf("\n");
      }

   }
}
