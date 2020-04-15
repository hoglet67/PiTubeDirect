#include <inttypes.h>
#include <stdio.h>
#include "mousepointers.h"
#include "sprites.h"

char vdu23buf[300];
int vdu23cnt;

void do_vdu23(void) {
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
      if (! strncmp(vdu23buf+1, "8X10", 4)) {
         memcpy(fontbuffer, font01, 4096);
      } else if (! strncmp(vdu23buf+1, "8X11SNSF", 8)) {
         memcpy(fontbuffer, font02, 4096);
      } else if (! strncmp(vdu23buf+1, "8X14", 4)) {
         memcpy(fontbuffer, font03, 4096);
      } else if (! strncmp(vdu23buf+1, "8X8", 3)) {
         memcpy(fontbuffer, font04, 4096);
      } else if (! strncmp(vdu23buf+1, "8X8ITAL", 7)) {
         memcpy(fontbuffer, font05, 4096);
      } else if (! strncmp(vdu23buf+1, "9X16", 4)) {
         memcpy(fontbuffer, font06, 4096);
      } else if (! strncmp(vdu23buf+1, "BIGSERIF", 8)) {
         memcpy(fontbuffer, font07, 4096);
      } else if (! strncmp(vdu23buf+1, "BLCKSNSF", 8)) {
         memcpy(fontbuffer, font08, 4096);
      } else if (! strncmp(vdu23buf+1, "BLOCK", 5)) {
         memcpy(fontbuffer, font09, 4096);
      } else if (! strncmp(vdu23buf+1, "BOLD", 4)) {
         memcpy(fontbuffer, font10, 4096);
      } else if (! strncmp(vdu23buf+1, "BROADWAY", 8)) {
         memcpy(fontbuffer, font11, 4096);
      } else if (! strncmp(vdu23buf+1, "COMPUTER", 8)) {
         memcpy(fontbuffer, font12, 4096);
      } else if (! strncmp(vdu23buf+1, "COURIER", 7)) {
         memcpy(fontbuffer, font13, 4096);
      } else if (! strncmp(vdu23buf+1, "FUTURE", 6)) {
         memcpy(fontbuffer, font14, 4096);
      } else if (! strncmp(vdu23buf+1, "GREEK", 5)) {
         memcpy(fontbuffer, font15, 4096);
      } else if (! strncmp(vdu23buf+1, "HOLLOW", 6)) {
         memcpy(fontbuffer, font16, 4096);
      } else if (! strncmp(vdu23buf+1, "ITALICS", 7)) {
         memcpy(fontbuffer, font17, 4096);
      } else if (! strncmp(vdu23buf+1, "LCD", 3)) {
         memcpy(fontbuffer, font18, 4096);
      } else if (! strncmp(vdu23buf+1, "MEDIEVAL", 8)) {
         memcpy(fontbuffer, font19, 4096);
      } else if (! strncmp(vdu23buf+1, "NORWAY", 6)) {
         memcpy(fontbuffer, font20, 4096);
      } else if (! strncmp(vdu23buf+1, "SANSERIF", 8)) {
         memcpy(fontbuffer, font21, 4096);
      } else if (! strncmp(vdu23buf+1, "SCRIPT", 6)) {
         memcpy(fontbuffer, font22, 4096);
      } else if (! strncmp(vdu23buf+1, "SLANT", 5)) {
         memcpy(fontbuffer, font23, 4096);
      } else if (! strncmp(vdu23buf+1, "SMALL", 5)) {
         memcpy(fontbuffer, font24, 4096);
      } else if (! strncmp(vdu23buf+1, "STANDARD", 8)) {
         memcpy(fontbuffer, font25, 4096);
      } else if (! strncmp(vdu23buf+1, "STRETCH", 7)) {
         memcpy(fontbuffer, font26, 4096);
      } else if (! strncmp(vdu23buf+1, "SUB", 3)) {
         memcpy(fontbuffer, font27, 4096);
      } else if (! strncmp(vdu23buf+1, "SUPER", 5)) {
         memcpy(fontbuffer, font28, 4096);
      } else if (! strncmp(vdu23buf+1, "THIN", 4)) {
         memcpy(fontbuffer, font29, 4096);
      } else if (! strncmp(vdu23buf+1, "THIN8X8", 7)) {
         memcpy(fontbuffer, font30, 4096);
      } else if (! strncmp(vdu23buf+1, "THNSERIF", 8)) {
         memcpy(fontbuffer, font31, 4096);
      } else {
         printf("No matching font found\r\n");
      }
   }

   // VDU 23,4: set font scale
   // params: hor. scale, vert. scale, char spacing, other 5 bytes are ignored
   if (vdu23buf[0] == 4) {
      font_scale_w = vdu23buf[1] > 0 ? vdu23buf[1] : 1;
      font_scale_h = vdu23buf[2] > 0 ? vdu23buf[2] : 1;
      font_spacing = vdu23buf[3];
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
         mp_restore();
      }
      mp_save_x = x;
      mp_save_y = y;
      mp_save();
      col = ((vdu23buf[6]&0x1F) << 11) + ((vdu23buf[7]&0x3F) << 5) + (vdu23buf[8]&0x1F);
      // Draw black mask in actuale background colour
      mp = vdu23buf[1];
      for (i=0; i<8; i++) {
         p = mpblack[mp * 8 + i];
         for (b=0; b<8; b++) {
            if (p&0x80) {
               // mask pixel set, set to bg colour
               fb_putpixel(x+b*2, y-i*2, g_bg_col);
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
               fb_setpixel(x+b*2, y-i*2, col);
            }
            p <<= 1;
         }
      }
   }

   // vdu 23,6: turn off mouse pointer
   // params:   none
   if (vdu23buf[0] == 6) {
      mp_restore();
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

      sp_save(p,x,y);
      sp_set(p,x,y);
   }

   // vdu 23,9: unset sprite
   // params:   sprite number (1 byte)
   //           x position (1 word)
   //           y position (1 word)
   if (vdu23buf[0] == 9) {
      x = vdu23buf[2] + vdu23buf[3]*256;
      y = vdu23buf[4] + vdu23buf[5]*256;
      p = vdu23buf[1];

      sp_restore(p,x,y);
   }

}
