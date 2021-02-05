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
#include "fonts.h"
#include "mousepointers.h"
#include "sprites.h"

// Default colours
#define COL_BLACK    0
#define COL_WHITE   (screen->ncolour)

// Current screen mode
static screen_mode_t *screen = NULL;

// Current font
static int16_t font_width;
static int16_t font_height;
static int16_t text_height; // of whole screen
static int16_t text_width;  // of whole screen

// Text area clip window
static t_clip_window_t t_window;

// Character colour / cursor position
static uint8_t c_bg_col;
static uint8_t c_fg_col;
static int16_t c_x_pos;
static int16_t c_y_pos;

static int16_t e_enabled = 0;
static int16_t e_x_pos;
static int16_t e_y_pos;

// Graphics colour / cursor position
static uint8_t g_bg_col;
static uint8_t g_fg_col;
static int16_t g_x_pos;
static int16_t g_x_pos_last1;
static int16_t g_x_pos_last2;
static int16_t g_y_pos;
static int16_t g_y_pos_last1;
static int16_t g_y_pos_last2;
static int16_t g_x_origin;
static int16_t g_y_origin;

// Graphics area clip window
static g_clip_window_t g_window;

// Text or graphical cursor for printing characters
static int8_t text_at_g_cursor;

static int e_visible; // Current visibility of the edit cursor
static int c_visible; // Current visibility of the write cursor

// VDU Queue
#define VDU_QSIZE 8192
static volatile int vdu_wp = 0;
static volatile int vdu_rp = 0;
static uint8_t vdu_queue[VDU_QSIZE];

#define VDU_BUF_LEN 16

typedef struct {
   int len;
   void (*handler)(uint8_t *buf);
} vdu_operation_t;

static void vdu_4(uint8_t *buf);
static void vdu_5(uint8_t *buf);
static void vdu_16(uint8_t *buf);
static void vdu_17(uint8_t *buf);
static void vdu_18(uint8_t *buf);
static void vdu_19(uint8_t *buf);
static void vdu_20(uint8_t *buf);
static void vdu_22(uint8_t *buf);
static void vdu_23(uint8_t *buf);
static void vdu_24(uint8_t *buf);
static void vdu_25(uint8_t *buf);
static void vdu_26(uint8_t *buf);
static void vdu_27(uint8_t *bu0f);
static void vdu_28(uint8_t *buf);
static void vdu_29(uint8_t *buf);
static void vdu_nop(uint8_t *buf);
static void vdu_default(uint8_t *buf);

static vdu_operation_t vdu_operation_table[256] = {
   // Entries 8-13,30,31,127 are filled in by VDU 4/5
   // remaining entries >=32 are filled in by fb_initialize
   { 0, vdu_nop }, // 0 -  Does nothing
   { 0, vdu_nop }, // 1 -  Send next character to printer only (do nothing)
   { 0, vdu_nop }, // 2 -  Enable printer (do nothing)
   { 0, vdu_nop }, // 3 -  Disable printer (do nothing)
   { 0, vdu_4   }, // 4 -  Write text at text cursor
   { 0, vdu_5   }, // 5 -  Write text at graphics cursor
   { 0, vdu_nop }, // 6 -  TODO: Enable VDU drivers
   { 0, vdu_nop }, // 7 -  Make a short beep (do nothing)
   { 0, vdu_nop }, // 8 -  Backspace cursor one character
   { 0, vdu_nop }, // 9 -  Forward space cursor one character
   { 0, vdu_nop }, // 10 - Move cursor down one line
   { 0, vdu_nop }, // 11 - Move cursor up one line
   { 0, vdu_nop }, // 12 - Clear text area
   { 0, vdu_nop }, // 13 - Move cursor to start of current line
   { 0, vdu_nop }, // 14 - Page mode on (do nothing)
   { 0, vdu_nop }, // 15 - Page mode off (do nothing)
   { 0, vdu_16  }, // 16 - Clear graphics area
   { 1, vdu_17  }, // 17 - Define text colour
   { 2, vdu_18  }, // 18 - Define graphics colour
   { 6, vdu_19  }, // 19 - Define logical colour
   { 0, vdu_20  }, // 20 - Reset logical colours to defaults
   { 0, vdu_nop }, // 21 - TODO: Disable VDU drivers or delete current line
   { 1, vdu_22  }, // 22 - Select screen mode
   { 9, vdu_23  }, // 23 - Re-program display character (+many other things)
   { 8, vdu_24  }, // 24 - Define graphics window
   { 5, vdu_25  }, // 25 - PLOT mode,x,y
   { 0, vdu_26  }, // 26 - Restore default windows
   { 1, vdu_27  }, // 27 - Escape next character
   { 4, vdu_28  }, // 28 - Define text window
   { 4, vdu_29  }, // 29 - Define graphics origin
   { 0, vdu_nop }, // 30 - Home text cursor to top left
   { 2, vdu_nop }  // 31 - Move text cursor to x,y
};

// ==========================================================================
// Static methods
// ==========================================================================

