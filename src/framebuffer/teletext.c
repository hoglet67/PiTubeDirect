// Teletext support for MODE 7
// Original version by Rod Thomas January 2021

// Significant additions by Hoglet February 2021
// - Fix errors in SAA5050 font
// - Render to a 480x500 frame buffer and use GPU for scaling
// - Correctly implement Set At/Set After semantics for all control codes
// - Fix graphics hold issues
// - Replicate SAA5050 graphics hold bug
// - Characters 35,95 and 96 swapped (as is done by the OS's VDU handler)
// - Support copy-based editing
// - Support out-of-order character writes (with selective re-rendering)
// - Correct rendering of double height graphics
// - Flashing regions re-implemented using Palette changes only
// - Conceal/Reveal control via VDU 23,18,2
// - Added modes 70 (60x25) and 71 (80x25)
// - Move character rounding to the font code, so it could be used in other modes
// - Added graphics characters to the SAA fonts to simplify things

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <inttypes.h>

#include "framebuffer.h"
#include "teletext.h"
#include "screen_modes.h"
#include "fonts.h"

// These are the maxium size of screen mode that we support
#define MAX_COLUMNS 80
#define MAX_ROWS    32

// Main structure holding Teletext state
struct {

   // Current line state
   pixel_t fgd_colour;
   pixel_t bgd_colour;
   int graphics;
   int separated;
   int doubled;
   int double_bottom; // Set for the bottom line of doubled text, clear for the top line
   int flashing;
   int concealed;
   int held;
   int held_char;
   int held_separated;

   // Working state
   int last_row;
   int last_col;

   // Teletext options
   int reveal; // reveal concealed text

   // Number of rows and columns of text on the screen
   int columns;
   int rows;

   // A local copy of the screen
   uint8_t mode7screen[MAX_ROWS][MAX_COLUMNS];

   // Counts of the number of double-height control codes in each
   unsigned int dh_count[MAX_ROWS];

} tt;

// Screen Mode Handlers
static void tt_reset          (screen_mode_t *screen);
static void tt_clear          (screen_mode_t *screen, t_clip_window_t *text_window, pixel_t bg_col);
static void tt_scroll         (screen_mode_t *screen, t_clip_window_t *text_window, pixel_t bg_col);
static void tt_flash          (screen_mode_t *screen);
static void tt_write_character(screen_mode_t *screen, int c, int col, int row, pixel_t fg_col, pixel_t bg_col);
static int  tt_read_character (screen_mode_t *screen, int col, int row);
static void tt_unknown_vdu    (screen_mode_t *screen, uint8_t *buf);

// Screen Mode Definition
static screen_mode_t teletext_screen_modes[] = {
   // Standard 40x25 teletext display
   {
      .mode_num        = 7,
      .width           = 480,
      .height          = 500,
      .xeigfactor      = 1,
      .yeigfactor      = 1,
      .bpp             = 8,
      .ncolour         = 255,
      .par             = 1.25f, // 5:4
      .reset           = tt_reset,
      .clear           = tt_clear,
      .scroll          = tt_scroll,
      .flash           = tt_flash,
      .write_character = tt_write_character,
      .read_character  = tt_read_character,
      .unknown_vdu     = tt_unknown_vdu
   },
   // Wide 60x25 teletext display
   {
      .mode_num        = 70,
      .width           = 720,
      .height          = 500,
      .xeigfactor      = 1,
      .yeigfactor      = 1,
      .bpp             = 8,
      .ncolour         = 255,
      .par             = 1.25f, // 5:4
      .reset           = tt_reset,
      .clear           = tt_clear,
      .scroll          = tt_scroll,
      .flash           = tt_flash,
      .write_character = tt_write_character,
      .read_character  = tt_read_character,
      .unknown_vdu     = tt_unknown_vdu
   },
   // Wide 80x25 teletext display
   {
      .mode_num        = 71,
      .width           = 960,
      .height          = 500,
      .xeigfactor      = 1,
      .yeigfactor      = 1,
      .bpp             = 8,
      .ncolour         = 255,
      .par             = 1.00f, // 1:1
      .reset           = tt_reset,
      .clear           = tt_clear,
      .scroll          = tt_scroll,
      .flash           = tt_flash,
      .write_character = tt_write_character,
      .read_character  = tt_read_character,
      .unknown_vdu     = tt_unknown_vdu
   },
   {
      .mode_num        = -1
   }
};

static void set_font(screen_mode_t *screen, int num) {
   // This screen mode always uses the SAA505x family of fonts
   char name[] = "SAA5050";
   name[6] += (num & 7);
   font_t *font = get_font_by_name(name);
   font->set_rounding(font, TRUE);
   screen->font = font;
}

screen_mode_t *tt_get_screen_mode(int mode_num) {
   screen_mode_t *sm = NULL;
   screen_mode_t *tmp = teletext_screen_modes;
   while (tmp->mode_num >= 0) {
      if (tmp->mode_num == mode_num) {
         sm = tmp;
         break;
      }
      tmp++;
   }
   if (sm != NULL) {
      set_font(sm, 0);
   }
   return sm;
}

