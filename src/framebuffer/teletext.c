// Teletext support for MODE 7
// Rod Thomas January 2021

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <inttypes.h>

#include "framebuffer.h"
#include "teletext.h"
#include "screen_modes.h"

// TODO: These should somehow be passed into the screen mode constructor
#define COLUMNS 40
#define ROWS    25

// Main structure holding Teletext state
struct {

   // Current line state
   tt_colour_t fgd_colour;
   tt_colour_t bgd_colour;
   int graphics;
   int separated;
   int doubled;
   int double_bottom; // Set for the bottom line of doubled text, clear for the top line
   int flashing;
   int concealed;
   int held;
   int held_char;
   int held_separated;

   // Display parameters
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

   // A local copy of the screen
   uint8_t mode7screen[ROWS][COLUMNS];

   // Counts of the number of double-height control codes in each
   unsigned int dh_count[ROWS];

} tt = {

   // Current line state
   .fgd_colour     = TT_WHITE,
   .bgd_colour     = TT_BLACK,
   .graphics       = FALSE,
   .separated      = FALSE,
   .doubled        = FALSE,
   .double_bottom  = TRUE,
   .flashing       = FALSE,
   .concealed      = FALSE,
   .held           = FALSE,
   .held_char      = 32,
   .held_separated = FALSE,

   // Display parameters
   .columns        = COLUMNS,
   .rows           = ROWS,
   .xstart         = 0,
   .ystart         = 0,
   .char_w         = 6,
   .char_h         = 10,
   .size_w         = 15,
   .size_h         = 20,
   .scale_w        = 1,
   .scale_h        = 1
};

// Arbitrary limit on the number of flash regions
#define TT_MAX_FLASH_REGION 25

struct{
   int fgd_colour;
   int bgd_colour;
   int row;
   int column;
   int length;
   int chars[COLUMNS];
} tt_flash_region [TT_MAX_FLASH_REGION];
int tt_no_flash_region = 0;

// Screen Mode definition
static void tt_reset          (screen_mode_t *screen);
static void tt_clear          (screen_mode_t *screen, t_clip_window_t *text_window, pixel_t bg_col);
static void tt_scroll         (screen_mode_t *screen, t_clip_window_t *text_window, pixel_t bg_col);
static void tt_flash          (screen_mode_t *screen);
static void tt_write_character(screen_mode_t *screen, int c, int col, int row, pixel_t fg_col, pixel_t bg_col);
static int  tt_read_character (screen_mode_t *screen, int col, int row);

// #define LORES

static screen_mode_t teletext_screen_mode = {
   .mode_num        = 7,
#ifdef LORES
   .width           = 480,
   .height          = 500,
#else
   .width           = 1200,
   .height          = 1000,
#endif
   .xeigfactor      = 1,
   .yeigfactor      = 1,
   .bpp             = 8,
   .ncolour         = 255,
   .reset           = tt_reset,
   .clear           = tt_clear,
   .scroll          = tt_scroll,
   .flash           = tt_flash,
   .write_character = tt_write_character,
   .read_character  = tt_read_character
};

screen_mode_t *tt_get_screen_mode() {
   // This screen mode always uses the SAA5050 font
   teletext_screen_mode.font = get_font_by_name("SAA5050");
   // Note: these metrics give a 40 x 25 display
#ifdef LORES
   teletext_screen_mode.font->scale_w = 2;
   teletext_screen_mode.font->scale_h = 2;
#else
   teletext_screen_mode.font->scale_w = 5;
   teletext_screen_mode.font->scale_h = 4;
#endif
   return &teletext_screen_mode;
}


// This is called on initialization, and VDU 20 to set the default palette