static void update_font_size();
static void update_text_area();
static void init_variables();
static void reset_areas();
static void set_text_area(t_clip_window_t *window);
static void invert_cursor(int x_pos, int y_pos, int editing);
static void show_cursor();
static void show_edit_cursor();
static void hide_cursor();
static void hide_edit_cursor();
static void enable_edit_cursor();
static void disable_edit_cursor();
static void cursor_interrupt();
static void edit_cursor_up();
static void edit_cursor_down();
static void edit_cursor_left();
static void edit_cursor_right();
static void text_area_scroll();
static void update_g_cursors(int16_t x, int16_t y);
static void change_mode(screen_mode_t *new_screen);
static void set_graphics_area(screen_mode_t *screen, g_clip_window_t *window);

// These are used in VDU 4 mode
static void text_cursor_left();
static void text_cursor_right();
static void text_cursor_up();
static void text_cursor_down();
static void text_cursor_col0();
static void text_cursor_home();
static void text_cursor_tab();
static void text_area_clear();
static void text_delete();

// These are used in VDU 5 mode
static void graphics_cursor_left();
static void graphics_cursor_right();
static void graphics_cursor_up();
static void graphics_cursor_down();
static void graphics_cursor_col0();
static void graphics_cursor_home();
static void graphics_cursor_tab();
static void graphics_area_clear();
static void graphics_delete();

static void update_font_size() {
   // get the current font from the screen
   font_t *font = screen->font;
   // Calculate the font size, taking account of scale and spacing
   font_width  = font->width * font->scale_w + font->spacing;
   font_height = font->height * font->scale_h + font->spacing;
   // Calc screen text size
   text_width = screen->width / font_width;
   text_height = screen->height / font_height;
}

static void update_text_area() {
   // Make sure font size hasn't changed
   update_font_size();
   // Make sure text area is on the screen
   if (t_window.right >= text_width) {
      t_window.right = text_width - 1;
      if (t_window.left > t_window.right) {
         t_window.left = t_window.right;
      }
   }
   if (t_window.bottom >= text_height) {
      t_window.bottom = text_height - 1;
      if (t_window.top > t_window.bottom) {
         t_window.top = t_window.bottom;
      }
   }
   // Make sure cursor is in text area
   int16_t tmp_x = c_x_pos;
   int16_t tmp_y = c_y_pos;
   if (tmp_x < t_window.left) {
      tmp_x = t_window.left;
   } else if (tmp_x > t_window.right) {
      tmp_x = t_window.right;
   }
   if (tmp_y < t_window.top) {
      tmp_y = t_window.top;
   } else if (tmp_y > t_window.bottom) {
      tmp_y = t_window.bottom;
   }
   if (c_x_pos != tmp_x || c_y_pos != tmp_y) {
      if (!text_at_g_cursor) {
         hide_cursor();
      }
      c_x_pos = tmp_x;
      c_y_pos = tmp_y;
      if (!text_at_g_cursor) {
         show_cursor();
      }
   }
}

static void init_variables() {

   // Character colour / cursor position
   c_bg_col  = COL_BLACK;
   c_fg_col  = COL_WHITE;
   c_visible = 0;

   // Edit cursor
   e_x_pos   = 0;
   e_y_pos   = 0;
   e_visible = 0;
   e_enabled = 0;

   // Graphics colour / cursor position
   g_bg_col  = COL_BLACK;
   g_fg_col  = COL_WHITE;

   // Cursor mode
   vdu_4(NULL);

   // Reset text/grapics areas and home cursors (VDU 26 actions)
   reset_areas();

}

static void reset_areas() {
   // Calculate the size of the text area
   update_font_size();
   // Initialize the text area to the full screen
   // (left, bottom, right, top)
   t_clip_window_t default_t_window = {0, text_height - 1, text_width - 1, 0};
   set_text_area(&default_t_window);
   // Set the graphics origin to 0,0
   g_x_origin = 0;
   g_y_origin = 0;
   // Initialize the graphics area to the full screen
   // (left, bottom, right, top)
   g_clip_window_t default_graphics_window = {0, 0, (screen->width << screen->xeigfactor) - 1, (screen->height << screen->yeigfactor) - 1};
   set_graphics_area(screen, &default_graphics_window);
   // Initialize the default plot mode to normal plotting
   prim_set_graphics_plotmode(0);
   // Home the text cursor
   c_x_pos = t_window.left;
   c_y_pos = t_window.top;
   // Home the graphics cursor
   g_x_pos       = 0;
   g_x_pos_last1 = 0;
   g_x_pos_last2 = 0;
   g_y_pos       = 0;
   g_y_pos_last1 = 0;
   g_y_pos_last2 = 0;
}

// 0,0 is the top left
static void set_text_area(t_clip_window_t *window) {
   if (window->left > window->right || window->right > text_width - 1 || window->top > window->bottom || window->bottom > text_height - 1) {
      return;
   }
   // Shallow copy of the struct
   t_window = *window;
   // Update any dependant variabled
   update_text_area();
}

static void invert_cursor(int x_pos, int y_pos, int editing) {
   int x = x_pos * font_width;
   int y = screen->height - y_pos * font_height - 1;
   int y1 = editing ? font_height - 2 : 0;
   for (int i = y1; i < font_height; i++) {
      for (int j = 0; j < font_width; j++) {
         pixel_t col = screen->get_pixel(screen, x + j, y - i);
         col ^= screen->get_colour(screen, COL_WHITE);
         screen->set_pixel(screen, x + j, y - i, col);
      }
   }
}

