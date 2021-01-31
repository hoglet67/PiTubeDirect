#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

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
static font_t *font = NULL;
static int16_t font_width;
static int16_t font_height;
static int16_t text_height; // of whole screen
static int16_t text_width;  // of whole screen
static int16_t text_x_min;
static int16_t text_x_max;
static int16_t text_y_min;
static int16_t text_y_max;

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
static void vdu_8(uint8_t *buf);
static void vdu_9(uint8_t *buf);
static void vdu_10(uint8_t *buf);
static void vdu_11(uint8_t *buf);
static void vdu_12(uint8_t *buf);
static void vdu_13(uint8_t *buf);
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
static void vdu_30(uint8_t *buf);
static void vdu_31(uint8_t *buf);
static void vdu_127(uint8_t *buf);
static void vdu_136(uint8_t *buf);
static void vdu_137(uint8_t *buf);
static void vdu_138(uint8_t *buf);
static void vdu_139(uint8_t *buf);

static void vdu_nop(uint8_t *buf);

static void vdu_default(uint8_t *buf);

static vdu_operation_t vdu_operation_table[256] = {
   { 0, vdu_nop }, // 0
   { 0, vdu_nop },
   { 0, vdu_nop },
   { 0, vdu_nop },
   { 0, vdu_4   },
   { 0, vdu_5   },
   { 0, vdu_nop },
   { 0, vdu_nop },
   { 0, vdu_8   }, // 8
   { 0, vdu_9   },
   { 0, vdu_10  },
   { 0, vdu_11  },
   { 0, vdu_12  },
   { 0, vdu_13  },
   { 0, vdu_nop },
   { 0, vdu_nop },
   { 0, vdu_16  }, // 16
   { 1, vdu_17  },
   { 2, vdu_18  },
   { 6, vdu_19  },
   { 0, vdu_20  },
   { 0, vdu_nop },
   { 1, vdu_22  },
   { 9, vdu_23  },
   { 8, vdu_24  }, // 24
   { 5, vdu_25  },
   { 0, vdu_26  },
   { 1, vdu_27  },
   { 4, vdu_28  },
   { 4, vdu_29  },
   { 0, vdu_30  },
   { 2, vdu_31  }
   // Entries 32 to 255 are filled in by fb_initialize
};

// ==========================================================================
// Static methods
// ==========================================================================

static void update_font_size();
static void update_text_area();
static void init_variables();
static void reset_areas();
static void set_text_area(uint8_t left, uint8_t bottom, uint8_t right, uint8_t top);
static int  text_area_active();
static void clear_text_area();
static void scroll_text_area();
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
static void scroll();
static void cursor_left();
static void cursor_right();
static void cursor_up();
static void cursor_down();
static void cursor_col0();
static void cursor_home();
static void cursor_next();
static void update_g_cursors(int16_t x, int16_t y);
static void draw_character(int c, int invert);
static void draw_character_and_advance(int c);
static void change_mode(screen_mode_t *new_screen);