// Set up MODE 7
static void tt_reset(screen_mode_t *screen) {
   // TODO: Fix hardcoded scale factors
   //
   // The below code works as follows:
   //
   // - The SAA5050 font is 6x10
   //
   // - tt_put_block renders each font pixel as a 2 x 2 block of cells, with the following sizes:
   //   - TL cell = 2 px x tt.scale_h px
   //   - TR cell = 3 px x tt.scale_h px
   //   - BL cell = 2 px x tt.scale_h px
   //   - BR cell = 3 px x tt.scale_h px
   //   - (i.e. a total of 5 pixels horizontally and 2 * h_scale pixels vertically)
   //
   // - So he display ends up at:
   //   - 40 *  6 * (5          ) = 1200
   //   - 25 * 10 * (2 * h_scale) = 1000
   //
   // - And each character ends up 30 * 40 px
   //
   // It would be nice to allow the frame buffer to be 480x500, and then rely on the Pi to
   // scale this up to the physical display, as it has a choice of several scaling kernels.
   //
#ifdef LORES
   tt.size_w = tt.char_w * 4 / 2;        //        12
#else
   tt.size_w = tt.char_w * 5 / 2;        // 15
#endif
   tt.size_h = tt.char_h * 2;            // 20
   int width = tt.columns * tt.size_w;   // 600 / 480
   int height = tt.rows * tt.size_h;     // 500 / 500
   tt.scale_w = screen->width / width;   // 2   /   1
   tt.scale_h = screen->height / height; // 2   /   1
   tt.xstart = 0;
   tt.ystart = screen->height - 1;
   //fb_set_char_metrics(tt.size_w, tt.size_h, tt.scale_w, tt.scale_h);
   //fb_reset_areas();
   tt_no_flash_region = 0;
   screen->set_colour(screen, TT_BLACK,     0x00, 0x00, 0x00);
   screen->set_colour(screen, TT_RED,       0xff, 0x00, 0x00);
   screen->set_colour(screen, TT_GREEN,     0x00, 0xff, 0x00);
   screen->set_colour(screen, TT_YELLOW,    0xff, 0xff, 0x00);
   screen->set_colour(screen, TT_BLUE,      0x00, 0x00, 0xff);
   screen->set_colour(screen, TT_MAGENTA,   0xff, 0x00, 0xff);
   screen->set_colour(screen, TT_CYAN,      0x00, 0xff, 0xff);
   screen->set_colour(screen, TT_WHITE,     0xff, 0xff, 0xff);
}

static void update_double_height_counts() {
   unsigned int *count = tt.dh_count;
   for (int row = 0; row < tt.rows; row++) {
      *count = 0;
      for (int col = 0; col < tt.columns; col++) {
         if (tt.mode7screen[row][col] == TT_DOUBLE) {
            (*count)++;
         }
      }
      count++;
   }
}
static void tt_clear(screen_mode_t *screen, t_clip_window_t *text_window, pixel_t bg_col) {
   // Call the default implementation to clear the framebuffer
   default_clear_screen(screen, text_window, bg_col);
   // Clear the backing store
   if (text_window == NULL) {
      memset(tt.mode7screen, 32, sizeof(tt.mode7screen));
   } else {
      for (int row = text_window->top; row <= text_window->bottom; row++) {
         for (int col = text_window->left; col <= text_window->right; col++) {
            tt.mode7screen[row][col] = 32;
         }
      }
   }
   // Recalculate the double height counts
   update_double_height_counts();
}

static void tt_scroll(screen_mode_t *screen, t_clip_window_t *text_window, pixel_t bg_col) {
   // Call the default implementation to scroll the framebuffer
   default_scroll_screen(screen, text_window, bg_col);
   // Scroll the backing store
   for (int row = text_window->top; row < text_window->bottom; row++) {
      for (int col = text_window->left; col <= text_window->right; col++) {
         tt.mode7screen[row][col] = tt.mode7screen[row + 1][col];
      }
   }
   for (int col = text_window->left; col <= text_window->right; col++) {
      tt.mode7screen[text_window->bottom][col] = 32;
   }
   // Recalculate the double height counts
   update_double_height_counts();
}


static int tt_read_character(screen_mode_t *screen, int col, int row) {
   return tt.mode7screen[row][col];
}

static void tt_reset_line_state(int row) {
   // Reset the state at the beginning of each line
   tt.fgd_colour = TT_WHITE;
   tt.bgd_colour = TT_BLACK;
   tt.graphics = FALSE;
   tt.separated = FALSE;
   tt.doubled = FALSE;
   tt.flashing = FALSE;
   tt.concealed = FALSE;
   tt.held = FALSE;
   tt.held_char = 32;
   tt.held_separated = FALSE;
   // The bottom row of double height is only selected if the number of consecutive
   // preceeding rows that contain the double height control codes is odd
   // This attribute also causes normal height stuff on the bottom row of double height
   // to be supressed (i.e. displayed as spaces in the current background colour).
   tt.double_bottom = FALSE;
   for (int r = row - 1; r >= 0 && tt.dh_count[r] > 0; r--) {
      tt.double_bottom = !tt.double_bottom;
   }
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
            screen->write_character(screen, c, col, row, tt.fgd_colour, tt.bgd_colour);
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
   if (tt.flashing) {
      tt_add_flash_char(c);
   }
   switch (c) {
   case TT_CONCEAL:
      tt.concealed = TRUE;
      break;
   case TT_CONTIGUOUS:
      tt.separated = FALSE;
      break;
   case TT_SEPARATED:
      tt.separated = TRUE;
      break;
   case TT_BLACK_BGD:
      tt.bgd_colour = TT_BLACK;
      break;
   case TT_NEW_BGD:
      tt.bgd_colour = tt.fgd_colour;
      break;
   case TT_HOLD:
      tt.held = TRUE;
      break;
   }

   if (tt.graphics && (c & 0x20)) {
      // The held graphics character is the last character seen with bit 5 set in graphics mode
      tt.held_char = c;
      tt.held_separated = tt.separated;
   } else if (c >= 128 && c <= 159 && !tt.held) {
      // Control codes when hold inactive will reset the held graphics character
      tt.held_char = 32;
   } else if ((c == TT_NORMAL && tt.doubled) || (c == TT_DOUBLE && !tt.doubled)) {
      // A change of height will reset the held graphics character, even if hold active
      tt.held_char = 32;
   }

   // return the character that will actually be rendered with draw_character

   // HOLD, CONCEAL, DOUBLED are "Set At" so this decision has to be made after the above case statement

   if (tt.concealed || (tt.double_bottom && !tt.doubled)) {
      // Anything normal height on the bottom row of double height should be displayed as a space
      return 32;
   } else if (c  >= 128 && c <= 159) {
      if (tt.held) {
         // Display control codes as the held character
         return tt.held_char;
      } else {
         // Display control codes as space (the default behaviour)
         return 32;
      }
   } else {
      // Display the character passed in
      return c;
   }
}