static void show_cursor() {
   if (c_visible) {
      return;
   }
   invert_cursor(c_x_pos, c_y_pos, 0);
   c_visible = 1;
}

static void show_edit_cursor() {
   if (e_visible) {
      return;
   }
   invert_cursor(e_x_pos, e_y_pos, 1);
   e_visible = 1;
}

static void hide_cursor() {
   if (!c_visible) {
      return;
   }
   invert_cursor(c_x_pos, c_y_pos, 0);
   c_visible = 0;
}

static void hide_edit_cursor() {
   if (!e_visible) {
      return;
   }
   invert_cursor(e_x_pos, e_y_pos, 1);
   e_visible = 0;
}

static void enable_edit_cursor() {
   if (!e_enabled) {
      e_enabled = 1;
      e_x_pos = c_x_pos;
      e_y_pos = c_y_pos;
      show_edit_cursor();
   }
}

static void disable_edit_cursor() {
   if (e_enabled) {
      hide_edit_cursor();
      e_x_pos = 0;
      e_y_pos = 0;
      e_enabled = 0;
   }
}

static void cursor_interrupt() {
   if (e_enabled) {
      if (e_visible) {
         hide_edit_cursor();
      } else {
         show_edit_cursor();
      }
   }
}

static void edit_cursor_up() {
   enable_edit_cursor();
   hide_edit_cursor();
   if (e_y_pos > t_window.top) {
      e_y_pos--;
   } else {
      e_y_pos = t_window.bottom;
   }
   show_edit_cursor();
}

static void edit_cursor_down() {
   enable_edit_cursor();
   hide_edit_cursor();
   if (e_y_pos < t_window.bottom) {
      e_y_pos++;
   } else {
      e_y_pos = t_window.top;
   }
   show_edit_cursor();
}

static void edit_cursor_left() {
   enable_edit_cursor();
   hide_edit_cursor();
   if (e_x_pos > t_window.left) {
      e_x_pos--;
   } else {
      e_x_pos = t_window.right;
      if (e_y_pos > t_window.top) {
         e_y_pos--;
      } else {
         e_y_pos = t_window.bottom;
      }
   }
   show_edit_cursor();
}

static void edit_cursor_right() {
   enable_edit_cursor();
   hide_edit_cursor();
   if (e_x_pos < t_window.right) {
      e_x_pos++;
   } else {
      e_x_pos = t_window.left;
      if (e_y_pos < t_window.bottom) {
         e_y_pos++;
      } else {
         e_y_pos = t_window.top;
      }
   }
   show_edit_cursor();
}

static void text_area_scroll() {
   pixel_t bg_col = screen->get_colour(screen, c_bg_col);
   if (e_enabled) {
      hide_edit_cursor();
   }
   screen->scroll(screen, &t_window, bg_col);
   if (e_enabled) {
      if (e_y_pos > t_window.top) {
         e_y_pos--;
      }
      show_edit_cursor();
   }
}


static void update_g_cursors(int16_t x, int16_t y) {
   g_x_pos_last2 = g_x_pos_last1;
   g_x_pos_last1 = g_x_pos;
   g_x_pos       = x;
   g_y_pos_last2 = g_y_pos_last1;
   g_y_pos_last1 = g_y_pos;
   g_y_pos       = y;
}

static void change_mode(screen_mode_t *new_screen) {
   if (new_screen && (new_screen != screen || new_screen->mode_num >= CUSTOM_8BPP_SCREEN_MODE)) {
      screen = new_screen;
      screen->init(screen);
   }
   // initialze VDU variable
   init_variables();
   // clear frame buffer
   screen->clear(screen, NULL, screen->get_colour(screen, c_bg_col));
   // Show the cursor
   c_visible = 0;
   show_cursor();
}

static void set_graphics_area(screen_mode_t *screen, g_clip_window_t *window) {
   // Sanity check illegal windowss
   if (window->left   < 0 || window->left   >= screen->width  << screen->xeigfactor ||
       window->bottom < 0 || window->bottom >= screen->height << screen->yeigfactor) {
      return;
   }
   if (window->right  < 0 || window->right  >= screen->width  << screen->xeigfactor ||
       window->top    < 0 || window->top    >= screen->height << screen->yeigfactor) {
      return;
   }
   if (window->left >= window->right || window->bottom >= window->top) {
      return;
   }
   // Accept the window
   g_window = *window;
   // Transform to screen coordinates
   int16_t x1 = window->left   >> screen->xeigfactor;
   int16_t y1 = window->bottom >> screen->yeigfactor;
   int16_t x2 = window->right  >> screen->xeigfactor;
   int16_t y2 = window->top    >> screen->yeigfactor;
   // Set the clipping window
   prim_set_graphics_area(screen, x1, y1, x2, y2);
}

// ==========================================================================
// VDU 4 mode: cursor commands operate on text cursor
// ==========================================================================

static void text_cursor_left() {
   hide_cursor();
   if (c_x_pos > t_window.left) {
      c_x_pos--;
   } else {
      c_x_pos = t_window.right;
      text_cursor_up();
   }
   show_cursor();
}

