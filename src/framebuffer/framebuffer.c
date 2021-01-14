#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include <math.h>

#include "../info.h"
#include "../rpi-armtimer.h"
#include "../rpi-aux.h"
#include "../rpi-gpio.h"
#include "../rpi-interrupts.h"
#include "../startup.h"
#include "../tube-defs.h"

#include "screen_modes.h"
#include "framebuffer.h"
#include "primitives.h"
#include "v3d.h"
#include "fonts.h"
#include "vdu23.h"
#include "vdu_state.h"

// Default screen mode
#define DEFAULT_SCREEN_MODE 8

// Default colours - TODO this should be dynamic
#define COL_BLACK    0
#define COL_WHITE   15

// Current screen mode
static screen_mode_t *screen = NULL;

// Current font
static font_t *font = NULL;
static int16_t font_width;
static int16_t font_height;
static int16_t text_width;
static int16_t text_height;

// Character colour / cursor position
static int16_t c_bg_col;
static int16_t c_fg_col;
static int16_t c_x_pos;
static int16_t c_y_pos;

static int16_t e_enabled = 0;
static int16_t e_x_pos;
static int16_t e_y_pos;

// Graphics colour / cursor position
static int16_t g_bg_col;
static int16_t g_fg_col;
static int16_t g_x_pos;
static int16_t g_x_pos_last1;
static int16_t g_x_pos_last2;
static int16_t g_y_pos;
static int16_t g_y_pos_last1;
static int16_t g_y_pos_last2;
static int16_t g_mode;

// Text or graphical cursor for printing characters
static int8_t g_cursor;

static int e_visible; // Current visibility of the edit cursor
static int c_visible; // Current visibility of the write cursor

// VDU Queue
#define VDU_QSIZE 8192
static volatile int vdu_wp = 0;
static volatile int vdu_rp = 0;
static char vdu_queue[VDU_QSIZE];

// ==========================================================================
// Static methods
// ==========================================================================

static void calc_text_area() {
   // Calculate the text params
   if (screen && font) {
      font_width  = font->width * font->scale_w + font->spacing;
      font_height = font->height * font->scale_h + font->spacing;
      text_width  = screen->width / font_width;
      text_height = screen->height / font_height;
   }
}

static void fb_init_variables() {

    // Character colour / cursor position
   c_bg_col  = COL_BLACK;
   c_fg_col  = COL_WHITE;
   c_x_pos   = 0;
   c_y_pos   = 0;
   c_visible = 0;

   // Edit cursor
   e_x_pos   = 0;
   e_y_pos   = 0;
   e_visible = 0;
   e_enabled = 0;

   // Graphics colour / cursor position
   g_bg_col      = COL_BLACK;
   g_fg_col      = COL_WHITE;
   g_x_pos       = 0;
   g_x_pos_last1 = 0;
   g_x_pos_last2 = 0;
   g_y_pos       = 0;
   g_y_pos_last1 = 0;
   g_y_pos_last2 = 0;
   g_mode        = 0;

   // Cursor mode
   g_cursor      = IN_VDU4;

   // Calculate the size of the text area
   calc_text_area();

   // Set the graphics origin to 0,0
   fb_set_graphics_origin(0, 0);
   fb_set_graphics_plotmode(0);
}

static void fb_invert_cursor(int x_pos, int y_pos, int editing) {
   int x = x_pos * font_width;
   int y = screen->height - y_pos * font_height - 1;
   int y1 = editing ? font_height - 2 : 0;
   for (int i = y1; i < font_height; i++) {
      for (int j = 0; j < font_width; j++) {
         int col = screen->get_pixel(screen, x + j, y - i);
         col ^= screen->get_colour(screen, COL_WHITE);
         screen->set_pixel(screen, x + j, y - i, col);
      }
   }
}

static void fb_show_cursor() {
   if (c_visible) {
      return;
   }
   fb_invert_cursor(c_x_pos, c_y_pos, 0);
   c_visible = 1;
}

