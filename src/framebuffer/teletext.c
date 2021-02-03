// Teletext support for MODE 7
// Rod Thomas January 2021

#include <stdlib.h>
#include <stdio.h>
#include <inttypes.h>

#include "framebuffer.h"
#include "teletext.h"
#include "screen_modes.h"

// Main structure holding Teletext state
struct {
   tt_colour_t fgd_colour;
   tt_colour_t bgd_colour;
   int graphics;
   int separated;
   int doubled;
   int has_double;
   int double_top; // Set for the top line of doubled text, clear for the bottom line
   int flashing;
   int concealed;
   int held;
   int held_char;
   int columns;
   int rows;
   int xstart;
   int ystart;
   int char_w;
   int char_h;
   int size_w;
   int size_h;
   int scale_w;
   int scale_h;
   font_t *font;
} tt = {TT_WHITE, TT_BLACK, FALSE, FALSE, FALSE, FALSE, TRUE, FALSE, FALSE, FALSE, 32, 40, 25, 0, 0, 6, 10, 15, 20, 1, 1};

// Arbitrary limit on the number of flash regions
#define TT_MAX_FLASH_REGION 25

struct{
   int fgd_colour;
   int bgd_colour;
   int row;
   int column;
   int length;
   int chars[40];
} tt_flash_region [TT_MAX_FLASH_REGION];
int tt_no_flash_region = 0;


// This is called on initialization, and VDU 20 to set the default palette

static void tt_reset(screen_mode_t *screen) {
   // Set up MODE 7
   tt.size_w = tt.char_w * 5 / 2;
   tt.size_h = tt.char_h * 2;
   int width = tt.columns * tt.size_w;
   int height = tt.rows * tt.size_h;
   tt.scale_w = screen->width / width;
   tt.scale_h = screen->height / height;
   tt.xstart = 0;
   tt.ystart = screen->height - 1;
   //fb_set_char_metrics(tt.size_w, tt.size_h, tt.scale_w, tt.scale_h);
   //fb_reset_areas();
   tt_no_flash_region = 0;
   tt.font = get_font_by_name("SAA5050");
   screen->set_colour(screen, TT_BLACK,     0x00, 0x00, 0x00);
   screen->set_colour(screen, TT_RED,       0xff, 0x00, 0x00);
   screen->set_colour(screen, TT_GREEN,     0x00, 0xff, 0x00);
   screen->set_colour(screen, TT_YELLOW,    0xff, 0xff, 0x00);
   screen->set_colour(screen, TT_BLUE,      0x00, 0x00, 0xff);
   screen->set_colour(screen, TT_MAGENTA,   0xff, 0x00, 0xff);
   screen->set_colour(screen, TT_CYAN,      0x00, 0xff, 0xff);
   screen->set_colour(screen, TT_WHITE,     0xff, 0xff, 0xff);
}

static void tt_reset_line_state() {
   // Reset the state at the beginning of each line
   if (tt.has_double) {
      tt.double_top = !tt.double_top;
   }
   tt.fgd_colour = TT_WHITE;
   tt.bgd_colour = TT_BLACK;
   tt.graphics = FALSE;
   tt.separated = FALSE;
   tt.doubled = FALSE;
   tt.has_double = FALSE;
   tt.flashing = FALSE;
   tt.concealed = FALSE;
   tt.held = FALSE;
}

static void tt_flash(screen_mode_t *screen) {
   // Make the flash regions flash
   // Note the flash is asymmetric with the text shown twice as long as its hidden
   static int count = 2;
   if (count < 2) {
      int fgd = tt.fgd_colour;
      int bgd = tt.bgd_colour;
      int c = 32;
      for (int f = 0; f < tt_no_flash_region; f++) {
         for (int i = 0; i < tt_flash_region[f].length; i++) {
            int row = tt_flash_region[f].row;
            int col = tt_flash_region[f].column + i;
            if (count == 1) {
               tt.bgd_colour = TT_BLACK;
            } else {
               c = tt_flash_region[f].chars[i];
               tt.fgd_colour = tt_flash_region[f].fgd_colour;
               tt.bgd_colour = tt_flash_region[f].bgd_colour;
            }
            screen->draw_character(screen, tt.font, c, col, row, tt.fgd_colour, tt.bgd_colour);
         }
      }
      tt.fgd_colour = fgd;
      tt.bgd_colour = bgd;
   }
   if (count == 0) count = 2;
   else count--;
}