static void tt_process_controls_after(int c, int col, int row) {
   switch(c) {
   case TT_A_RED ... TT_A_WHITE:
      tt.graphics = FALSE;
      tt.concealed = FALSE;
      tt.fgd_colour = c - 128;
      // A change from graphics back to text, even when held, will reset the held character
      tt.held_char = 32;
      break;
   case TT_G_RED ... TT_G_WHITE:
      tt.graphics = TRUE;
      tt.concealed = FALSE;
      tt.fgd_colour = c - 144;
      break;
   case TT_FLASH:
      tt.flashing = TRUE;
      tt_start_flash_area(col + 1, row);
      break;
   case TT_STEADY:
      tt.flashing = FALSE;
      break;
   case TT_NORMAL:
      tt.doubled = FALSE;
      break;
   case TT_DOUBLE:
      tt.doubled = TRUE;
      break;
   case TT_RELEASE:
      // Release (and start of line) are the only things that cleat the hold flag
      tt.held = FALSE;
      // Release also resets the held mosiac to back to space
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
            // TODO: Fix hardcoded scale factors
#ifdef LORES
            screen->set_pixel(screen, x + x2, y - y2 * yscale - ys, colour);
#else
            int xscale = (x2 == 0) ? 2 : 3;
            for (int xs = 0; xs < xscale; xs++) {
               screen->set_pixel(screen, x + x2 * 2 + xs, y - y2 * yscale - ys, colour);
            }
#endif
         }
      }
   }
}

static inline int tt_pixel_set(int p, int x, int y) {
   // Tests whether the given pixel is set to foreground colour
   return ((teletext_screen_mode.font->buffer[p + y] >> (tt.char_w - x - 1)) & 1);
}