static void fb_show_edit_cursor() {
   if (e_visible) {
      return;
   }
   fb_invert_cursor(e_x_pos, e_y_pos, 1);
   e_visible = 1;
}

static void fb_hide_cursor() {
   if (!c_visible) {
      return;
   }
   fb_invert_cursor(c_x_pos, c_y_pos, 0);
   c_visible = 0;
}

static void fb_hide_edit_cursor() {
   if (!e_visible) {
      return;
   }
   fb_invert_cursor(e_x_pos, e_y_pos, 1);
   e_visible = 0;
}

static void fb_enable_edit_cursor() {
   if (!e_enabled) {
      e_enabled = 1;
      e_x_pos = c_x_pos;
      e_y_pos = c_y_pos;
      fb_show_edit_cursor();
   }
}

static void fb_disable_edit_cursor() {
   if (e_enabled) {
      fb_hide_edit_cursor();
      e_x_pos = 0;
      e_y_pos = 0;
      e_enabled = 0;
   }
}

static void fb_cursor_interrupt() {
   if (e_enabled) {
      if (e_visible) {
         fb_hide_edit_cursor();
      } else {
         fb_show_edit_cursor();
      }
   }
}

static void fb_edit_cursor_up() {
   fb_enable_edit_cursor();
   fb_hide_edit_cursor();
   if (e_y_pos > 0) {
      e_y_pos--;
   } else {
      e_y_pos = text_height - 1;
   }
   fb_show_edit_cursor();
}

static void fb_edit_cursor_down() {
   fb_enable_edit_cursor();
   fb_hide_edit_cursor();
   if (e_y_pos < text_height - 1) {
      e_y_pos++;
   } else {
      e_y_pos = 0;
   }
   fb_show_edit_cursor();
}

static void fb_edit_cursor_left() {
   fb_enable_edit_cursor();
   fb_hide_edit_cursor();
   if (e_x_pos > 0) {
      e_x_pos--;
   } else {
      e_x_pos = text_width - 1;
      if (e_y_pos > 0) {
         e_y_pos--;
      } else {
         e_y_pos = text_height - 1;
      }
   }
   fb_show_edit_cursor();
}

static void fb_edit_cursor_right() {
   fb_enable_edit_cursor();
   fb_hide_edit_cursor();
   if (e_x_pos < text_width - 1) {
      e_x_pos++;
   } else {
      e_x_pos = 0;
      if (e_y_pos < text_height - 1) {
         e_y_pos++;
      } else {
         e_y_pos = 0;
      }
   }
   fb_show_edit_cursor();
}

static void fb_scroll() {
   if (e_enabled) {
      fb_hide_edit_cursor();
   }
   screen->scroll(screen, font_height);
   if (e_enabled) {
      if (e_y_pos > 0) {
         e_y_pos--;
      }
      fb_show_edit_cursor();
   }
}

static void fb_clear() {
   // initialize the cursor positions, origin, colours, etc.
   // TODO: clearing all of these may not be strictly correct
   fb_init_variables();
   // clear frame buffer
   screen->clear(screen, screen->get_colour(screen, c_bg_col));
   // Show the cursor
   fb_show_cursor();
}


static void fb_cursor_left() {
   fb_hide_cursor();
   if (c_x_pos > 0) {
      c_x_pos--;
   } else {
      c_x_pos = text_width - 1;
   }
   fb_show_cursor();
}

static void fb_cursor_right() {
   fb_hide_cursor();
   if (c_x_pos < text_width - 1) {
      c_x_pos++;
   } else {
      c_x_pos = 0;
   }
   fb_show_cursor();
}

static void fb_cursor_up() {
   fb_hide_cursor();
   if (c_y_pos > 0) {
      c_y_pos--;
   } else {
      c_y_pos = text_height - 1;
   }
   fb_show_cursor();
}

