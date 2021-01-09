#include <inttypes.h>
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

int font_width = 8;
int font_height = 16;
int font_scale_w = 1;
int font_scale_h = 1;
int font_spacing = 0;

uint8_t fontbuffer[4096];

void gr_draw_character(int c, int x_pos, int y_pos, int colour)
{
#ifdef DEBUG_VDU
   printf("gr_draw_character %c at %d,%d; data: ", c, x_pos, y_pos);
#endif

   // Set pointer to character in fontbuffer, characters are plotted upside down so
   // the pointer goes to the last byte and then decreases.
   int p = (c+1) * 16;

   // other variables
   int i, j, data;
   int x = x_pos;
   int y = y_pos;
   int sx, sy;

   for(j=0; j<16; j++) {
      data = fontbuffer[--p];
#ifdef DEBUG_VDU
      printf("%02X ", data);
#endif
      for(i=0; i<8; i++) {
         if (data & 0x80) {
            for (sx=0; sx<font_scale_w; sx++) {
               for (sy=0; sy<font_scale_h; sy++) {
                  fb_putpixel(x+sx, y+sy, colour);
               }
            }
         }
         x += font_scale_w;
         data <<= 1;
      }
      x = x_pos;
      y += font_scale_h;
   }
#ifdef DEBUG_VDU
   printf("\r\n");
#endif

}
