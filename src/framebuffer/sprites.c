#include "sprites.h"

/* Reserve room for 256 sprites of 16x16 pixels each
 * that is 256 bytes per sprite. So we need to reserve
 * 65535 bytes for both the sprites and the "backup"
 * space.
 */

uint8_t sprites[0xFFFF];

static pixel_t spsave[0xFFFF];

void sp_save(screen_mode_t *screen, int sprite, int sx, int sy) {
   for (int x = 0; x < 16; x++) {
      for (int y = 0; y < 16; y++) {
         spsave[sprite * 256 + y * 16 + x] = screen->get_pixel(screen, sx + x * 2, sy - y * 2);
      }
   }
}

void sp_restore(screen_mode_t *screen, int sprite, int sx, int sy) {
   for (int x = 0; x < 16; x++) {
      for (int y = 0; y < 16; y++) {
         screen->set_pixel(screen, sx + x * 2, sy - y * 2, spsave[sprite * 256 + y * 16 + x]);
      }
   }
}

void sp_set(screen_mode_t *screen, int sprite, int sx, int sy) {
   for (int y = 0; y < 16; y++) {
      for (int x = 0; x < 16; x++) {
         int index = sprites[sprite * 256 + y * 16 + x];
         if (index != 255) {
            pixel_t pixel = screen->get_colour(screen, index);
            // pixel set, set to colour
            //fb_putpixel(sx+x*2, sy-y*2, col);
            screen->set_pixel(screen, sx + x * 2, sy - y * 2, pixel);
         }
      }
   }
}