static void text_cursor_right() {
   hide_cursor();
   if (c_x_pos < t_window.right) {
      c_x_pos++;
   } else {
      c_x_pos = t_window.left;
      text_cursor_down();
   }
   show_cursor();
}

static void text_cursor_up() {
   hide_cursor();
   if (c_y_pos > t_window.top) {
      c_y_pos--;
   } else {
      c_y_pos = t_window.bottom;
   }
   show_cursor();
}

static void text_cursor_down() {
   hide_cursor();
   if (c_y_pos < t_window.bottom) {
      c_y_pos++;
   } else {
      text_area_scroll();
   }
   show_cursor();
}

static void text_cursor_col0() {
   disable_edit_cursor();
   hide_cursor();
   c_x_pos = t_window.left;
   show_cursor();
}

static void text_cursor_home() {
   hide_cursor();
   c_x_pos = t_window.left;
   c_y_pos = t_window.top;
   show_cursor();
}

static void text_cursor_tab(uint8_t *buf) {
   uint8_t x = buf[1];
   uint8_t y = buf[2];
#ifdef DEBUG_VDU
   printf("cursor move to %d %d\r\n", x, y);
#endif
   // Take account of current text window
   x += t_window.left;
   y += t_window.top;
   if (x <= t_window.right && y <= t_window.bottom) {
      hide_cursor();
      c_x_pos = x;
      c_y_pos = y;
      show_cursor();
   }
}

static void text_area_clear() {
   pixel_t bg_col = screen->get_colour(screen, c_bg_col);
   screen->clear(screen, &t_window, bg_col);
   c_x_pos = t_window.left;
   c_y_pos = t_window.top;
   c_visible = 0;
   show_cursor();
}

static void text_delete() {
   text_cursor_left();
   hide_cursor();
   int x = c_x_pos * font_width;
   int y = screen->height - c_y_pos * font_height - 1;
   pixel_t col = screen->get_colour(screen, c_bg_col);
   prim_fill_rectangle(screen, x, y, x + (font_width - 1), y - (font_height - 1), col);
   show_cursor();
}

// ==========================================================================
// VDU 5 mode: cursor commands operate on graphics cursor
// ==========================================================================

// Notes:
//    g_x_pos/g_y_pos are in absolute external coordinates
//    g_window.left/max are also in absolute external coordinates
//    font_width/height are in screen pixels

static void graphics_cursor_left() {
   g_x_pos -= font_width << screen->xeigfactor;
   if (g_x_pos < g_window.left) {
      g_x_pos = g_window.right + 1 - (font_width << screen->xeigfactor);
      graphics_cursor_up();
   }
}

static void graphics_cursor_right() {
   g_x_pos += font_width << screen->xeigfactor;
   if (g_x_pos > g_window.right) {
      g_x_pos = g_window.left;
      graphics_cursor_down();
   }
}

static void graphics_cursor_up() {
   g_y_pos += font_height << screen->yeigfactor;
   if (g_y_pos > g_window.top) {
      g_y_pos = g_window.bottom - 1 + (font_height << screen->yeigfactor);
   }
}

static void graphics_cursor_down() {
   g_y_pos -= font_height << screen->yeigfactor;
   if (g_y_pos < g_window.bottom) {
      g_y_pos = g_window.top;
   }
}

static void graphics_cursor_col0() {
   g_x_pos = g_window.left;
}

static void graphics_cursor_home() {
   g_x_pos = g_window.left;
   g_y_pos = g_window.top;
}

static void graphics_cursor_tab(uint8_t *buf) {
   uint8_t x = buf[1];
   uint8_t y = buf[2];
#ifdef DEBUG_VDU
   printf("cursor move to %d %d\r\n", x, y);
#endif
   // Scale to absolute external coordinates
   x *= font_width << screen->xeigfactor;
   y *= font_height << screen->yeigfactor;
   // Take account of current text window
   x += g_window.left;
   y += g_window.bottom;
   // Deliberately don't range check here
   g_x_pos = g_window.left + x;
   g_y_pos = g_window.bottom + y;
}

static void graphics_area_clear() {
   g_x_pos = g_window.left;
   g_y_pos = g_window.top;
   pixel_t col = screen->get_colour(screen, g_bg_col);
   prim_clear_graphics_area(screen, col);
}

static void graphics_delete() {
   graphics_cursor_left();
   int x = g_x_pos >> screen->xeigfactor;
   int y = g_y_pos >> screen->yeigfactor;
   pixel_t col = screen->get_colour(screen, g_bg_col);
   prim_fill_rectangle(screen, x, y, x + (font_width - 1), y - (font_height - 1), col);
}

// ==========================================================================
// VDU 23 commands
// ==========================================================================

static void vdu23_3(uint8_t *buf) {
   // VDU 23,3: select font by name
   // params: eight bytes font name, trailing zeros if name shorter than 8 chars
   // (selects the default font if the lookup fails)
   // get the current font from the screen
   if (buf[1] == 0) {
      screen->font = get_font_by_number(buf[2]);
   } else {
      screen->font = get_font_by_name((char *)buf+1);
   }
#ifdef DEBUG_VDU
   printf("Font set to %s\r\n", screen->font->name);
#endif
   update_text_area();
}