// Redraw character c at col, row using the current line state
static void tt_draw_character(struct screen_mode *screen, int ch, int col, int row) {
   int colour[2];
   int xoffset = tt.xstart + col * tt.size_w * tt.scale_w;
   int yoffset = tt.ystart - row * tt.size_h * tt.scale_h;

   if (tt.graphics && (ch & 0x3f) >= 0x20) {

      // Draws the graphics characters based on the 2x6 matrix

      if (ch & 64) {
         ch |= 0x20; // Set bit 5 if the 6th block is set
      } else {
         ch &= 0x1f;
      }

      // Use the held value of separated during hold mode
      int separated = tt.held ? tt.held_separated : tt.separated;

      // Which of the three vertical blocks to start and end with
      int ybstart = (tt.doubled &&  tt.double_bottom) ? 1 : 0;
      int ybend   = (tt.doubled && !tt.double_bottom) ? 1 : 2;

      // The width of a block is half a character (in pixels)
      int xbsize  = tt.scale_w * tt.size_w / 2;

      // The width of the graphics seperator, if needed
      int xbsep   =  xbsize / 3;

      // The height of a top/bottom block is one third of a character (rounded down)
      int ybsize0 = tt.scale_h * (tt.size_h / 3) * (tt.doubled ? 2 : 1);

      // The height of a middle block is whatever is remaining (in pixels)
      int ybsize1 = tt.size_h * tt.scale_h - (tt.doubled ? 1 : 2) * ybsize0;

      // The height of horizontal the graphics seperator, if needed
      int ybsep   =  ybsize0 / 3;

      // Iterate over the Y blocks
      int y = yoffset;
      for (int yb = ybstart; yb <= ybend; yb++) {
         // If the top cell of double hight and block 1, remove the seperator
         if (tt.doubled && !tt.double_bottom && yb == 1) {
            ybsep = 0;
         }
         // Deterine the colour of the block the next two horizontal blocks
         colour[0] = ((ch >> (2 * yb    )) & 1) ? tt.fgd_colour : tt.bgd_colour;
         colour[1] = ((ch >> (2 * yb + 1)) & 1) ? tt.fgd_colour : tt.bgd_colour;
         // Iterate over the Y pixels in a block
         int ybsize = (yb == 1) ? ybsize1 : ybsize0;
         for (int y2 = 0; y2 < ybsize ; y2++, y--) {
            // Iterate over the X blocks
            int x = xoffset;
            for (int xb = 0; xb < 2; xb++) {
               // Iterate over the X pixels in a block
               for (int x2 = 0; x2 < xbsize; x2++, x++) {
                  if (separated && (x2 < xbsep || y2 >= ybsize - ybsep)) {
                     // Seperated graphics
                     screen->set_pixel(screen, x, y, tt.bgd_colour);
                  } else {
                     // Normal graphics
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
         if (tt.double_bottom) {
            py_from = tt.char_h / 2;
         } else {
            py_to = py_to / 2;
         }
      }
      for (int py = py_from; py < py_to; py++, y -= yinc) {
         int x = xoffset;
         // TODO: Fix hardcoded scale factors
#ifdef LORES
         for (int px = 0; px < tt.char_w; px++, x += (tt.scale_w * 2)) {
#else
         for (int px = 0; px < tt.char_w; px++, x += (tt.scale_w * 5 / 2)) {
#endif
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
}

static void re_render_row(screen_mode_t *screen, int col, int row) {
   for (; col < tt.columns; col++) {
      uint8_t tmpc = tt.mode7screen[row][col];
      uint8_t renderc = tt_process_controls(tmpc, col, row);
      tt_draw_character(screen, renderc, col, row);
      tt_process_controls_after(tmpc, col, row);
   }
}

static void tt_write_character(screen_mode_t *screen, int c, int col, int row, pixel_t fg_col, pixel_t bg_col) {

   // Note: fg_col/bg_col (from COLOUR n) are ignored in teletext mode
   // because colour control characters are used insread

   static int last_row = -1;
   static int last_col = -1;

   // Remap some codes to accomodate differences between the
   // Beeb's character set and the SAA5050 Character ROM
   // (this is also done by Beeb OS VDU driver)
   //
   //     Beeb   SAA5050
   // 35: #      £
   // 95: _      #
   // 96: £      _
   if (c == 35) {
      c = 95;
   } else if (c == 95) {
      c = 96;
   } else if (c == 96) {
      c = 35;
   }

   // Detect non-linear accesses, and reconstruct the line state
   if (row != last_row || col != last_col + 1) {
      tt_reset_line_state(row);
      for (int i = 0; i < col; i++) {
         uint8_t tmpc = tt.mode7screen[row][i];
         tt_process_controls(tmpc, i, row);
         tt_process_controls_after(tmpc, i, row);
      }
   }

   // Render the current character
   uint8_t renderc = tt_process_controls(c, col, row);
   tt_draw_character(screen, renderc, col, row);
   tt_process_controls_after(c, col, row);

   // Update the backing store
   int oldc = tt.mode7screen[row][col];
   tt.mode7screen[row][col] = c;

   // Test if the old/new characters is a control character
   int is_cc1 = (oldc & 0xE0) == 0x80; // true if the old character at row,col was a control code
   int is_cc2 = (c    & 0xE0) == 0x80; // true if the new character at row,col was a control code

   // If either is a control character, then it gets more involved
   if ((c != oldc) && (is_cc1 || is_cc2)) {

      // Update the double height counts
      int old_dh_state = tt.dh_count[row] ? TRUE : FALSE;
      if (c == TT_DOUBLE) {
         tt.dh_count[row]++;
      }
      if (oldc == TT_DOUBLE) {
         tt.dh_count[row]--;
      }
      int new_dh_state = tt.dh_count[row] ? TRUE : FALSE;

      // Re-render the rest of the current row
      re_render_row(screen, col + 1, row);

      // If the double height state has changed then re-render additional row
      // only stopping when we re-render one without any double height codes,
      // or when we run out of rows
      if (new_dh_state != old_dh_state) {
         while (++row < tt.rows) {
            tt_reset_line_state(row);
            re_render_row(screen, 0, row);
            if (tt.dh_count[row] == 0) {
               break;
            }
         }
      }

      // Invalidate the current line state
      row = -1;
      col = -1;
   }

   // Remember the current row, col
   last_row = row;
   last_col = col;
}