static void update_font_size() {
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
   if (text_x_max >= text_width) {
      text_x_max = text_width - 1;
      if (text_x_min > text_x_max) {
         text_x_min = text_x_max;
      }
   }
   if (text_y_max >= text_height) {
      text_y_max = text_height - 1;
      if (text_y_min > text_y_max) {
         text_y_min = text_y_max;
      }
   }
   // Make sure cursor is in text area
   int16_t tmp_x = c_x_pos;
   int16_t tmp_y = c_y_pos;
   if (tmp_x < text_x_min) {
      tmp_x = text_x_min;
   } else if (tmp_x > text_x_max) {
      tmp_x = text_x_max;
   }
   if (tmp_y < text_y_min) {
      tmp_y = text_y_min;
   } else if (tmp_y > text_y_max) {
      tmp_y = text_y_max;
   }
   if (c_x_pos != tmp_x || c_y_pos != tmp_y) {
      hide_cursor();
      c_x_pos = tmp_x;
      c_y_pos = tmp_y;
      show_cursor();
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
   text_at_g_cursor = 0;

   // Reset text/grapics areas and home cursors (VDU 26 actions)
   reset_areas();

}

static void reset_areas() {
   // Calculate the size of the text area
   update_font_size();
   // left, bottom, right, top
   set_text_area(0, text_height - 1, text_width - 1, 0);
   // Set the graphics origin to 0,0
   fb_set_graphics_origin(0, 0);
   fb_set_graphics_plotmode(0);
   fb_set_graphics_area(screen, 0, 0, (screen->width << screen->xeigfactor) - 1, (screen->height << screen->yeigfactor) - 1);
   // Home the text cursor
   c_x_pos = text_x_min;
   c_y_pos = text_y_min;
   // Home the graphics cursor
   g_x_pos       = 0;
   g_x_pos_last1 = 0;
   g_x_pos_last2 = 0;
   g_y_pos       = 0;
   g_y_pos_last1 = 0;
   g_y_pos_last2 = 0;
}


// 0,0 is the top left
static void set_text_area(uint8_t left, uint8_t bottom, uint8_t right, uint8_t top) {
   if (left > right || right > text_width - 1 || top > bottom || bottom > text_height - 1) {
      return;
   }
   text_x_min = left;
   text_y_min = top;
   text_x_max = right;
   text_y_max = bottom;
   // Update any dependant variabled
   update_text_area();
}


static int text_area_active() {
   if (text_x_min == 0 && text_x_max == text_width - 1 && text_y_min == 0 && text_y_max == text_height - 1) {
      return 0;
   } else {
      return 1;
   }
}

static void clear_text_area() {
   pixel_t bg_col = screen->get_colour(screen, c_bg_col);
   if (text_area_active()) {
      // Pixel 0,0 is in the bottom left
      // Character 0,0 is in the top left
      // So the Y axis needs flipping

      // Top/Left
      int x1 = text_x_min * font_width;
      int y1 = screen->height - 1 - text_y_min * font_height;

      // Bottom/Right
      int x2 = (text_x_max + 1) * font_width;
      int y2 = screen->height - 1 - (text_y_max + 1) * font_height;

      // Clear from top to bottom
      for (int y = y1; y > y2; y--) {
         for (int x = x1; x < x2; x++) {
            screen->set_pixel(screen, x, y, bg_col);
         }
      }
   } else {
      screen->clear(screen, bg_col);
   }
   c_x_pos = text_x_min;
   c_y_pos = text_y_min;
   c_visible = 0;
   show_cursor();
}

static void scroll_text_area() {
   pixel_t bg_col = screen->get_colour(screen, c_bg_col);
   if (text_area_active()) {
      // Top/Left
      int x1 = text_x_min * font_width;
      int y1 = screen->height - 1 - text_y_min * font_height;

      // Bottom/Right
      int x2 = (text_x_max + 1) * font_width;
      int y2 = screen->height - 1 - text_y_max * font_height;

      // Scroll from top to bottom
      for (int y = y1; y > y2; y--) {
         int z = y - font_height;
         for (int x = x1; x < x2; x++) {
            screen->set_pixel(screen, x, y, screen->get_pixel(screen, x, z));
         }
      }
      // Blank the bottom line
      for (int y = y2; y > y2 - font_height; y--) {
         for (int x = x1; x < x2; x++) {
            screen->set_pixel(screen, x, y, bg_col);
         }
      }
   } else {
      screen->scroll(screen, font_height, bg_col);
   }
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
   if (e_y_pos > text_y_min) {
      e_y_pos--;
   } else {
      e_y_pos = text_y_max;
   }
   show_edit_cursor();
}

static void edit_cursor_down() {
   enable_edit_cursor();
   hide_edit_cursor();
   if (e_y_pos < text_y_max) {
      e_y_pos++;
   } else {
      e_y_pos = text_y_min;
   }
   show_edit_cursor();
}

static void edit_cursor_left() {
   enable_edit_cursor();
   hide_edit_cursor();
   if (e_x_pos > text_x_min) {
      e_x_pos--;
   } else {
      e_x_pos = text_x_max;
      if (e_y_pos > text_y_min) {
         e_y_pos--;
      } else {
         e_y_pos = text_y_max;
      }
   }
   show_edit_cursor();
}

static void edit_cursor_right() {
   enable_edit_cursor();
   hide_edit_cursor();
   if (e_x_pos < text_x_max) {
      e_x_pos++;
   } else {
      e_x_pos = text_x_min;
      if (e_y_pos < text_y_max) {
         e_y_pos++;
      } else {
         e_y_pos = text_y_min;
      }
   }
   show_edit_cursor();
}

static void scroll() {
   if (e_enabled) {
      hide_edit_cursor();
   }
   scroll_text_area();
   if (e_enabled) {
      if (e_y_pos > text_y_min) {
         e_y_pos--;
      }
      show_edit_cursor();
   }
}

static void cursor_left() {
   hide_cursor();
   if (c_x_pos > text_x_min) {
      c_x_pos--;
   } else {
      c_x_pos = text_x_max;
   }
   show_cursor();
}

static void cursor_right() {
   hide_cursor();
   if (c_x_pos < text_x_max) {
      c_x_pos++;
   } else {
      c_x_pos = text_x_min;
   }
   show_cursor();
}

static void cursor_up() {
   hide_cursor();
   if (c_y_pos > text_y_min) {
      c_y_pos--;
   } else {
      c_y_pos = text_y_max;
   }
   show_cursor();
}

static void cursor_down() {
   hide_cursor();
   if (c_y_pos < text_y_max) {
      c_y_pos++;
   } else {
      scroll();
   }
   show_cursor();
}

static void cursor_col0() {
   disable_edit_cursor();
   hide_cursor();
   c_x_pos = text_x_min;
   show_cursor();
}

static void cursor_home() {
   hide_cursor();
   c_x_pos = text_x_min;
   c_y_pos = text_y_min;
   show_cursor();
}

static void cursor_next() {
   hide_cursor();
   if (c_x_pos < text_x_max) {
      c_x_pos++;
   } else {
      c_x_pos = text_x_min;
      cursor_down();
   }
   show_cursor();
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
   screen->clear(screen, screen->get_colour(screen, c_bg_col));
   // Show the cursor
   show_cursor();
}




// ==========================================================================
// Drawing Primitives
// ==========================================================================

static void draw_character(int c, int invert) {
   pixel_t fg_col = screen->get_colour(screen, c_fg_col);
   pixel_t bg_col = screen->get_colour(screen, c_bg_col);
   int x = c_x_pos * font_width;
   // Pixel 0,0 is in the bottom left
   // Character 0,0 is in the top left
   // So the Y axis needs flipping
   int y = screen->height - c_y_pos * font_height - 1;
   if (invert) {
      font->draw_character(font, screen, c, x, y, bg_col, fg_col);
   } else {
      font->draw_character(font, screen, c, x, y, fg_col, bg_col);
   }
}

static void draw_character_and_advance(int c) {
   // Draw the next character at the cursor position
   hide_cursor();
   draw_character(c, 0);
   show_cursor();

   // Advance the drawing position
   cursor_next();
}

// ==========================================================================
// VDU 23 commands
// ==========================================================================

static void vdu23_3(uint8_t *buf) {
   // VDU 23,3: select font by name
   // params: eight bytes font name, trailing zeros if name shorter than 8 chars
   // (selects the default font if the lookup fails)
   if (buf[1] == 0) {
      font = get_font_by_number(buf[2]);
   } else {
      font = get_font_by_name((char *)buf+1);
   }
#ifdef DEBUG_VDU
   printf("Font set to %s\r\n", font->name);
#endif
   update_text_area();
}

static void vdu23_4(uint8_t *buf) {
   // VDU 23,4: set font scale
   // params: hor. scale, vert. scale, char spacing, other 5 bytes are ignored
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
}

static void vdu_5(uint8_t *buf) {
   text_at_g_cursor = 1;
}

static void vdu_8(uint8_t *buf) {
   cursor_left();
}

static void vdu_9(uint8_t *buf) {
   cursor_right();
}

static void vdu_10(uint8_t *buf) {
   cursor_down();
}

static void vdu_11(uint8_t *buf) {
   cursor_up();
}

static void vdu_12(uint8_t *buf) {
   clear_text_area();
}

static void vdu_13(uint8_t *buf) {
   cursor_col0();
}

static void vdu_16(uint8_t *buf) {
   fb_clear_graphics_area(screen, screen->get_colour(screen, g_bg_col));
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
   fb_set_graphics_plotmode(mode);
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
      define_character(font, buf[0], buf + 1);
   }
}