static void vdu23_4(uint8_t *buf) {
   // VDU 23,4: set font scale
   // params: hor. scale, vert. scale, char spacing, other 5 bytes are ignored
   // get the current font from the screen
   font_t *font = screen->font;
   font->scale_w = buf[1] > 0 ? buf[1] : 1;
   font->scale_h = buf[2] > 0 ? buf[2] : 1;
   font->spacing = buf[3];
#ifdef DEBUG_VDU
   printf("Font scale   set to %d,%d\r\n", font->scale_w, font->scale_h);
   printf("Font spacing set to %d\r\n", font->spacing);
#endif
   update_text_area();
}

static void vdu23_5(uint8_t *buf) {
   // VDU 23,5: set mouse pointer
   // params:   mouse pointer id ( 0...31)
   //           mouse x ( 0 < x < 256)
   //           reserved: should be 0
   //           mouse y ( 64 < y < 256)
   //           reserved: should be 0
   //           mouse colour red (0 < r < 31)
   //           mouse colour green (0 < g < 63)
   //           mouse colour blue (0 < b < 31)
   int x = buf[2] * 5;
   int y = (buf[4]-64) * 5.3333;
   if (mp_save_x != 65535) {
      mp_restore(screen);
   }
   mp_save_x = x;
   mp_save_y = y;
   mp_save(screen);
   int col = ((buf[6]&0x1F) << 11) + ((buf[7]&0x3F) << 5) + (buf[8]&0x1F);
   // Draw black mask in actuale background colour
   int mp = buf[1];

   // Read the current graphics background colour, and translate to a pixel value
   pixel_t g_bg_col = screen->get_colour(screen, fb_get_g_bg_col());
   for (int i=0; i<8; i++) {
      int p = mpblack[mp * 8 + i];
      for (int b=0; b<8; b++) {
         if (p&0x80) {
            // mask pixel set, set to bg colour
            screen->set_pixel(screen, x + b * 2, y - i * 2, g_bg_col);
         }
         p <<= 1;
      }
   }

   // Draw white mask in given foreground colour
   for (int i=0; i<8; i++) {
      int p = mpwhite[mp * 8 + i];
      for (int b=0; b<8; b++) {
         if (p&0x80) {
            // mask pixel set, set to foreground colour
            screen->set_pixel(screen, x + b * 2, y - i * 2, col);
         }
         p <<= 1;
      }
   }
}

static void vdu23_6(uint8_t *buf) {
   // VDU 23,6: turn off mouse pointer
   // params:   none
   mp_restore(screen);
   mp_save_x = 0xFFFF;
   mp_save_y = 0xFFFF;

}

static void vdu23_7(uint8_t *buf) {
   // VDU 23,7: define sprite
   // params:   sprite number
   //           16x16 pixels (256 bytes)
   memcpy(sprites+buf[1]*256, buf+2, 256);
}

static void vdu23_8(uint8_t *buf) {
   // vdu 23,8: set sprite
   // params:   sprite number (1 byte)
   //           x position (1 word)
   //           y position (1 word)
   int x = buf[2] + buf[3]*256;
   int y = buf[4] + buf[5]*256;
   int p = buf[1];
   sp_save(screen, p, x, y);
   sp_set(screen, p, x, y);
}

static void vdu23_9(uint8_t *buf) {
   // vdu 23,9: unset sprite
   // params:   sprite number (1 byte)
   //           x position (1 word)
   //           y position (1 word)
   int x = buf[2] + buf[3]*256;
   int y = buf[4] + buf[5]*256;
   int p = buf[1];
   sp_restore(screen, p, x, y);
}

static void vdu23_17(uint8_t *buf) {
   int16_t tmp;
   // vdu 23,17: Set subsidary colour effects
   switch (buf[1]) {
   case 0:
      // VDU 23,17,0 - sets tint for text foreground colour
      c_fg_col = ((c_fg_col) & 0x3f) | (buf[2] & 0xc0);
      break;
   case 1:
      // VDU 23,17,1 - sets tint for text background colour
      c_bg_col = ((c_bg_col) & 0x3f) | (buf[2] & 0xc0);
      break;
   case 2:
      // VDU 23,17,2 - sets tint for graphics foreground colour
      g_fg_col = ((g_fg_col) & 0x3f) | (buf[2] & 0xc0);
      break;
   case 3:
      // VDU 23,17,3 - sets tint for graphics background colour
      g_bg_col = ((g_bg_col) & 0x3f) | (buf[2] & 0xc0);
      break;
   case 4:
      // TODO: VDU 23,17,4 - Select colour patterns
      break;
   case 5:
      // TODO: VDU 23,17,5 - Swap text colours
      tmp = c_fg_col;
      c_fg_col = g_bg_col;
      c_bg_col = tmp;
      break;
   case 6:
      // TODO: VDU 23,17,6 - Set ECF origin
      break;
   case 7:
      // TODO VDU 23,17,7 - Set character size and spacing
      // VDU 23,17,7,flags,xsize;ysize;0,0
      break;
   default:
      break;
   }
}