static inline int is_control(int c) {
   return !(c & 0x60);
}

static inline int is_graphics(int c) {
   return (c & 0x20);
}

static inline int is_double(int c) {
   return (c & 0x7f) == TT_DOUBLE;
}

static inline int is_normal(int c) {
   return (c & 0x7f) == TT_NORMAL;
}

static void set_palette(screen_mode_t *screen, int mark) {
   // Setup colour palatte
   // Bits 5..3 control the space colour
   // Bits 2..0 control the mark colour
   //
   // Examples:
   // - solid blue would be 100100
   // - red/green flashing would be 001010
   //
   for (int i = 0; i < 0x40; i++) {
      // Depending on whether mark is true
      int colour = (mark ? i : (i >> 3)) & 0x07;
      int red   = (colour & 1) ? 0xff : 0x00;
      int green = (colour & 2) ? 0xff : 0x00;
      int blue  = (colour & 4) ? 0xff : 0x00;
      screen->set_colour(screen, i, red, green, blue);
   }
   screen->update_palette(screen, 0, 0x40);
}

static void set_foreground(tt_colour_t colour) {
   colour &= 0x07;
   if (tt.flashing) {
      tt.fgd_colour = (tt.bgd_colour & 0x38) | colour;
   } else {
      tt.fgd_colour = (colour << 3) | colour;
   }
}

static void set_background(tt_colour_t colour) {
   colour &= 0x07;
   tt.bgd_colour = (colour << 3) | colour;
   if (tt.flashing) {
      tt.fgd_colour = (colour << 3) | (tt.fgd_colour & 0x07);
   }
}

static void set_flashing(int on) {
   if (on) {
      tt.fgd_colour = (tt.bgd_colour & 0x38) | (tt.fgd_colour & 0x07);
   } else {
      tt.fgd_colour = ((tt.fgd_colour & 0x07) << 3) | (tt.fgd_colour & 0x07);
   }
   tt.flashing = on;
}

// This is called on initialization, on mode change, and VDU 20
// It sets the default palette, and resets the default display options
static void tt_reset(screen_mode_t *screen) {
   font_t *font = screen->font;
   // Set the rows/columns based on the the font
   tt.columns = screen->width / font->get_overall_w(font);
   tt.rows = screen->height / font->get_overall_h(font);
   // Reset the rendering params to sensible defaults
   tt.last_row = -1;
   tt.last_col = -1;
   tt.reveal = 0;
   // Configure the default palette
   set_palette(screen, FALSE);
   // Initialize the font
   set_font(screen, 0);
}

