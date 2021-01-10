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
#include "v3d.h"
#include "fonts.h"
#include "vdu23.h"
#include "vdu_state.h"

// Default screen mode
#define DEFAULT_SCREEN_MODE 8

// Logical resolution
#define BBC_X_RESOLUTION 1280
#define BBC_Y_RESOLUTION 1024

// Default colours - TODO this should be dynamic
#define COL_BLACK    0
#define COL_WHITE   15

// Current screen mode
static screen_mode_t *screen = NULL;

// Current font
static font_t *font = NULL;
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
static int16_t g_x_origin;
static int16_t g_x_pos;
static int16_t g_x_pos_last1;
static int16_t g_x_pos_last2;
static int16_t g_y_origin;
static int16_t g_y_pos;
static int16_t g_y_pos_last1;
static int16_t g_y_pos_last2;
static int16_t g_mode;
static int16_t g_plotmode;

// Text or graphical cursor for printing characters
static int8_t g_cursor;

// VDU Queue
#define VDU_QSIZE 8192
static volatile int vdu_wp = 0;
static volatile int vdu_rp = 0;
static char vdu_queue[VDU_QSIZE];

static inline pixel_t get_colour(unsigned int index) {
   return screen->get_colour(screen, index);
}

static inline void set_colour(unsigned int index, int r, int g, int b) {
   screen->set_colour(screen, index, r, g, b);
}

// Fill modes:
#define HL_LR_NB 1 // Horizontal line fill (left & right) to non-background
#define HL_RO_BG 2 // Horizontal line fill (right only) to background
#define HL_LR_FG 3 // Horizontal line fill (left & right) to foreground
#define HL_RO_NF 4 // Horizontal line fill (right only) to non-foreground
#define AF_NONBG 5 // Flood (area fill) to non-background
#define AF_TOFGD 6 // Flood (area fill) to foreground

static int e_visible; // Current visibility of the edit cursor
static int c_visible; // Current visibility of the write cursor