static void vdu23_22(uint8_t *buf) {
   // VDU 23,22,xpixels;ypixels;xchars,ychars,colours,flags
   // User Defined Screen Mode
   int16_t x_pixels = buf[1] | (buf[2] << 8);
   int16_t y_pixels = buf[3] | (buf[4] << 8);
   unsigned int n_colours = buf[7] & 0xff;
   if (n_colours == 0) {
      n_colours = 256;
   }
   screen_mode_t *new_screen = get_screen_mode(CUSTOM_8BPP_SCREEN_MODE);
   new_screen->width = x_pixels;
   new_screen->height = y_pixels;
   new_screen->xeigfactor = 1;
   new_screen->yeigfactor = 1;
   new_screen->ncolour = n_colours - 1;
   change_mode(new_screen);
}

// ==========================================================================
// VDU commands
// ==========================================================================

static void vdu_4(uint8_t *buf) {
   text_at_g_cursor = 0;
   vdu_operation_table[  8].handler = text_cursor_left;
   vdu_operation_table[  9].handler = text_cursor_right;
   vdu_operation_table[ 10].handler = text_cursor_down;
   vdu_operation_table[ 11].handler = text_cursor_up;
   vdu_operation_table[ 12].handler = text_area_clear;
   vdu_operation_table[ 13].handler = text_cursor_col0;
   vdu_operation_table[ 30].handler = text_cursor_home;
   vdu_operation_table[ 31].handler = text_cursor_tab;
   vdu_operation_table[127].handler = text_delete;
   show_cursor();
}

static void vdu_5(uint8_t *buf) {
   hide_cursor();
   text_at_g_cursor = 1;
   vdu_operation_table[  8].handler = graphics_cursor_left;
   vdu_operation_table[  9].handler = graphics_cursor_right;
   vdu_operation_table[ 10].handler = graphics_cursor_down;
   vdu_operation_table[ 11].handler = graphics_cursor_up;
   vdu_operation_table[ 12].handler = graphics_area_clear;
   vdu_operation_table[ 13].handler = graphics_cursor_col0;
   vdu_operation_table[ 30].handler = graphics_cursor_home;
   vdu_operation_table[ 31].handler = graphics_cursor_tab;
   vdu_operation_table[127].handler = graphics_delete;
}


static void vdu_16(uint8_t *buf) {
   prim_clear_graphics_area(screen, screen->get_colour(screen, g_bg_col));
}

static void vdu_17(uint8_t *buf) {
   uint8_t c = buf[1];
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
}

static void vdu_18(uint8_t *buf) {
   uint8_t mode = buf[1];
   uint8_t col  = buf[2];
   prim_set_graphics_plotmode(mode);
   if (col & 128) {
      g_bg_col = col & 63;
   } else {
      g_fg_col = col & 63;
   }
}

static void vdu_19(uint8_t *buf) {
   uint8_t l = buf[1];
   uint8_t p = buf[2];
   uint8_t r = buf[3];
   uint8_t g = buf[4];
   uint8_t b = buf[5];
   // See http://beebwiki.mdfs.net/VDU_19
   if (p < 16) {
      int i = (p & 8) ? 255 : 127;
      b = (p & 4) ? i : 0;
      g = (p & 2) ? i : 0;
      r = (p & 1) ? i : 0;
   }
   screen->set_colour(screen, l, r, g, b);
}

static void vdu_20(uint8_t *buf) {
   screen->reset(screen);
}

static void vdu_22(uint8_t *buf) {
   uint8_t mode = buf[1];
   screen_mode_t *new_screen = get_screen_mode(mode);
   if (new_screen != NULL) {
      change_mode(new_screen);
   } else {
      fb_writes("Unsupported screen mode!\r\n");
   }
}

static void vdu_23(uint8_t *buf) {
#ifdef DEBUG_VDU
   for (int i = 0; i < 10; i++) {
      printf("%X", buf[i]);
      printf("\n\r");
   }
#endif
   // TODO: we could update the vdu23 sub commands to avoid this increment
   buf++; // skip the 23
   switch (buf[0]) {
   case  3: vdu23_3(buf); break;
   case  4: vdu23_4(buf); break;
   case  5: vdu23_5(buf); break;
   case  6: vdu23_6(buf); break;
   case  7: vdu23_7(buf); break;
   case  8: vdu23_8(buf); break;
   case  9: vdu23_9(buf); break;
   case 17: vdu23_17(buf); break;
   case 22: vdu23_22(buf); break;
   }
   // User defined characters
   if (buf[0] >= 128) {
      define_character(screen->font, buf[0], buf + 1);
   }
}

static void vdu_24(uint8_t *buf) {
   g_clip_window_t window;
   window.left   = (int16_t)(buf[1] + (buf[2] << 8));
   window.bottom = (int16_t)(buf[3] + (buf[4] << 8));
   window.right  = (int16_t)(buf[5] + (buf[6] << 8));
   window.top    = (int16_t)(buf[7] + (buf[8] << 8));
#ifdef DEBUG_VDU
   printf("graphics area left:%d bottom:%d right:%d top:%d\r\n",
          window.left, window.bottom, window.right, window.top);
#endif
   // Transform to absolute external coordinates
   window.left   += g_x_origin;
   window.bottom += g_y_origin;
   window.right  += g_x_origin;
   window.top    += g_y_origin;
   // Set the window
   set_graphics_area(screen, &window);
}