static void vdu_24(uint8_t *buf) {
   int16_t x1 = (int16_t)(buf[1] + (buf[2] << 8));
   int16_t y1 = (int16_t)(buf[3] + (buf[4] << 8));
   int16_t x2 = (int16_t)(buf[5] + (buf[6] << 8));
   int16_t y2 = (int16_t)(buf[7] + (buf[8] << 8));
   fb_set_graphics_area(screen, x1, y1, x2, y2);
#ifdef DEBUG_VDU
   printf("graphics area %d %d %d %d\r\n", x1, y1, x2, y2);
#endif
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
      // Absolute position X, Y.
      update_g_cursors(x, y);
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

   if (col >= 0) {

      pixel_t colour = screen->get_colour(screen, col);

      switch (g_mode & 0xF8) {

      case 0:
         // Plot solid line (both endpoints included)
         fb_draw_line(screen, g_x_pos_last1, g_y_pos_last1, g_x_pos, g_y_pos, colour, g_mode);
         break;
      case 8:
         // Plot solid line (final endpoint omitted)
         fb_draw_line(screen, g_x_pos_last1, g_y_pos_last1, g_x_pos, g_y_pos, colour, g_mode);
         break;
      case 16:
         // Plot dotted line (both endpoints included)
         fb_draw_line(screen, g_x_pos_last1, g_y_pos_last1, g_x_pos, g_y_pos, colour, g_mode);
         break;
      case 24:
         // Plot dotted line (final endpoint omitted)
         fb_draw_line(screen, g_x_pos_last1, g_y_pos_last1, g_x_pos, g_y_pos, colour, g_mode);
         break;
      case 32:
         // Plot solid line (initial endpoint omitted)
         fb_draw_line(screen, g_x_pos_last1, g_y_pos_last1, g_x_pos, g_y_pos, colour, g_mode);
         break;
      case 40:
         // Plot solid line (final endpoint omitted)
         fb_draw_line(screen, g_x_pos_last1, g_y_pos_last1, g_x_pos, g_y_pos, colour, g_mode);
         break;
      case 48:
         // Plot dotted line (initial endpoint omitted)
         fb_draw_line(screen, g_x_pos_last1, g_y_pos_last1, g_x_pos, g_y_pos, colour, g_mode);
         break;
      case 56:
         // Plot dotted line (final endpoint omitted)
         fb_draw_line(screen, g_x_pos_last1, g_y_pos_last1, g_x_pos, g_y_pos, colour, g_mode);
         break;
      case 64:
         // Plot point
         fb_set_pixel(screen, g_x_pos, g_y_pos, colour);
         break;
      case 72:
         // Horizontal line fill (left and right) to non-background
         fb_fill_area(screen, g_x_pos, g_y_pos, colour, HL_LR_NB);
         break;
      case 80:
         // Fill a triangle
         fb_fill_triangle(screen, g_x_pos_last2, g_y_pos_last2, g_x_pos_last1, g_y_pos_last1, g_x_pos, g_y_pos, colour);
         break;
      case 88:
         // Horizontal line fill (right only) to background
         fb_fill_area(screen, g_x_pos, g_y_pos, colour, HL_RO_BG);
         break;
      case 96:
         // Fill a rectangle
         fb_fill_rectangle(screen, g_x_pos_last1, g_y_pos_last1, g_x_pos, g_y_pos, colour);
        break;
      case 104:
         // Horizontal line fill (left and right) to foreground
         fb_fill_area(screen, g_x_pos, g_y_pos, colour, HL_LR_FG);
         break;
      case 112:
         // Fill a parallelogram
         fb_fill_parallelogram(screen, g_x_pos_last2, g_y_pos_last2, g_x_pos_last1, g_y_pos_last1, g_x_pos, g_y_pos, colour);
         break;
      case 120:
         // Horizontal line fill (right only) to non-foreground
         fb_fill_area(screen, g_x_pos, g_y_pos, colour, HL_RO_NF);
         break;
      case 128:
         // Flood fill to non-background
         fb_fill_area(screen, g_x_pos, g_y_pos, colour, AF_NONBG);
         break;
      case 136:
         // Flood fill to foreground
         fb_fill_area(screen, g_x_pos, g_y_pos, colour, AF_TOFGD);
         break;
      case 144:
         // Draw a circle outline
         fb_draw_circle(screen, g_x_pos_last1, g_y_pos_last1, g_x_pos, g_y_pos, colour);
         break;
      case 152:
         // Fill a circle
         fb_fill_circle(screen, g_x_pos_last1, g_y_pos_last1, g_x_pos, g_y_pos, colour);
         break;
      case 160:
         // Plot a circular arc
         fb_draw_arc(screen, g_x_pos_last2, g_y_pos_last2, g_x_pos_last1, g_y_pos_last1, g_x_pos, g_y_pos, colour);
         break;
      case 168:
         // Plot a filled chord segment
         fb_fill_chord(screen, g_x_pos_last2, g_y_pos_last2, g_x_pos_last1, g_y_pos_last1, g_x_pos, g_y_pos, colour);
         break;
      case 176:
         // Plot a filled sector
         fb_fill_sector(screen, g_x_pos_last2, g_y_pos_last2, g_x_pos_last1, g_y_pos_last1, g_x_pos, g_y_pos, colour);
         break;
      case 184:
         // Move/Copy rectangle
         fb_move_copy_rectangle(screen, g_x_pos_last2, g_y_pos_last2, g_x_pos_last1, g_y_pos_last1, g_x_pos, g_y_pos, ((g_mode & 2) == 0));
         break;
      case 192:
         // Plot ellipse outline
         skew = (g_y_pos > g_y_pos_last2) ? g_x_pos - g_x_pos_last2 : g_x_pos_last2 - g_x_pos;
         fb_draw_ellipse(screen, g_x_pos_last2, g_y_pos_last2, abs(g_x_pos_last1 - g_x_pos_last2), abs(g_y_pos - g_y_pos_last2), skew, colour);
         break;
      case 200:
         // Plot solid ellipse
         skew = (g_y_pos > g_y_pos_last2) ? g_x_pos - g_x_pos_last2 : g_x_pos_last2 - g_x_pos;
         fb_fill_ellipse(screen, g_x_pos_last2, g_y_pos_last2, abs(g_x_pos_last1 - g_x_pos_last2), abs(g_y_pos - g_y_pos_last2), skew, colour);
         break;
      default:
         printf("Unsuppported plot code: %d\r\n", g_mode);
         break;
      }

      // Roland also had implemented these outlines
      // fb_draw_rectangle(screen, g_x_pos, g_y_pos, g_x_pos_last1, g_y_pos_last1, colour);
      // fb_draw_parallelogram(screen, g_x_pos, g_y_pos, g_x_pos_last1, g_y_pos_last1, g_x_pos_last2, g_y_pos_last2, colour);
      // fb_draw_triangle(screen, g_x_pos, g_y_pos, g_x_pos_last1, g_y_pos_last1, g_x_pos_last2, g_y_pos_last2, colour);
   }
}

