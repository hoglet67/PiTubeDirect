#include <inttypes.h>

/* Reserve room for 256 sprites of 16x16 pixels each
 * that is 256 bytes per sprite. So we need to reserve
 * 65535 bytes for both the sprites and the "backup"
 * space.
 */

uint8_t sprites[0xFFFF];
#ifdef BPP32
uint32_t spsave[0xFFFF];
#else
uint16_t spsave[0xFFFF];
#endif

void sp_save(int sprite, int sx, int sy) {
   int x, y;
   for (x=0; x<16; x++) {
      for (y=0; y<16; y++) {
         spsave[sprite*256+x+y*16] = fb_getpixel(sx+x*2, sy-y*2);
      }
   }
}

void sp_restore(int sprite, int sx, int sy) {
   int x, y;
   for (x=0; x<16; x++) {
      for (y=0; y<16; y++) {
         fb_setpixel(sx+x*2, sy-y*2, spsave[sprite*256+x+y*16]);
      }
   }
}

void sp_set(int sprite, int sx, int sy) {
   int x, y, col;
   for (y=0; y<16; y++) {
      for (x=0; x<16; x++) {
         col = sprites[sprite*256+x+y*16];
         if (col != 255) {
            // pixel set, set to colour
            fb_putpixel(sx+x*2, sy-y*2, col);
         }
      }
   }
}