static void vdu_25(uint8_t *buf) {
   int col;
   int skew;
   uint8_t g_mode = buf[1];
   int16_t x = (int16_t)(buf[2] + (buf[3] << 8));
   int16_t y = (int16_t)(buf[4] + (buf[5] << 8));

#ifdef DEBUG_VDU
   printf("plot %d %d %d\r\n", g_mode, x, y);
#endif

   if (g_mode & 4) {
      // Relative to the graphics origin
      update_g_cursors(g_x_origin + x, g_y_origin + y);
   } else {
      // Relative to the last point.
      update_g_cursors(g_x_pos + x, g_y_pos + y);
   }
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

   // Transform plotting coordinates to screen coordinates
   int x_pos       = g_x_pos       >> screen->xeigfactor;
   int y_pos       = g_y_pos       >> screen->yeigfactor;
   int x_pos_last1 = g_x_pos_last1 >> screen->xeigfactor;
   int y_pos_last1 = g_y_pos_last1 >> screen->yeigfactor;
   int x_pos_last2 = g_x_pos_last2 >> screen->xeigfactor;
   int y_pos_last2 = g_y_pos_last2 >> screen->yeigfactor;

   if (col >= 0) {

      pixel_t colour = screen->get_colour(screen, col);

      switch (g_mode & 0xF8) {

      case 0:
         // Plot solid line (both endpoints included)
         prim_draw_line(screen, x_pos_last1, y_pos_last1, x_pos, y_pos, colour, g_mode);
         break;
      case 8:
         // Plot solid line (final endpoint omitted)
         prim_draw_line(screen, x_pos_last1, y_pos_last1, x_pos, y_pos, colour, g_mode);
         break;
      case 16:
         // Plot dotted line (both endpoints included)
         prim_draw_line(screen, x_pos_last1, y_pos_last1, x_pos, y_pos, colour, g_mode);
         break;
      case 24:
         // Plot dotted line (final endpoint omitted)
         prim_draw_line(screen, x_pos_last1, y_pos_last1, x_pos, y_pos, colour, g_mode);
         break;
      case 32:
         // Plot solid line (initial endpoint omitted)
         prim_draw_line(screen, x_pos_last1, y_pos_last1, x_pos, y_pos, colour, g_mode);
         break;
      case 40:
         // Plot solid line (final endpoint omitted)
         prim_draw_line(screen, x_pos_last1, y_pos_last1, x_pos, y_pos, colour, g_mode);
         break;
      case 48:
         // Plot dotted line (initial endpoint omitted)
         prim_draw_line(screen, x_pos_last1, y_pos_last1, x_pos, y_pos, colour, g_mode);
         break;
      case 56:
         // Plot dotted line (final endpoint omitted)
         prim_draw_line(screen, x_pos_last1, y_pos_last1, x_pos, y_pos, colour, g_mode);
         break;
      case 64:
         // Plot point
         prim_set_pixel(screen, x_pos, y_pos, colour);
         break;
      case 72:
         // Horizontal line fill (left and right) to non-background
         prim_fill_area(screen, x_pos, y_pos, colour, HL_LR_NB);
         break;
      case 80:
         // Fill a triangle
         prim_fill_triangle(screen, x_pos_last2, y_pos_last2, x_pos_last1, y_pos_last1, x_pos, y_pos, colour);
         break;
      case 88:
         // Horizontal line fill (right only) to background
         prim_fill_area(screen, x_pos, y_pos, colour, HL_RO_BG);
         break;
      case 96:
         // Fill a rectangle
         prim_fill_rectangle(screen, x_pos_last1, y_pos_last1, x_pos, y_pos, colour);
        break;
      case 104:
         // Horizontal line fill (left and right) to foreground
         prim_fill_area(screen, x_pos, y_pos, colour, HL_LR_FG);
         break;
      case 112:
         // Fill a parallelogram
         prim_fill_parallelogram(screen, x_pos_last2, y_pos_last2, x_pos_last1, y_pos_last1, x_pos, y_pos, colour);
         break;
      case 120:
         // Horizontal line fill (right only) to non-foreground
         prim_fill_area(screen, x_pos, y_pos, colour, HL_RO_NF);
         break;
      case 128:
         // Flood fill to non-background
         prim_fill_area(screen, x_pos, y_pos, colour, AF_NONBG);
         break;
      case 136:
         // Flood fill to foreground
         prim_fill_area(screen, x_pos, y_pos, colour, AF_TOFGD);
         break;
      case 144:
         // Draw a circle outline
         prim_draw_circle(screen, x_pos_last1, y_pos_last1, x_pos, y_pos, colour);
         break;
      case 152:
         // Fill a circle
         prim_fill_circle(screen, x_pos_last1, y_pos_last1, x_pos, y_pos, colour);
         break;
      case 160:
         // Plot a circular arc
         prim_draw_arc(screen, x_pos_last2, y_pos_last2, x_pos_last1, y_pos_last1, x_pos, y_pos, colour);
         break;
      case 168:
         // Plot a filled chord segment
         prim_fill_chord(screen, x_pos_last2, y_pos_last2, x_pos_last1, y_pos_last1, x_pos, y_pos, colour);
         break;
      case 176:
         // Plot a filled sector
         prim_fill_sector(screen, x_pos_last2, y_pos_last2, x_pos_last1, y_pos_last1, x_pos, y_pos, colour);
         break;
      case 184:
         // Move/Copy rectangle
         prim_move_copy_rectangle(screen, x_pos_last2, y_pos_last2, x_pos_last1, y_pos_last1, x_pos, y_pos, ((g_mode & 2) == 0));
         break;
      case 192:
         // Plot ellipse outline
         skew = (y_pos > y_pos_last2) ? x_pos - x_pos_last2 : x_pos_last2 - x_pos;
         prim_draw_ellipse(screen, x_pos_last2, y_pos_last2, abs(x_pos_last1 - x_pos_last2), abs(y_pos - y_pos_last2), skew, colour);
         break;
      case 200:
         // Plot solid ellipse
         skew = (y_pos > y_pos_last2) ? x_pos - x_pos_last2 : x_pos_last2 - x_pos;
         prim_fill_ellipse(screen, x_pos_last2, y_pos_last2, abs(x_pos_last1 - x_pos_last2), abs(y_pos - y_pos_last2), skew, colour);
         break;
      default:
         printf("Unsuppported plot code: %d\r\n", g_mode);
         break;
      }
   }
}