static void vdu_26(uint8_t *buf) {
   reset_areas();
}

static void vdu_27(uint8_t *buf) {
   uint8_t c = buf[1];
   draw_character_and_advance(c);
}

static void vdu_28(uint8_t *buf) {
   uint8_t left   = buf[1];
   uint8_t bottom = buf[2];
   uint8_t right  = buf[3];
   uint8_t top    = buf[4];
   set_text_area(left, bottom, right, top);
#ifdef DEBUG_VDU
   printf("text area left:%d bottom:%d right:%d top:%d\r\n", left, bottom, right, top);
#endif
}

static void vdu_29(uint8_t *buf) {
   int16_t x = (int16_t)(buf[1] + (buf[2] << 8));
   int16_t y = (int16_t)(buf[3] + (buf[4] << 8));
   fb_set_graphics_origin(x, y);
#ifdef DEBUG_VDU
   printf("graphics origin %d %d\r\n", x, y);
#endif
}

static void vdu_30(uint8_t *buf) {
   cursor_home();
}

static void vdu_31(uint8_t *buf) {
   uint8_t x = buf[1];
   uint8_t y = buf[2];
#ifdef DEBUG_VDU
   printf("cursor move to %d %d\r\n", x, y);
#endif
   // Take account of current text window
   x += text_x_min;
   y += text_y_min;
   if (x <= text_x_max && y <= text_y_max) {
      hide_cursor();
      c_x_pos = x;
      c_y_pos = y;
      show_cursor();
   }
}