static void fb_cursor_down() {
   fb_hide_cursor();
   if (c_y_pos < text_height - 1) {
      c_y_pos++;
   } else {
      fb_scroll();
   }
   fb_show_cursor();
}

static void fb_cursor_col0() {
   fb_disable_edit_cursor();
   fb_hide_cursor();
   c_x_pos = 0;
   fb_show_cursor();
}

static void fb_cursor_home() {
   fb_hide_cursor();
   c_x_pos = 0;
   c_y_pos = 0;
}


static void fb_cursor_next() {
   fb_hide_cursor();
   if (c_x_pos < text_width - 1) {
      c_x_pos++;
   } else {
      c_x_pos = 0;
      fb_cursor_down();
   }
   fb_show_cursor();
}


static void update_g_cursors(int16_t x, int16_t y) {
   g_x_pos_last2 = g_x_pos_last1;
   g_x_pos_last1 = g_x_pos;
   g_x_pos       = x;
   g_y_pos_last2 = g_y_pos_last1;
   g_y_pos_last1 = g_y_pos;
   g_y_pos       = y;
}

// ==========================================================================
// Drawing Primitives
// ==========================================================================

static void fb_draw_character(int c, int invert) {
   int fg_col = screen->get_colour(screen, c_fg_col);
   int gb_col = screen->get_colour(screen, c_bg_col);
   int x = c_x_pos * font_width;
   // Pixel 0,0 is in the bottom left
   // Character 0,0 is in the top left
   // So the Y axis needs flipping
   int y = screen->height - c_y_pos * font_height - 1;
   if (invert) {
      font->draw_character(font, screen, c, x, y, gb_col, fg_col);
   } else {
      font->draw_character(font, screen, c, x, y, fg_col, gb_col);
   }
}

static void fb_draw_character_and_advance(int c) {
   int invert = 0;
   if (font->num_chars <= 128) {
      invert = c >= 0x80;
      c &= 0x7f;
   }

   // Draw the next character at the cursor position
   fb_hide_cursor();
   fb_draw_character(c, invert);
   fb_show_cursor();

   // Advance the drawing position
   fb_cursor_next();
}

static int calc_radius(int x1, int y1, int x2, int y2) {
   return (int)(sqrt((x2-x1)*(x2-x1)+(y2-y1)*(y2-y1)) + 0.5);
}

// ==========================================================================
// Public interface
// ==========================================================================

void fb_initialize() {
   font = get_font_by_number(DEFAULT_FONT);

   if (!font) {
      printf("No font loaded\n\r");
   } else {
      printf("Font %s loaded\n\r", font->name);
   }

   fb_init_variables();

   fb_writec(22);
   fb_writec(DEFAULT_SCREEN_MODE);

   // TODO: This should be elsewhere
   // Initialize Timer Interrupts
   RPI_ArmTimerInit();
   RPI_GetIrqController()->Enable_Basic_IRQs = RPI_BASIC_ARM_TIMER_IRQ;

   fb_writes("\r\nPiTubeDirect VDU Driver\r\n");
#ifdef DEBUG_VDU
   fb_writes("Kernel debugging is enabled, execution might be slow!\r\n");
#endif
   fb_writes("\r\n");

}

void fb_writec_buffered(char ch) {
   // TODO: Deal with overflow
   vdu_queue[vdu_wp] = ch;
   vdu_wp = (vdu_wp + 1) & (VDU_QSIZE - 1);
}

void fb_process_vdu_queue() {
   static int cursor_count = 0;
   _data_memory_barrier();
   RPI_GetArmTimer()->IRQClear = 0;
   while (vdu_rp != vdu_wp) {
      char ch = vdu_queue[vdu_rp];
      fb_writec(ch);
      vdu_rp = (vdu_rp + 1) & (VDU_QSIZE - 1);
   }
   cursor_count++;
   if (cursor_count == 250) {
      fb_cursor_interrupt();
      cursor_count = 0;
   }
}