static void tt_add_flash_char(int c) {
   // Add a character position to the current flash region
   int i = tt_no_flash_region - 1;
   if (i >= 0 && i < TT_MAX_FLASH_REGION) {
      tt_flash_region[i].chars[tt_flash_region[i].length++] = c;
   }
}

static void tt_start_flash_area(int col, int row) {
   // Create a new flash region
   int i = tt_no_flash_region;
   if (i < TT_MAX_FLASH_REGION) {
      tt_flash_region[i].fgd_colour = tt.fgd_colour;
      tt_flash_region[i].bgd_colour = tt.bgd_colour;
      tt_flash_region[i].column = col;
      tt_flash_region[i].row = row;
      tt_flash_region[i].length = 0;
      tt_no_flash_region++;
   }
}

static int tt_process_controls(int c, int col, int row) {
   // Process control characters, when graphics hold is in force some settings
   // are deferred until after the held character is printed e.g. colour changes
   // see tt_process_controls_after
   int rc = 32;
   if (c < 128 || c > 159 ) {
      rc = c;
      if (tt.held) {
         tt.held_char = c;
      }
   } else if (tt.held) {
      rc = tt.held_char;
   }
   if (tt.flashing) {
      tt_add_flash_char(c);
   }
   switch (c) {
   case TT_A_RED ... TT_A_WHITE:
      if (!tt.held) {
         tt.graphics = FALSE;
         tt.concealed = FALSE;
         tt.fgd_colour = c - 128;
      }
      break;
   case TT_FLASH:
      tt.flashing = TRUE;
      tt_start_flash_area(col + 1, row);
      break;
   case TT_STEADY:
      tt.flashing = FALSE;
      break;
   case TT_NORMAL:
      if (!tt.held) {
         tt.doubled = FALSE;
      }
      break;
   case TT_DOUBLE:
      if (!tt.held) {
         tt.doubled = TRUE;
         tt.has_double = TRUE;
      }
      break;
   case TT_G_RED ... TT_G_WHITE:
      if (!tt.held) {
         tt.graphics = TRUE;
         tt.concealed = FALSE;
         tt.fgd_colour = c - 144;
      }
      break;
   case TT_CONCEAL:
      tt.concealed = TRUE;
      break;
   case TT_CONTIGUOUS:
      if (!tt.held) tt.separated = FALSE;
      break;
   case TT_SEPARATED:
      if (!tt.held) tt.separated = TRUE;
      break;
   case TT_BLACK_BGD:
      tt.bgd_colour = TT_BLACK;
      break;
   case TT_NEW_BGD:
      tt.bgd_colour = tt.fgd_colour;
      break;
   case TT_HOLD:
      tt.held = TRUE;
      tt.held_char = 32;
      break;
   }
   return rc;
}

static void tt_process_controls_after(int c, int col, int row) {
   switch(c) {
   case TT_A_RED ... TT_A_WHITE:
      tt.graphics = FALSE;
      tt.concealed = FALSE;
      tt.fgd_colour = c - 128;
      tt.held = FALSE;
      break;
   case TT_NORMAL:
      tt.doubled = FALSE;
      tt.held = FALSE;
      break;
   case TT_DOUBLE:
      tt.doubled = TRUE;
      tt.has_double = TRUE;
      tt.held = FALSE;
      break;
   case TT_G_RED ... TT_G_WHITE:
      tt.graphics = TRUE;
      tt.concealed = FALSE;
      tt.fgd_colour = c - 144;
      break;
   case TT_CONTIGUOUS:
      tt.separated = FALSE;
      if (tt.held_char != 32) tt.held = FALSE;
      break;
   case TT_SEPARATED:
      tt.separated = TRUE;
      if (tt.held_char != 32) tt.held = FALSE;
      break;
   case TT_RELEASE:
      tt.held = FALSE;
      tt.held_char = 32;
      break;
   }
}

static inline void tt_put_block(screen_mode_t *screen, int x, int y, tt_block_t map) {
   // For each character pixel puts a 2x2 block with character rounding according to the map
   // Scaling is carried out to each element of the block to achieve a full screen
   int ydouble = (tt.doubled) ? 2 : 1;
   int yscale = tt.scale_h * ydouble;
   for (int y2 = 0; y2 < 2; y2 ++) {
      for (int ys = 0; ys < yscale; ys++) {
         for (int x2 = 0; x2 < 2; x2++) {
            int code = (x2 + 1) * (y2 * 3 + 1);
            unsigned int colour = (map & code) ? tt.fgd_colour : tt.bgd_colour;
            int xscale = (x2 == 0) ? 2 : 3;
            for (int xs = 0; xs < xscale; xs++) {
               screen->set_pixel(screen, x + x2 * 2 + xs, y - y2 * yscale - ys, colour);
            }
         }
      }
   }
}