static void calc_text_area() {
   // Calculate the text size
   if (screen && font) {
      text_width = screen->width / font->width;
      text_height = screen->height / font->height;
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
   g_x_origin    = 0;
   g_x_pos       = 0;
   g_x_pos_last1 = 0;
   g_x_pos_last2 = 0;
   g_y_origin    = 0;
   g_y_pos       = 0;
   g_y_pos_last1 = 0;
   g_y_pos_last2 = 0;
   g_mode        = 0;
   g_plotmode    = 0;

   // Cursor mode
   g_cursor      = IN_VDU4;

   // Calculate the size of the text area
   calc_text_area();
}

static void fb_invert_cursor(int x_pos, int y_pos, int editing) {
   int x = x_pos * font->width;
   int y = screen->height - y_pos * font->height - 1;
   int y1 = editing ? font->height - 2 : 0;
   for (int i = y1; i < font->height; i++) {
      for (int j = 0; j < font->width; j++) {
         int col = screen->get_pixel(screen, x + j, y - i);
         col ^= get_colour(COL_WHITE);
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
   screen->scroll(screen);
   if (e_enabled) {
      if (e_y_pos > 0) {
         e_y_pos--;
      }
      fb_show_edit_cursor();
   }
}

// ==========================================================


void fb_clear() {
   // initialize the cursor positions, origin, colours, etc.
   // TODO: clearing all of these may not be strictly correct
   fb_init_variables();
   // clear frame buffer
   screen->clear(screen, get_colour(c_bg_col));
   // Show the cursor
   fb_show_cursor();
}


void fb_cursor_left() {
   fb_hide_cursor();
   if (c_x_pos > 0) {
      c_x_pos--;
   } else {
      c_x_pos = text_width - 1;
   }
   fb_show_cursor();
}

void fb_cursor_right() {
   fb_hide_cursor();
   if (c_x_pos < text_width - 1) {
      c_x_pos++;
   } else {
      c_x_pos = 0;
   }
   fb_show_cursor();
}

void fb_cursor_up() {
   fb_hide_cursor();
   if (c_y_pos > 0) {
      c_y_pos--;
   } else {
      c_y_pos = text_height - 1;
   }
   fb_show_cursor();
}

void fb_cursor_down() {
   fb_hide_cursor();
   if (c_y_pos < text_height - 1) {
      c_y_pos++;
   } else {
      fb_scroll();
   }
   fb_show_cursor();
}

void fb_cursor_col0() {
   fb_disable_edit_cursor();
   fb_hide_cursor();
   c_x_pos = 0;
   fb_show_cursor();
}

void fb_cursor_home() {
   fb_hide_cursor();
   c_x_pos = 0;
   c_y_pos = 0;
}


void fb_cursor_next() {
   fb_hide_cursor();
   if (c_x_pos < text_width - 1) {
      c_x_pos++;
   } else {
      c_x_pos = 0;
      fb_cursor_down();
   }
   fb_show_cursor();
}

volatile int d;

void fb_initialize() {
   font = get_font(DEFAULT_FONT);

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

void update_g_cursors(int16_t x, int16_t y) {
   g_x_pos_last2 = g_x_pos_last1;
   g_x_pos_last1 = g_x_pos;
   g_x_pos       = x;
   g_y_pos_last2 = g_y_pos_last1;
   g_y_pos_last1 = g_y_pos;
   g_y_pos       = y;
}

int calc_radius(int x1, int y1, int x2, int y2) {
   return (int)(sqrt((x2-x1)*(x2-x1)+(y2-y1)*(y2-y1)) + 0.5);
}

static void fb_draw_character(int c, int invert) {
   int fg_col = get_colour(c_fg_col);
   int gb_col = get_colour(c_bg_col);
   int x = c_x_pos * font->width;
   // Pixel 0,0 is in the bottom left
   // Character 0,0 is in the top left
   // So the Y axis needs flipping
   int y = screen->height - c_y_pos * font->height - 1;
   if (invert) {
      font->draw_character(font, screen, c, x, y, gb_col, fg_col);
   } else {
      font->draw_character(font, screen, c, x, y, fg_col, gb_col);
   }
}


void fb_writec_buffered(char ch) {
   // TODO: Deal with overflow
   vdu_queue[vdu_wp] = ch;
   vdu_wp = (vdu_wp + 1) & (VDU_QSIZE - 1);
}


void fb_writec(char ch) {
   int invert;
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
         c_bg_col = c & 15;
#ifdef DEBUG_VDU
         printf("bg = %d\r\n", c_bg_col);
#endif
      } else {
         c_fg_col = c & 15;
#ifdef DEBUG_VDU
         printf("fg = %d\r\n", c_fg_col);
#endif
      }
      return;

   } else if (state == IN_VDU18) {
      switch (count) {
      case 0:
         g_plotmode = c;
         break;
      case 1:
         if (c & 128) {
            g_bg_col = c & 15;
         } else {
            g_fg_col = c & 15;
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
            set_colour(l, r, g, b);
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
            if (g_mode < 32) {
               // Draw a line
               fb_draw_line(g_x_pos_last1, g_y_pos_last1, g_x_pos, g_y_pos, col);
            } else if (g_mode >= 64 && g_mode < 72) {
               // Plot a single pixel
               fb_putpixel(g_x_pos, g_y_pos, g_fg_col);
            } else if (g_mode >= 72 && g_mode < 80) {
               // Horizontal line fill (left and right) to non background
               fb_fill_area(g_x_pos, g_y_pos, col, HL_LR_NB);
            } else if (g_mode >= 80 && g_mode < 88) {
               // Fill a triangle
               fb_fill_triangle(g_x_pos_last2, g_y_pos_last2, g_x_pos_last1, g_y_pos_last1, g_x_pos, g_y_pos, col);
            } else if (g_mode >= 88 && g_mode < 96) {
               // Horizontal line fill (right only) to background
               fb_fill_area(g_x_pos, g_y_pos, col, HL_RO_BG);
            } else if (g_mode >= 96 && g_mode < 104) {
               // Fill a rectangle
               fb_fill_rectangle(g_x_pos_last1, g_y_pos_last1, g_x_pos, g_y_pos, col);
            } else if (g_mode >= 104 && g_mode < 112) {
               // Horizontal line fill (left and right) to foreground
               fb_fill_area(g_x_pos, g_y_pos, col, HL_LR_FG);
            } else if (g_mode >= 112 && g_mode < 120) {
               // Fill a parallelogram
               fb_fill_parallelogram(g_x_pos_last2, g_y_pos_last2, g_x_pos_last1, g_y_pos_last1, g_x_pos, g_y_pos, col);
            } else if (g_mode >= 120 && g_mode < 128) {
               // Horizontal line fill (right only) to non-foreground
               fb_fill_area(g_x_pos, g_y_pos, col, HL_RO_NF);
            } else if (g_mode >= 128 && g_mode < 136) {
               // Flood fill to non-background
               fb_fill_area(g_x_pos, g_y_pos, col, AF_NONBG);
            } else if (g_mode >= 136 && g_mode < 144) {
               // Flood fill to non-foreground
               fb_fill_area(g_x_pos, g_y_pos, col, AF_TOFGD);
            } else if (g_mode >= 144 && g_mode < 152) {
               // Draw a circle outline
               int radius = calc_radius(g_x_pos_last1, g_y_pos_last1, g_x_pos, g_y_pos);
               fb_draw_circle(g_x_pos_last1, g_y_pos_last1, radius, col);
            } else if (g_mode >= 152 && g_mode < 160) {
               // Fill a circle
               int radius = calc_radius(g_x_pos_last1, g_y_pos_last1, g_x_pos, g_y_pos);
               fb_fill_circle(g_x_pos_last1, g_y_pos_last1, radius, col);
            } else if (g_mode >= 160 && g_mode < 168) {
               // Draw a rectangle outline
               fb_draw_rectangle(g_x_pos, g_y_pos, g_x_pos_last1, g_y_pos_last1, col);
            } else if (g_mode >= 168 && g_mode < 176) {
               // Draw a parallelogram outline
               fb_draw_parallelogram(g_x_pos, g_y_pos, g_x_pos_last1, g_y_pos_last1, g_x_pos_last2, g_y_pos_last2, col);
            } else if (g_mode >= 176 && g_mode < 184) {
               // Draw a triangle outline
               fb_draw_triangle(g_x_pos, g_y_pos, g_x_pos_last1, g_y_pos_last1, g_x_pos_last2, g_y_pos_last2, col);
            } else if (g_mode >= 192 && g_mode < 200) {
               // Draw an ellipse
               fb_draw_ellipse(g_x_pos_last2, g_y_pos_last2, abs(g_x_pos_last1 - g_x_pos_last2), abs(g_y_pos - g_y_pos_last2), col);
            } else if (g_mode >= 200 && g_mode < 208) {
               // Fill a n ellipse
               fb_fill_ellipse(g_x_pos_last2, g_y_pos_last2, abs(g_x_pos_last1 - g_x_pos_last2), abs(g_y_pos - g_y_pos_last2), col);
            }
         }
      }
      count++;
      if (count == 5) {
         state = NORMAL;
      }
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
         g_x_origin = x_tmp;
         g_y_origin = y_tmp;
#ifdef DEBUG_VDU
         printf("graphics origin %d %d\r\n", g_x_origin, g_y_origin);
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
         invert = c >= 0x80;
         c &= 0x7f;

         // Draw the next character at the cursor position
         fb_hide_cursor();
         fb_draw_character(c, invert);
         fb_show_cursor();

         // Advance the drawing position
         fb_cursor_next();

      } else {
         font->draw_character(font, screen, c, g_x_pos, g_y_pos, g_fg_col, g_bg_col);
         update_g_cursors(g_x_pos + font->scale_w * font->width + font->spacing, g_y_pos);
      }
   }
}

void fb_writes(char *string) {
   while (*string) {
      fb_writec(*string++);
   }
}

/* The difference between fb_putpixel and fb_setpixel is that
 * fp_putpixel gets the colour from the logical colour table
 * and fb_setpixel writes the 16 or 32 bits colour.
 */
void fb_putpixel(int x, int y, unsigned int colour) {
   fb_setpixel(x, y, get_colour(colour));
}

void fb_setpixel(int x, int y, unsigned int colour) {
   x = ((x + g_x_origin) * screen->width)  / BBC_X_RESOLUTION;
   y = ((y + g_y_origin) * screen->height) / BBC_Y_RESOLUTION;
   if (x < 0  || x > screen->width - 1) {
      return;
   }
   if (y < 0 || y > screen->height - 1) {
      return;
   }
   screen->set_pixel(screen, x, y, colour);
}

unsigned int fb_getpixel(int x, int y) {
   x = ((x + g_x_origin) * screen->width)  / BBC_X_RESOLUTION;
   y = ((y + g_y_origin) * screen->height) / BBC_Y_RESOLUTION;
   if (x < 0  || x > screen->width - 1) {
      return g_bg_col;
   }
   if (y < 0 || y > screen->height - 1) {
      return g_bg_col;
   }
   return screen->get_pixel(screen, x, y);
}


// Implementation of Bresenham's line drawing algorithm from here:
// http://tech-algorithm.com/articles/drawing-line-using-bresenham-algorithm/
void fb_draw_line(int x,int y,int x2, int y2, unsigned int color) {
   int i;
   int w = x2 - x;
   int h = y2 - y;
   int dx1 = 0, dy1 = 0, dx2 = 0, dy2 = 0;
   if (w < 0) dx1 = -1 ; else if (w > 0) dx1 = 1;
   if (h < 0) dy1 = -1 ; else if (h > 0) dy1 = 1;
   if (w < 0) dx2 = -1 ; else if (w > 0) dx2 = 1;
   int longest = abs(w);
   int shortest = abs(h);
   if (!(longest > shortest)) {
      longest = abs(h);
      shortest = abs(w);
      if (h < 0) dy2 = -1 ; else if (h > 0) dy2 = 1;
      dx2 = 0;
   }
   int numerator = longest >> 1 ;
   for (i = 0; i <= longest; i++) {
      fb_putpixel(x, y, color);
      numerator += shortest;
      if (!(numerator < longest)) {
         numerator -= longest;
         x += dx1;
         y += dy1;
      } else {
         x += dx2;
         y += dy2;
      }
   }
}

void fb_fill_triangle(int x, int y, int x2, int y2, int x3, int y3, unsigned int colour) {
   x = ((x + g_x_origin) * screen->width)  / BBC_X_RESOLUTION;
   y = ((y + g_y_origin) * screen->height) / BBC_Y_RESOLUTION;
   x2 = ((x2 + g_x_origin) * screen->width)  / BBC_X_RESOLUTION;
   y2 = ((y2 + g_y_origin) * screen->height) / BBC_Y_RESOLUTION;
   x3 = ((x3 + g_x_origin) * screen->width)  / BBC_X_RESOLUTION;
   y3 = ((y3 + g_y_origin) * screen->height) / BBC_Y_RESOLUTION;
   // Flip y axis
   y = screen->height - 1 - y;
   y2 = screen->height - 1 - y2;
   y3 = screen->height - 1 - y3;
   colour = get_colour(colour);
   v3d_draw_triangle(x, y, x2, y2, x3, y3, colour);
}

void fb_draw_triangle(int x1, int y1, int x2, int y2, int x3, int y3, unsigned int colour) {
   fb_draw_line(x1, y1, x2, y2, colour);
   fb_draw_line(x2, y2, x3, y3, colour);
   fb_draw_line(x3, y3, x1, y1, colour);
}

void fb_draw_circle(int xc, int yc, int r, unsigned int colour) {
   int x=0;
   int y=r;
   int p=3-(2*r);

   fb_putpixel(xc+x,yc-y,colour);

   for(x=0;x<=y;x++)
      {
         if (p<0)
            {
               p+=4*x+6;
            }
         else
            {
               y--;
               p+=4*(x-y)+10;
            }

         fb_putpixel(xc+x,yc-y,colour);
         fb_putpixel(xc-x,yc-y,colour);
         fb_putpixel(xc+x,yc+y,colour);
         fb_putpixel(xc-x,yc+y,colour);
         fb_putpixel(xc+y,yc-x,colour);
         fb_putpixel(xc-y,yc-x,colour);
         fb_putpixel(xc+y,yc+x,colour);
         fb_putpixel(xc-y,yc+x,colour);
      }
}

void fb_fill_circle(int xc, int yc, int r, unsigned int colour) {
   int x=0;
   int y=r;
   int p=3-(2*r);

   fb_putpixel(xc+x,yc-y,colour);

   for(x=0;x<=y;x++)
      {
         if (p<0)
            {
               p+=4*x+6;
            }
         else
            {
               y--;
               p+=4*(x-y)+10;
            }

         fb_draw_line(xc+x,yc-y,xc-x,yc-y,colour);
         fb_draw_line(xc+x,yc+y,xc-x,yc+y,colour);
         fb_draw_line(xc+y,yc-x,xc-y,yc-x,colour);
         fb_draw_line(xc+y,yc+x,xc-y,yc+x,colour);
      }
}
/* Bad rectangle due to triangle bug
   void fb_fill_rectangle(int x1, int y1, int x2, int y2, unsigned int colour) {
   fb_fill_triangle(x1, y1, x1, y2, x2, y2, colour);
   fb_fill_triangle(x1, y1, x2, y1, x2, y2, colour);
   }
*/

void fb_fill_rectangle(int x1, int y1, int x2, int y2, unsigned int colour) {
   int y;
   for (y = y1; y <= y2; y++) {
      fb_draw_line(x1, y, x2, y, colour);
   }
}

void fb_draw_rectangle(int x1, int y1, int x2, int y2, unsigned int colour) {
   fb_draw_line(x1, y1, x2, y1, colour);
   fb_draw_line(x2, y1, x2, y2, colour);
   fb_draw_line(x2, y2, x1, y2, colour);
   fb_draw_line(x1, y2, x1, y1, colour);
}

void fb_fill_parallelogram(int x1, int y1, int x2, int y2, int x3, int y3, unsigned int colour) {
   int x4 = x3 - x2 + x1;
   int y4 = y3 - y2 + y1;
   fb_fill_triangle(x1, y1, x2, y2, x3, y3, colour);
   fb_fill_triangle(x1, y1, x4, y4, x3, y3, colour);
}

void fb_draw_parallelogram(int x1, int y1, int x2, int y2, int x3, int y3, unsigned int colour) {
   int x4 = x3 - x2 + x1;
   int y4 = y3 - y2 + y1;
   fb_draw_line(x1, y1, x2, y2, colour);
   fb_draw_line(x2, y2, x3, y3, colour);
   fb_draw_line(x3, y3, x4, y4, colour);
   fb_draw_line(x4, y4, x1, y1, colour);
}

void fb_draw_ellipse(int xc, int yc, int width, int height, unsigned int colour) {
   int a2 = width * width;
   int b2 = height * height;
   int fa2 = 4 * a2, fb2 = 4 * b2;
   int x, y, sigma;

   /* first half */
   for (x = 0, y = height, sigma = 2*b2+a2*(1-2*height); b2*x <= a2*y; x++)
      {
         fb_putpixel(xc + x, yc + y, colour);
         fb_putpixel(xc - x, yc + y, colour);
         fb_putpixel(xc + x, yc - y, colour);
         fb_putpixel(xc - x, yc - y, colour);
         if (sigma >= 0)
            {
               sigma += fa2 * (1 - y);
               y--;
            }
         sigma += b2 * ((4 * x) + 6);
      }

   /* second half */
   for (x = width, y = 0, sigma = 2*a2+b2*(1-2*width); a2*y <= b2*x; y++)
      {
         fb_putpixel(xc + x, yc + y, colour);
         fb_putpixel(xc - x, yc + y, colour);
         fb_putpixel(xc + x, yc - y, colour);
         fb_putpixel(xc - x, yc - y, colour);
         if (sigma >= 0)
            {
               sigma += fb2 * (1 - x);
               x--;
            }
         sigma += a2 * ((4 * y) + 6);
      }
}

void fb_fill_ellipse(int xc, int yc, int width, int height, unsigned int colour) {
   int a2 = width * width;
   int b2 = height * height;
   int fa2 = 4 * a2, fb2 = 4 * b2;
   int x, y, sigma;

   /* first half */
   for (x = 0, y = height, sigma = 2*b2+a2*(1-2*height); b2*x <= a2*y; x++)
      {
         fb_draw_line(xc + x, yc + y, xc - x, yc + y, colour);
         fb_draw_line(xc + x, yc - y, xc - x, yc - y, colour);
         if (sigma >= 0)
            {
               sigma += fa2 * (1 - y);
               y--;
            }
         sigma += b2 * ((4 * x) + 6);
      }

   /* second half */
   for (x = width, y = 0, sigma = 2*a2+b2*(1-2*width); a2*y <= b2*x; y++)
      {
         fb_draw_line(xc + x, yc + y, xc - x, yc + y, colour);
         fb_draw_line(xc + x, yc - y, xc - x, yc - y, colour);
         if (sigma >= 0)
            {
               sigma += fb2 * (1 - x);
               x--;
            }
         sigma += a2 * ((4 * y) + 6);
      }
}

void fb_fill_area(int x, int y, unsigned int colour, unsigned int mode) {
   /*   Modes:
    * HL_LR_NB: horizontal line fill (left & right) to non-background - done
    * HL_RO_BG: Horizontal line fill (right only) to background - done
    * HL_LR_FG: Horizontal line fill (left & right) to foreground
    * HL_RO_NF: Horizontal line fill (right only) to non-foreground - done
    * AF_NONBG: Flood (area fill) to non-background
    * AF_TOFGD: Flood (area fill) to foreground
    */

   int save_x = x;
   int save_y = y;
   int x_left = x;
   int y_left = y;
   int x_right = x;
   int y_right = y;
   int real_y;
   unsigned int stop = 0;

   // printf("Plot (%d,%d), colour %d, mode %d\r\n", x, y, colour, mode);
   // printf("g_bg_col = %d, g_fg_col = %d\n\r", g_bg_col, g_fg_col);

   switch(mode) {
   case HL_LR_NB:
      while (! stop) {
         if (fb_getpixel(x_right,y) == get_colour(g_bg_col) && x_right <= BBC_X_RESOLUTION) {
            x_right += BBC_X_RESOLUTION/screen->width;   // speeds up but might fail if not integer
         } else {
            stop = 1;
         }
      }
      stop = 0;
      x = save_x - 1;
      while (! stop) {
         if (fb_getpixel(x_left,y) == get_colour(g_bg_col) && x_left >= 0) {
            x_left -= BBC_X_RESOLUTION/screen->width;    // speeds up but might fail if not integer
         } else {
            stop = 1;
         }
      }
      break;

   case HL_RO_BG:
      while (! stop) {
         if (fb_getpixel(x_right,y) != get_colour(g_bg_col) && x_right <= BBC_X_RESOLUTION) {
            x_right += BBC_X_RESOLUTION/screen->width;   // speeds up but might fail if not integer
         } else {
            stop = 1;
         }
      }
      break;

   case HL_LR_FG:
      while (! stop) {
         if (fb_getpixel(x_right,y) != get_colour(g_fg_col) && x_right <= BBC_X_RESOLUTION) {
            x_right += BBC_X_RESOLUTION/screen->width;   // speeds up but might fail if not integer
         } else {
            stop = 1;
         }
      }
      stop = 0;
      x = save_x - 1;
      while (! stop) {
         if (fb_getpixel(x_left,y) != get_colour(g_fg_col) && x_left >= 0) {
            x_left -= BBC_X_RESOLUTION/screen->width;    // speeds up but might fail if not integer
         } else {
            stop = 1;
         }
      }
      break;


   case HL_RO_NF:
      while (! stop) {
         if (fb_getpixel(x_right,y) != get_colour(g_fg_col) && x_right <= BBC_X_RESOLUTION) {
            x_right += BBC_X_RESOLUTION/screen->width;   // speeds up but might fail if not integer
         } else {
            stop = 1;
         }
      }
      break;

   case AF_NONBG:
      // going up
      while (! stop) {
         if (fb_getpixel(x,y) == get_colour(g_bg_col) && y <= BBC_Y_RESOLUTION) {
            fb_fill_area(x, y, colour, HL_LR_NB);
            // As the BBC_Y_RESOLUTION is not a multiple of screen->height we have to increment
            // y until the physical y-coordinate increases. If we don't do that and simply increment
            // y by 2 then at some point the physical y-coordinate is the same as the previous drawn
            // line and the floodings stops. This also speeds up drawing because each physical line
            // is only drawn once.
            real_y = ((y + g_y_origin) * screen->height) / BBC_Y_RESOLUTION;
            while(((y + g_y_origin) * screen->height) / BBC_Y_RESOLUTION == real_y) {
               y++;
            }
            x = (g_x_pos_last1 + g_x_pos_last2) / 2;
         } else {
            stop = 1;
         }
      }
      // going down
      stop = 0;
      x = save_x;
      y = save_y - BBC_Y_RESOLUTION/screen->height;
      while (! stop) {
         if (fb_getpixel(x,y) == get_colour(g_bg_col) && y >= 0) {
            fb_fill_area(x, y, colour, HL_LR_NB);
            real_y = ((y + g_y_origin) * screen->height) / BBC_Y_RESOLUTION;
            while(((y + g_y_origin) * screen->height) / BBC_Y_RESOLUTION == real_y) {
               y--;
            }
            x = (g_x_pos_last1 + g_x_pos_last2) / 2;
         } else {
            stop = 1;
         }
      }
      colour = -1;  // prevents any additional line drawing
      break;

   case AF_TOFGD:
      // going up
      while (! stop) {
         if (fb_getpixel(x,y) != get_colour(g_fg_col) && y <= BBC_Y_RESOLUTION) {
            fb_fill_area(x, y, colour, HL_LR_FG);
            real_y = ((y + g_y_origin) * screen->height) / BBC_Y_RESOLUTION;
            while(((y + g_y_origin) * screen->height) / BBC_Y_RESOLUTION == real_y) {
               y++;
            }
            x = (g_x_pos_last1 + g_x_pos_last2) / 2;
         } else {
            stop = 1;
         }
      }
      // going down
      stop = 0;
      x = save_x;
      y = save_y - BBC_Y_RESOLUTION/screen->height;
      while (! stop) {
         if (fb_getpixel(x,y) != get_colour(g_fg_col) && y >= 0) {
            fb_fill_area(x, y, colour, HL_LR_NB);
            real_y = ((y + g_y_origin) * screen->height) / BBC_Y_RESOLUTION;
            while(((y + g_y_origin) * screen->height) / BBC_Y_RESOLUTION == real_y) {
               y--;
            }
            x = (g_x_pos_last1 + g_x_pos_last2) / 2;
         } else {
            stop = 1;
         }
      }
      colour = -1;  // prevents any additional line drawing
      break;

   default:
      printf( "Unknown fill mode %d\r\n", mode);
      break;
   }

   if (colour >= 0) {
      fb_draw_line(x_left, y_left, x_right, y_right, colour);
      g_x_pos_last1 = x_left;
      g_y_pos_last1 = y_left;
      g_x_pos_last2 = x_right;
      g_y_pos_last2 = y_right;
   }
}

uint32_t fb_get_address() {
   return (uint32_t) fb;
}

uint32_t fb_get_width() {
   return screen->width;
}

uint32_t fb_get_height() {
   return screen->height;
}

uint32_t fb_get_depth() {
   if (screen) {
      return screen->bpp;
   } else {
      return 0;
   }
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

int fb_get_cursor_x() {
   return c_x_pos;
};

int fb_get_cursor_y() {
   return c_y_pos;
};

int fb_get_edit_cursor_x() {
   return e_x_pos;
};

int fb_get_edit_cursor_y() {
   return e_y_pos;
};

int fb_get_edit_cursor_char() {
   int x = e_x_pos * font->width;
   int y = screen->height - e_y_pos * font->height - 1;
   return font->read_character(font, screen, x, y);
};

int16_t fb_get_g_bg_col() {
   return g_bg_col;
}

int16_t fb_get_g_fg_col() {
   return g_fg_col;
}