void fb_writec(char ch) {
   unsigned char c = (unsigned char) ch;

   static vdu_state_t state = NORMAL;
   static int count = 0;
   static int16_t x_tmp = 0;
   static int16_t y_tmp = 0;
   static int l; // logical colour
   static int p; // physical colour
   static int r;
   static int g;
   static int b;

   if (state == IN_VDU17) {
      state = NORMAL;
      if (c & 128) {
         c_bg_col = c & 127;
#ifdef DEBUG_VDU
         printf("bg = %d\r\n", c_bg_col);
#endif
      } else {
         c_fg_col = c & 127;
#ifdef DEBUG_VDU
         printf("fg = %d\r\n", c_fg_col);
#endif
      }
      return;

   } else if (state == IN_VDU18) {
      switch (count) {
      case 0:
         fb_set_graphics_plotmode(c);
         break;
      case 1:
         if (c & 128) {
            g_bg_col = c & 127;
         } else {
            g_fg_col = c & 127;
         }
         break;
      }
      count++;
      if (count == 2) {
         state = NORMAL;
      }
      return;

   } else if (state == IN_VDU19) {
      switch (count) {
      case 0:
         l = c;
         break;
      case 1:
         p = c;
         break;
      case 2:
         r = c;
         break;
      case 3:
         g = c;
         break;
      case 4:
         b = c;
         if (p == 255) {
            // init_colour_table();
            // update_palette(l, NUM_COLOURS);
         } else {
            // See http://beebwiki.mdfs.net/VDU_19
            if (p < 16) {
               int i = (p & 8) ? 255 : 127;
               b = (p & 4) ? i : 0;
               g = (p & 2) ? i : 0;
               r = (p & 1) ? i : 0;
            }
            screen->set_colour(screen, l, r, g, b);
         }
         state = NORMAL;
         break;
      }
      count++;
      return;

   } else if (state == IN_VDU22) {
      count++;
      if (count == 1) {
         state = NORMAL;
         screen_mode_t *new_screen = get_screen_mode(c);
         if (new_screen != NULL) {
            if (new_screen != screen) {
               screen = new_screen;
               screen->init(screen);
            }
            fb_clear();
         } else {
            fb_writes("Unsupported screen mode!\r\n");
         }
      }
      return;

   } else if (state == IN_VDU23) {
      // Pass to the vdu23 code (in vdu23.c)
      font_t *old_font = font;
      state = do_vdu23(screen, &font, c);
      if (font != old_font) {
         // Re-calculate the size of the text area
         calc_text_area();
      }
      return;

   } else if (state == IN_VDU25) {
      switch (count) {
      case 0:
         g_mode = c;
         break;
      case 1:
         x_tmp = c;
         break;
      case 2:
         x_tmp |= c << 8;
         break;
      case 3:
         y_tmp = c;
         break;
      case 4:
         y_tmp |= c << 8;
#ifdef DEBUG_VDU
         printf("plot %d %d %d\r\n", g_mode, x_tmp, y_tmp);
#endif

         if (g_mode & 4) {
            // Absolute position X, Y.
            update_g_cursors(x_tmp, y_tmp);
         } else {
            // Relative to the last point.
            update_g_cursors(g_x_pos + x_tmp, g_y_pos + y_tmp);
         }


         int col;
         switch (g_mode & 3) {
         case 0:
            col = -1;
            break;
         case 1:
            col = g_fg_col;
            break;
         case 2:
            col = 15 - g_fg_col;
            break;
         case 3:
            col = g_bg_col;
            break;
         }

         if (col >= 0) {

            pixel_t colour = screen->get_colour(screen, col);

            if (g_mode < 32) {
               // Draw a line
               fb_draw_line(screen, g_x_pos_last1, g_y_pos_last1, g_x_pos, g_y_pos, colour);
            } else if (g_mode >= 64 && g_mode < 72) {
               // Plot a single pixel
               fb_setpixel(screen, g_x_pos, g_y_pos, colour);
            } else if (g_mode >= 72 && g_mode < 80) {
               // Horizontal line fill (left and right) to non background
               fb_fill_area(screen, g_x_pos, g_y_pos, colour, HL_LR_NB);
            } else if (g_mode >= 80 && g_mode < 88) {
               // Fill a triangle
               fb_fill_triangle(screen, g_x_pos_last2, g_y_pos_last2, g_x_pos_last1, g_y_pos_last1, g_x_pos, g_y_pos, colour);
            } else if (g_mode >= 88 && g_mode < 96) {
               // Horizontal line fill (right only) to background
               fb_fill_area(screen, g_x_pos, g_y_pos, colour, HL_RO_BG);
            } else if (g_mode >= 96 && g_mode < 104) {
               // Fill a rectangle
               fb_fill_rectangle(screen, g_x_pos_last1, g_y_pos_last1, g_x_pos, g_y_pos, colour);
            } else if (g_mode >= 104 && g_mode < 112) {
               // Horizontal line fill (left and right) to foreground
               fb_fill_area(screen, g_x_pos, g_y_pos, colour, HL_LR_FG);
            } else if (g_mode >= 112 && g_mode < 120) {
               // Fill a parallelogram
               fb_fill_parallelogram(screen, g_x_pos_last2, g_y_pos_last2, g_x_pos_last1, g_y_pos_last1, g_x_pos, g_y_pos, colour);
            } else if (g_mode >= 120 && g_mode < 128) {
               // Horizontal line fill (right only) to non-foreground
               fb_fill_area(screen, g_x_pos, g_y_pos, colour, HL_RO_NF);
            } else if (g_mode >= 128 && g_mode < 136) {
               // Flood fill to non-background
               fb_fill_area(screen, g_x_pos, g_y_pos, colour, AF_NONBG);
            } else if (g_mode >= 136 && g_mode < 144) {
               // Flood fill to non-foreground
               fb_fill_area(screen, g_x_pos, g_y_pos, colour, AF_TOFGD);
            } else if (g_mode >= 144 && g_mode < 152) {
               // Draw a circle outline
               int radius = calc_radius(g_x_pos_last1, g_y_pos_last1, g_x_pos, g_y_pos);
               fb_draw_circle(screen, g_x_pos_last1, g_y_pos_last1, radius, colour);
            } else if (g_mode >= 152 && g_mode < 160) {
               // Fill a circle
               int radius = calc_radius(g_x_pos_last1, g_y_pos_last1, g_x_pos, g_y_pos);
               fb_fill_circle(screen, g_x_pos_last1, g_y_pos_last1, radius, colour);
            } else if (g_mode >= 160 && g_mode < 168) {
               // Draw a rectangle outline
               fb_draw_rectangle(screen, g_x_pos, g_y_pos, g_x_pos_last1, g_y_pos_last1, colour);
            } else if (g_mode >= 168 && g_mode < 176) {
               // Draw a parallelogram outline
               fb_draw_parallelogram(screen, g_x_pos, g_y_pos, g_x_pos_last1, g_y_pos_last1, g_x_pos_last2, g_y_pos_last2, colour);
            } else if (g_mode >= 176 && g_mode < 184) {
               // Draw a triangle outline
               fb_draw_triangle(screen, g_x_pos, g_y_pos, g_x_pos_last1, g_y_pos_last1, g_x_pos_last2, g_y_pos_last2, colour);
            } else if (g_mode >= 192 && g_mode < 200) {
               // Draw an ellipse
               fb_draw_ellipse(screen, g_x_pos_last2, g_y_pos_last2, abs(g_x_pos_last1 - g_x_pos_last2), abs(g_y_pos - g_y_pos_last2), colour);
            } else if (g_mode >= 200 && g_mode < 208) {
               // Fill a n ellipse
               fb_fill_ellipse(screen, g_x_pos_last2, g_y_pos_last2, abs(g_x_pos_last1 - g_x_pos_last2), abs(g_y_pos - g_y_pos_last2), colour);
            }
         }
      }
      count++;
      if (count == 5) {
         state = NORMAL;
      }
      return;

   } else if (state == IN_VDU27) {

      fb_draw_character_and_advance(c);
      state = NORMAL;
      return;

   } else if (state == IN_VDU29) {
      switch (count) {
      case 0:
         x_tmp = c;
         break;
      case 1:
         x_tmp |= c << 8;
         break;
      case 2:
         y_tmp = c;
         break;
      case 3:
         y_tmp |= c << 8;
         fb_set_graphics_origin(x_tmp, y_tmp);
#ifdef DEBUG_VDU
         printf("graphics origin %d %d\r\n", g_x_tmp, g_y_tmp);
#endif
      }
      count++;
      if (count == 4) {
         state = NORMAL;
      }
      return;

   } else if (state == IN_VDU31) {
      switch (count) {
      case 0:
         x_tmp = c;
         break;
      case 1:
         y_tmp = c;
         c_x_pos = x_tmp;
         c_y_pos = y_tmp;

#ifdef DEBUG_VDU
         printf("cursor move to %d %d\r\n", x_tmp, y_tmp);
#endif
      }
      count++;
      if (count == 2) {
         state = NORMAL;
      }
      return;

   }


   switch(c) {

   case 4:
      g_cursor = IN_VDU4;
      break;

   case 5:
      g_cursor = IN_VDU5;
      break;

   case 8:
      fb_cursor_left();
      break;

   case 9:
      fb_cursor_right();
      break;

   case 10:
      fb_cursor_down();
      break;

   case 11:
      fb_cursor_up();
      break;

   case 12:
      fb_clear();
      break;

   case 13:
      fb_cursor_col0();
      break;

   case 16:
      fb_clear();
      break;

   case 17:
      state = IN_VDU17;
      count = 0;
      return;

   case 18:
      state = IN_VDU18;
      count = 0;
      return;

   case 19:
      state = IN_VDU19;
      count = 0;
      return;

   case 22:
      state = IN_VDU22;
      count = 0;
      return;

   case 23:
      state = IN_VDU23;
      count = 0;
      return;

   case 25:
      state = IN_VDU25;
      count = 0;
      return;

   case 27:
      state = IN_VDU27;
      count = 0;
      return;

   case 29:
      state = IN_VDU29;
      count = 0;
      return;

   case 30:
      fb_cursor_home();
      break;

   case 31:
      state = IN_VDU31;
      count = 0;
      return;

   case 127:
      fb_cursor_left();
      fb_draw_character(32, 1);
      break;

   case 136:
      fb_edit_cursor_left();
      break;

   case 137:
      fb_edit_cursor_right();
      break;

   case 138:
      fb_edit_cursor_down();
      break;

   case 139:
      fb_edit_cursor_up();
      break;

   default:

      if (g_cursor == IN_VDU4) {
         fb_draw_character_and_advance(c);
      } else {
         font->draw_character(font, screen, c, g_x_pos, g_y_pos, g_fg_col, g_bg_col);
         update_g_cursors(g_x_pos + font_width, g_y_pos);
      }
   }
}

void fb_writes(char *string) {
   while (*string) {
      fb_writec(*string++);
   }
}

uint32_t fb_get_address() {
   return (uint32_t) fb;
}

int fb_get_edit_cursor_x() {
   return e_x_pos;
}

int fb_get_edit_cursor_y() {
   return e_y_pos;
}

int fb_get_edit_cursor_char() {
   int x = e_x_pos * font_width;
   int y = screen->height - e_y_pos * font_height - 1;
   return font->read_character(font, screen, x, y);
}

int16_t fb_get_g_bg_col() {
   return g_bg_col;
}

int16_t fb_get_g_fg_col() {
   return g_fg_col;
}
