#include <inttypes.h>
#include <string.h>

#include "vdu23.h"
#include "mousepointers.h"
#include "sprites.h"
#include "fonts.h"

// For fb_write for errors
#include "framebuffer.h"

static char vdu23buf[300];

static void do_vdu23_command(screen_mode_t *screen, font_t *font) {

   unsigned int x, y, col;
   unsigned int p, i, b, mp;

#ifdef DEBUG_VDU
   for (i=0; i<9; i++) {
      printf("%X", vdu23buf[i]);
      printf("\n\r");
   }
#endif

   // VDU 23,3: select font by name
   // params: eight bytes font name, trailing zeros if name shorter than 8 chars
   if (vdu23buf[0] == 3) {
      font_t *font = get_font(vdu23buf+1);
      if (!font) {
         fb_writes("No matching font found\r\n");
         // TODO: Fix hard-coded font data size
      } else {
      }
   }

   // VDU 23,4: set font scale
   // params: hor. scale, vert. scale, char spacing, other 5 bytes are ignored
   if (vdu23buf[0] == 4) {
      font->scale_w = vdu23buf[1] > 0 ? vdu23buf[1] : 1;
      font->scale_h = vdu23buf[2] > 0 ? vdu23buf[2] : 1;
      font->spacing = vdu23buf[3];
#ifdef DEBUG_VDU
      printf("Font scale set to %d,%d\r\n", font_scale_w, font_scale_h);
#endif
   }

   // vdu 23,5: set mouse pointer
   // params:   mouse pointer id ( 0...31)
   //           mouse x ( 0 < x < 256)
   //           reserved: should be 0
   //           mouse y ( 64 < y < 256)
   //           reserved: should be 0
   //           mouse colour red (0 < r < 31)
   //           mouse colour green (0 < g < 63)
   //           mouse colour blue (0 < b < 31)
   if (vdu23buf[0] == 5) {
      x = vdu23buf[2] * 5;
      y = (vdu23buf[4]-64) * 5.3333;
      if (mp_save_x != 65535) {
         mp_restore(screen);
      }
      mp_save_x = x;
      mp_save_y = y;
      mp_save(screen);
      col = ((vdu23buf[6]&0x1F) << 11) + ((vdu23buf[7]&0x3F) << 5) + (vdu23buf[8]&0x1F);
      // Draw black mask in actuale background colour
      mp = vdu23buf[1];

      // Read the current graphics background colour, and translate to a pixel value
      pixel_t g_bg_col =screen->get_colour(screen, fb_get_g_bg_col());
      for (i=0; i<8; i++) {
         p = mpblack[mp * 8 + i];
         for (b=0; b<8; b++) {
            if (p&0x80) {
               // mask pixel set, set to bg colour
               screen->set_pixel(screen, x + b * 2, y - i * 2, g_bg_col);
            }
            p <<= 1;
         }
      }

      // Draw white mask in given foreground colour
      for (i=0; i<8; i++) {
         p = mpwhite[mp * 8 + i];
         for (b=0; b<8; b++) {
            if (p&0x80) {
               // mask pixel set, set to foreground colour
               screen->set_pixel(screen, x + b * 2, y - i * 2, col);
            }
            p <<= 1;
         }
      }
   }

   // vdu 23,6: turn off mouse pointer
   // params:   none
   if (vdu23buf[0] == 6) {
      mp_restore(screen);
      mp_save_x = 65535;
      mp_save_y = 65535;
   }

   // vdu 23,7: define sprite
   // params:   sprite number
   //           16x16 pixels (256 bytes)
   if (vdu23buf[0] == 7) {
      memcpy(sprites+vdu23buf[1]*256, vdu23buf+2, 256);
   }

   // vdu 23,8: set sprite
   // params:   sprite number (1 byte)
   //           x position (1 word)
   //           y position (1 word)
   if (vdu23buf[0] == 8) {
      x = vdu23buf[2] + vdu23buf[3]*256;
      y = vdu23buf[4] + vdu23buf[5]*256;
      p = vdu23buf[1];

      sp_save(screen, p, x, y);
      sp_set(screen, p, x, y);
   }

   // vdu 23,9: unset sprite
   // params:   sprite number (1 byte)
   //           x position (1 word)
   //           y position (1 word)
   if (vdu23buf[0] == 9) {
      x = vdu23buf[2] + vdu23buf[3]*256;
      y = vdu23buf[4] + vdu23buf[5]*256;
      p = vdu23buf[1];

      sp_restore(screen, p, x, y);
   }

}


vdu_state_t do_vdu23(screen_mode_t *screen, font_t *font, uint8_t c) {
   static int count = 0;
   static int size = 0;

   vdu23buf[count] = c;

   if (count == 0) {
      size = (c == 7) ? 258 : 9;
   }

   if (count == size - 1) {
      do_vdu23_command(screen, font);
      count = 0;
      return NORMAL;
   } else {
      count++;
      return IN_VDU23;
   }
}