static void update_double_height_counts() {
   unsigned int *count = tt.dh_count;
   for (int row = 0; row < tt.rows; row++) {
      *count = 0;
      for (int col = 0; col < tt.columns; col++) {
         if (is_double(tt.mode7screen[row][col])) {
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
      memset(tt.mode7screen, TT_SPACE, sizeof(tt.mode7screen));
   } else {
      for (int row = text_window->top; row <= text_window->bottom; row++) {
         for (int col = text_window->left; col <= text_window->right; col++) {
            tt.mode7screen[row][col] = TT_SPACE;
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
      tt.mode7screen[text_window->bottom][col] = TT_SPACE;
   }
   // Recalculate the double height counts
   update_double_height_counts();
}


static int tt_read_character(screen_mode_t *screen, int col, int row) {
   int c = tt.mode7screen[row][col];

   // Reverse the mapping done in tt_write_character
   if (c == 35) {
      c = 96;
   } else if (c == 95) {
      c = 35;
   } else if (c == 96) {
      c = 95;
   }

   return c;
}

static void tt_reset_line_state(int row) {
   // Reset the state at the beginning of each line
   set_background(TT_BLACK);
   set_foreground(TT_WHITE);
   tt.graphics = FALSE;
   tt.separated = FALSE;
   tt.doubled = FALSE;
   set_flashing(FALSE);
   tt.concealed = FALSE;
   tt.held = FALSE;
   tt.held_char = TT_SPACE;
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

// Make the flash regions flash with a 3:1 mark:space ratio
// This is called at 3.125Hz, so the flash rate is 0.78125Hz
static void tt_flash(screen_mode_t *screen) {
   static int count = 0;
   if (count < 2) {
      set_palette(screen, count & 1);
   }
   count = (count + 1) & 3;
}

// Process control characters that are "Set At"
static int tt_process_controls(int c, int col, int row) {
   switch (c & 0x7F) {
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
      set_background(TT_BLACK);
      break;
   case TT_NEW_BGD:
      set_background(tt.fgd_colour);
      break;
   case TT_HOLD:
      tt.held = TRUE;
      break;
   }

   if (is_graphics(c) && tt.graphics) {
      // The held graphics character is the last character seen with bit 5 set in graphics mode
      tt.held_char = c;
      tt.held_separated = tt.separated;
   } else if (is_control(c) && !tt.held) {
      // Control codes when hold inactive will reset the held graphics character
      tt.held_char = TT_SPACE;
   } else if ((is_normal(c) && tt.doubled) || (is_double(c) && !tt.doubled)) {
      // A change of height will reset the held graphics character, even if hold active
      tt.held_char = TT_SPACE;
   }

   // return the character that will actually be rendered with draw_character

   // HOLD, CONCEAL, DOUBLED are "Set At" so this decision has to be made after the above case statement

   if ((tt.concealed && !tt.reveal) || (tt.double_bottom && !tt.doubled)) {
      // Anything normal height on the bottom row of double height should be displayed as a space
      return TT_SPACE;
   } else if (is_control(c)) {
      if (tt.held) {
         // Display control codes as the held character
         return tt.held_char;
      } else {
         // Display control codes as space (the default behaviour)
         return TT_SPACE;
      }
   } else {
      // Display the character passed in
      return c;
   }
}

// Process control characters that are "Set After"
static void tt_process_controls_after(int c, int col, int row) {
   switch(c & 0x7f) {
   case TT_A_RED ... TT_A_WHITE:
      tt.graphics = FALSE;
      tt.concealed = FALSE;
      set_foreground(c - TT_A_BLACK);
      // A change from graphics back to text, even when held, will reset the held character
      tt.held_char = TT_SPACE;
      break;
   case TT_G_RED ... TT_G_WHITE:
      tt.graphics = TRUE;
      tt.concealed = FALSE;
      set_foreground(c - TT_G_BLACK);
      break;
   case TT_FLASH:
      set_flashing(TRUE);
      break;
   case TT_STEADY:
      set_flashing(FALSE);
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
      tt.held_char = TT_SPACE;
      break;
   }
}

// Redraw character c at col, row using the current line state
static void tt_draw_character(screen_mode_t *screen, int c, int col, int row) {
   font_t *font = screen->font;

   int xoffset = col * font->get_overall_w(font);
   int yoffset = screen->height - row * font->get_overall_h(font) - 1;

   if (tt.graphics && is_graphics(c)) {
      // Use the held value of separated during hold mode
      int separated = tt.held ? tt.held_separated : tt.separated;
      // Copy bit 6 to bit 5 so the graphic block bits are in bits 5..0
      if (c & 0x40) {
         c |= 0x20;
      } else {
         c &= 0x1F;
      }
      // Copy seperated flag to bit 6
      if (separated) {
         c |= 0x40;
      } else {
         c &= 0x3F;
      }
      // Set bit 7 to select the graphics character from the character ROM
      c |= 0x80;

   } else {
      // Clear bit 7 to select the text characters from the character ROM
      c &= 0x7f;
   }

   if (tt.doubled) {
      // Use a custom font renderer to render double height
      int width  = font->width << font->get_rounding(font);
      int height = font->height << font->get_rounding(font);
      uint16_t *rowp = font->buffer + c * height + (tt.double_bottom ? (height >> 1) : 0);
      for (int y = 0; y < height; y++) {
         uint16_t row = *rowp;
         int mask = 1 << (width - 1);
         for (int x = 0; x < width; x++) {
            screen->set_pixel(screen, xoffset + x, yoffset - y, (row & mask) ? tt.fgd_colour : tt.bgd_colour);
            mask >>= 1;
         }
         if (y & 1) {
            rowp++;
         }
      }
   } else {
      // Use the standard font renderer to render normal height
      font->write_char(font, screen, c, xoffset, yoffset, tt.fgd_colour, tt.bgd_colour);
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
   if (row != tt.last_row || col != tt.last_col + 1) {
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

   // If the either the old or new character is a control character, then it gets more involved
   if ((c != oldc) && (is_control(c) || is_control(oldc))) {

      // Update the double height counts
      int old_dh_state = tt.dh_count[row] ? TRUE : FALSE;
      if (is_double(c)) {
         tt.dh_count[row]++;
      }
      if (is_double(oldc)) {
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
   tt.last_row = row;
   tt.last_col = col;
}

static void set_reveal(screen_mode_t *screen, int mode, int mask) {
   int reveal = ((tt.reveal & mask) ^ mode) & 1;
   if (reveal != tt.reveal) {
      // Update the reveal flag
      tt.reveal = reveal;
      // Re-render the screen
      for (int row = 0; row < tt.rows; row++) {
         tt_reset_line_state(row);
         re_render_row(screen, 0, row);
      }
      // Invalidate the current line state
      tt.last_row = -1;
      tt.last_col = -1;
   }
}

static void tt_unknown_vdu(screen_mode_t *screen, uint8_t *buf) {
   if (buf[0] == 23 && buf[1] == 18) {
      switch (buf[2]) {
      case 2:
         set_reveal(screen, buf[3], buf[4]);
         break;
      case 4:
         set_font(screen, buf[3]);
         break;
      }
   }
}