static inline int tt_pixel_set(int p, int x, int y) {
   // Tests whether the given pixel is set to foreground colour
   return ((tt.font->buffer[p + y] >> (tt.char_w - x - 1)) & 1);
}

// TODO: Font is ignored for now; the teletext font is hard coded
static void tt_draw_character(struct screen_mode *screen, font_t *font, int c, int col, int row, pixel_t fg_col, pixel_t bg_col) {
   if (col == 0) {
      tt_reset_line_state();
   }
   int ch = tt_process_controls(c, col, row);
   int colour[2];
   int xoffset = tt.xstart + col * tt.size_w * tt.scale_w;
   int yoffset = tt.ystart - row * tt.size_h * tt.scale_h;
   if (tt.graphics && ((ch > 0x9f && ch < 0xc0) || (ch > 0xdf && ch <= 0xff))) {
      // Draws the graphics characters based on the 2x6 matrix
      if (ch & 64) ch |= 0x20; // Set bit 6 if the 6th block is set
      else ch &= 0x1f;
      int y = yoffset;
      for (int yb = 0; yb < 3; yb++) { // y blocks
         int ysize = (yb == 1) ? 16 : 12;
         for (int y2 = 0; y2 < ysize; y2++, y--) {
            int x = xoffset;
            colour[0] = ((ch >> (2 * yb)) & 1) ? fg_col : bg_col;
            colour[1] = ((ch >> (2 * yb + 1)) & 1) ? fg_col : bg_col;
            for (int xb = 0; xb < 2; xb ++) { // x blocks
               for (int x2 = 0; x2 < tt.size_w; x2++, x++) {
                  if (tt.separated && ((x2 < 5) || (y2 >= ysize - 4))) {
                     screen->set_pixel(screen, x, y, bg_col);
                  } else {
                     screen->set_pixel(screen, x, y, colour[xb]);
                  }
               }
            }
         }
      }
   } else {
      // Draw character
      int p = ch * tt.char_h;
      int y = yoffset;
      int py_from = 0;
      int py_to = tt.char_h;
      int yinc = (tt.scale_h * 2);
      if (tt.doubled) {
         yinc *= 2;
         if (tt.double_top) {
            py_to = py_to / 2;
         } else {
            py_from = tt.char_h / 2;
         }
      }
      for (int py = py_from; py < py_to; py++, y -= yinc) {
         int x = xoffset;
         for (int px = 0; px < tt.char_w; px++, x += (tt.scale_w * 5 / 2)) {
            int map = TT_BLOCK_NONE;
            if (tt_pixel_set(p, px, py))
               map = TT_BLOCK_ALL;
            else {
               // Test surrounding pixels to determine rounding
               if (px > 0
                   && py > 0
                   && tt_pixel_set(p, px, py - 1)
                   && tt_pixel_set(p, px - 1, py)
                   && !tt_pixel_set(p, px - 1, py - 1))
                  map |= TT_BLOCK_TL;
               if (px < tt.char_w - 1
                   && py > 0
                   && tt_pixel_set(p, px, py - 1)
                   && tt_pixel_set(p, px + 1, py)
                   && !tt_pixel_set(p, px + 1, py - 1))
                  map |=  TT_BLOCK_TR;
               if (px > 0
                   && py < tt.char_h - 1
                   && tt_pixel_set(p, px - 1, py)
                   && tt_pixel_set(p, px, py + 1)
                   && !tt_pixel_set(p, px - 1, py + 1))
                  map |= TT_BLOCK_BL;
               if (px < tt.char_w - 1
                   && py < tt.char_h - 1
                   && tt_pixel_set(p, px + 1, py)
                   && tt_pixel_set(p, px, py + 1)
                   && !tt_pixel_set(p, px + 1, py + 1))
                  map |= TT_BLOCK_BR;
            }
            tt_put_block(screen, x, y, map);
         }
      }
   }
   tt_process_controls_after(c, col, row);
}

static screen_mode_t teletext_screen_mode = {
   .mode_num       = 7,
   .width          = 480, // 40 * 12
   .height         = 500, // 25 * 16
   .xeigfactor     = 1,
   .yeigfactor     = 1,
   .bpp            = 8,
   .ncolour        = 255,
   .reset          = tt_reset,
   .flash          = tt_flash,
   .draw_character = tt_draw_character
};

screen_mode_t *tt_get_screen_mode() {
   return &teletext_screen_mode;
}