static void vdu_127(uint8_t *buf) {
   cursor_left();
   draw_character(32, 1);
}

 static void vdu_136(uint8_t *buf) {
   edit_cursor_left();
}

static void vdu_137(uint8_t *buf) {
   edit_cursor_right();
}

static void vdu_138(uint8_t *buf) {
   edit_cursor_down();
}

static void vdu_139(uint8_t *buf) {
   edit_cursor_up();
}

static void vdu_nop(uint8_t *buf) {
}

static void vdu_default(uint8_t *buf) {
   uint8_t c = buf[0];
   if (text_at_g_cursor) {
      font->draw_character(font, screen, c, g_x_pos, g_y_pos, g_fg_col, g_bg_col);
      update_g_cursors(g_x_pos + font_width, g_y_pos);
   } else {
      draw_character_and_advance(c);
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
   vdu_operation_table[127].handler = vdu_127;
   vdu_operation_table[136].handler = vdu_136;
   vdu_operation_table[137].handler = vdu_137;
   vdu_operation_table[138].handler = vdu_138;
   vdu_operation_table[139].handler = vdu_139;

   font = get_font_by_number(DEFAULT_FONT);

   if (!font) {
      printf("No font loaded\n\r");
   } else {
      printf("Font %s loaded\n\r", font->name);
   }

   init_variables();

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

uint8_t fb_get_g_bg_col() {
   return g_bg_col;
}

uint8_t fb_get_g_fg_col() {
   return g_fg_col;
}