static void vdu_26(uint8_t *buf) {
   reset_areas();
}

static void vdu_27(uint8_t *buf) {
   uint8_t c = buf[1];
   switch (c) {
   case 136:
      edit_cursor_left();
      break;
   case 137:
      edit_cursor_right();
      break;
   case 138:
      edit_cursor_down();
      break;
   case 139:
      edit_cursor_up();
      break;
   default:
      vdu_default(&c);
   }
}

static void vdu_28(uint8_t *buf) {
   // left, bottom, right, top
   t_clip_window_t window = {buf[1], buf[2], buf[3], buf[4]};
   set_text_area(&window);
#ifdef DEBUG_VDU
   printf("text area left:%d bottom:%d right:%d top:%d\r\n",
          window.left, window.bottom, window.right, window.top);
#endif
}

static void vdu_29(uint8_t *buf) {
   g_x_origin = (int16_t)(buf[1] + (buf[2] << 8));
   g_y_origin = (int16_t)(buf[3] + (buf[4] << 8));
#ifdef DEBUG_VDU
   printf("graphics origin %d %d\r\n", g_x_origin, g_y_origin);
#endif
}


static void vdu_nop(uint8_t *buf) {
}

static void vdu_default(uint8_t *buf) {
   uint8_t c = buf[0];
   if (text_at_g_cursor) {
      // Draw the character at the graphics cursor (VDU 5 mode)
      int x = g_x_pos >> screen->xeigfactor;
      int y = g_y_pos >> screen->yeigfactor;
      // Only draw the foreground pixels
      pixel_t col = screen->get_colour(screen, g_fg_col);
      prim_draw_character(screen, c, x, y, col);
      // Advance the drawing position
      graphics_cursor_right();
   } else {
      // Draw the character at the text cursor (VDU 4 mode)
      // - Pixel 0,0 is in the bottom left
      // - Character 0,0 is in the top left
      // - So the Y axis needs flipping
      hide_cursor();
      // Draw the foreground and background pixels
      pixel_t fg_col = screen->get_colour(screen, c_fg_col);
      pixel_t bg_col = screen->get_colour(screen, c_bg_col);
      screen->write_character(screen, c, c_x_pos, c_y_pos, fg_col, bg_col);
      show_cursor();
      // Advance the drawing position
      text_cursor_right();
   }
}

// ==========================================================================
// Public interface
// ==========================================================================

void fb_initialize() {
   // Initialize the VDU operation table
   for (unsigned int i = 32; i < sizeof(vdu_operation_table) / sizeof(vdu_operation_t); i++) {
      vdu_operation_table[i].len = 0;
      vdu_operation_table[i].handler = vdu_default;
   }

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
      uint8_t ch = vdu_queue[vdu_rp];
      fb_writec(ch);
      vdu_rp = (vdu_rp + 1) & (VDU_QSIZE - 1);
   }
   cursor_count++;
   if (cursor_count == 250) {
      cursor_interrupt();
      cursor_count = 0;
   }
}

void fb_writec(char ch) {

   static int vdu_index = 0;
   static vdu_operation_t *vdu_op = NULL;
   static uint8_t vdu_buf[VDU_BUF_LEN];

   uint8_t c = (uint8_t) ch;

   // Buffer the character
   vdu_buf[vdu_index] = c;

   // Start of a VDU command
   if (vdu_index == 0) {
      vdu_op = vdu_operation_table + c;
   }

   // End of a VDU command
   if (vdu_index == vdu_op->len) {
      vdu_op->handler(vdu_buf);
      vdu_index = 0;
   } else {
      vdu_index++;
   }
}

void fb_writes(char *string) {
   while (*string) {
      fb_writec(*string++);
   }
}

int fb_get_edit_cursor_x() {
   return e_x_pos;
}

int fb_get_edit_cursor_y() {
   return e_y_pos;
}

int fb_get_edit_cursor_char() {
   if (e_enabled) {
      return screen->read_character(screen, e_x_pos, e_y_pos);
   } else {
      return 0;
   }
}

uint8_t fb_get_g_bg_col() {
   return g_bg_col;
}

uint8_t fb_get_g_fg_col() {
   return g_fg_col;
}
