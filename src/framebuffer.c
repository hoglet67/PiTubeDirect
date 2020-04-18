// #define DEBUG_VDU

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include <math.h>
#include "rpi-armtimer.h"
#include "rpi-interrupts.h"
#include "rpi-aux.h"
#include "rpi-gpio.h"
#include "rpi-mailbox-interface.h"
#include "startup.h"
#include "framebuffer.h"
#include "v3d.h"
#include "fonts.h"

// Character colour / cursor position
static int16_t c_bg_col;
static int16_t c_fg_col;
static int16_t c_x_pos;
static int16_t c_y_pos;

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

#include "vdu23.h"

#define BBC_X_RESOLUTION 1280
#define BBC_Y_RESOLUTION 1024

#define SCREEN_WIDTH    640
#define SCREEN_HEIGHT   480

//#define BPP32

#ifdef BPP32
#define SCREEN_DEPTH    32

#define ALPHA 0xFF000000
#define RED   0xFFFF0000
#define GREEN 0xFF00FF00
#define BLUE  0xFF0000FF

#else

#define SCREEN_DEPTH    16

// R4 R3 R2 R1 R0 G5 G4 G3 G2 G1 G0 B4 B3 B2 B1 B0
#define ALPHA 0x0000
#define RED   0xF800
#define GREEN 0x07E0
#define BLUE  0x001F

#endif

#define D_RED   ((((RED & ~ALPHA)   >> 1) & RED)   | ALPHA)
#define D_GREEN ((((GREEN & ~ALPHA) >> 1) & GREEN) | ALPHA)
#define D_BLUE  ((((BLUE & ~ALPHA)  >> 1) & BLUE)  | ALPHA)

// Fill modes:
#define HL_LR_NB 1 // Horizontal line fill (left & right) to non-background
#define HL_RO_BG 2 // Horizontal line fill (right only) to background
#define HL_LR_FG 3 // Horizontal line fill (left & right) to foreground
#define HL_RO_NF 4 // Horizontal line fill (right only) to non-foreground
#define AF_NONBG 5 // Flood (area fill) to non-background
#define AF_TOFGD 6 // Flood (area fill) to foreground


static unsigned int default_colour_table[] = {
   0x0000,                   // Dark Black
   D_RED,                    // Dark Red
   D_GREEN,                  // Dark Green
   D_RED | D_GREEN,          // Dark Yellow
   D_BLUE,                   // Dark Blue
   D_RED | D_BLUE,           // Dark Magenta
   D_GREEN | D_BLUE,         // Dark Cyan
   D_RED | D_GREEN | D_BLUE, // Dark White

   0x2945,                   // Light Black
   RED,                      // Red
   GREEN,                    // Green
   RED | GREEN,              // Yellow
   BLUE,                     // Blue
   RED | BLUE,               // Magenta
   GREEN | BLUE,             // Cyan
   RED | GREEN | BLUE        // White

};

static unsigned int colour_table[256];

#define NORMAL    0
#define IN_VDU4   4
#define IN_VDU5   5
#define IN_VDU17  17
#define IN_VDU18  18
#define IN_VDU19  19
#define IN_VDU22  22
#define IN_VDU23  23
#define IN_VDU25  25
#define IN_VDU29  29
#define IN_VDU31  31

static unsigned char* fb = NULL;
static int width = 0, height = 0, bpp = 0, pitch = 0;

void fb_init_variables() {

    // Character colour / cursor position
   c_bg_col = 0;
   c_fg_col = 15;
   c_x_pos  = 0;
   c_y_pos  = 0;

   // Graphics colour / cursor position
   g_bg_col      = 0;
   g_fg_col      = 15;
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
}

void fb_scroll() {
   _fast_scroll(fb, fb + 12 * pitch, (height - 12) * pitch);
   _fast_clear(fb + (height - 12) * pitch, 0, 12 * pitch);
}

void fb_clear() {
   // initialize the cursor positions, origin, colours, etc.
   // TODO: clearing all of these may not be strictly correct
   fb_init_variables();
   // clear frame buffer
   memset((void *)fb, colour_table[c_bg_col], height * pitch);
}

void fb_cursor_left() {
   if (c_x_pos > 0) {
      c_x_pos--;
   } else {
      c_x_pos = 79;
   }
}

void fb_cursor_right() {
   if (c_x_pos < 79) {
      c_x_pos++;
   } else {
      c_x_pos = 0;
   }
}

void fb_cursor_up() {
   if (c_y_pos > 0) {
      c_y_pos--;
   } else {
      c_y_pos = 39;
   }
}

void fb_cursor_down() {
   if (c_y_pos < 39) {
      c_y_pos++;
   } else {
      fb_scroll();
   }
}

void fb_cursor_col0() {
   c_x_pos = 0;
}

void fb_cursor_home() {
   c_x_pos = 0;
   c_y_pos = 0;
}


void fb_cursor_next() {
   if (c_x_pos < 79) {
      c_x_pos++;
   } else {
      c_x_pos = 0;
      fb_cursor_down();
   }
}

void fb_cursor_invert() {

}

volatile int d;

void fb_initialize() {

   int col, x, y;
    rpi_mailbox_property_t *mp;

    /* Copy default colour table */
    init_colour_table();

    /* Initialise a framebuffer... */
    RPI_PropertyInit();
    RPI_PropertyAddTag( TAG_ALLOCATE_BUFFER );
    RPI_PropertyAddTag( TAG_SET_PHYSICAL_SIZE, SCREEN_WIDTH, SCREEN_HEIGHT );
    RPI_PropertyAddTag( TAG_SET_VIRTUAL_SIZE, SCREEN_WIDTH, SCREEN_HEIGHT * 2 );
    RPI_PropertyAddTag( TAG_SET_DEPTH, SCREEN_DEPTH );
    RPI_PropertyAddTag( TAG_GET_PITCH );
    RPI_PropertyAddTag( TAG_GET_PHYSICAL_SIZE );
    RPI_PropertyAddTag( TAG_GET_DEPTH );
    RPI_PropertyProcess();


    if( ( mp = RPI_PropertyGet( TAG_GET_PHYSICAL_SIZE ) ) )
    {
        width = mp->data.buffer_32[0];
        height = mp->data.buffer_32[1];

        printf( "Initialised Framebuffer: %dx%d ", width, height );
    }

    if( ( mp = RPI_PropertyGet( TAG_GET_DEPTH ) ) )
    {
        bpp = mp->data.buffer_32[0];
        printf( "%dbpp\r\n", bpp );
    }

    if( ( mp = RPI_PropertyGet( TAG_GET_PITCH ) ) )
    {
        pitch = mp->data.buffer_32[0];
        printf( "Pitch: %d bytes\r\n", pitch );
    }

    if( ( mp = RPI_PropertyGet( TAG_ALLOCATE_BUFFER ) ) )
    {
        fb = (unsigned char*)mp->data.buffer_32[0];
        printf( "Framebuffer address: %8.8X\r\n", (unsigned int)fb );
    }

    // On the Pi 2/3 the mailbox returns the address with bits 31..30 set, which is wrong
    fb = (unsigned char *)(((unsigned int) fb) & 0x3fffffff);

    // Change bpp from bits to bytes
    bpp >>= 3;

    fb_clear();

    // Initialize Timer Interrupts
    RPI_ArmTimerInit();
    RPI_GetIrqController()->Enable_Basic_IRQs = RPI_BASIC_ARM_TIMER_IRQ;

    fb_writes("\r\n\r\nACORN ATOM PI-VDU V0.86\r\n>");
    #ifdef DEBUG_VDU
    fb_writes("Kernel debugging is enabled, execution might be slow!\r\n");
    #endif
    printf("\r\n\r\nScreen size: %d,%d\r\n>", width, height);

#ifdef BPP32
    for (y = 0; y < 16; y++) {
       uint32_t *fbptr = (uint32_t *) (fb + pitch * y);
       for (col = 23; col >= 0; col--) {
          for (x = 0; x < 8; x++) {
             *fbptr++ = 1 << col;
          }
       }
    }
#else
    for (y = 0; y < 16; y++) {
       uint16_t *fbptr = (uint16_t *) (fb + pitch * y);
       for (col = 15; col >= 0; col--) {
          for (x = 0; x < 8; x++) {
             *fbptr++ = 1 << col;
          }
       }
    }
#endif

    for (d = 0; d < 1000000; d++);

    // Uncomment this for testing from the serial port
    //while (1) {
    //        int c = RPI_AuxMiniUartRead();
    //        fb_writec(c);
    //}
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

void fb_draw_character(int c, int invert, int eor) {
   int c_fgol;
   int c_bgol;
   int i;
   int j;
   // Map the character to a section of the 6847 font data
   c *= 12;
   // Copy the character into the frame buffer
   for (i = 0; i < 12; i++) {
      int data = fontdata[c + i];
      unsigned char *fbptr = fb + c_x_pos * 8 * bpp + (c_y_pos * 12 + i) * pitch;
      if (invert) {
         data ^= 0xff;
      }
      c_fgol = colour_table[c_fg_col];
      c_bgol = colour_table[c_bg_col];

      for (j = 0; j < 8; j++) {
         int col = (data & 0x80) ? c_fgol : c_bgol;
#ifdef BPP32
         if (eor) {
            *(uint32_t *)fbptr ^= col;
         } else {
            *(uint32_t *)fbptr = col;
         }
         fbptr += 4;
#else
         if (eor) {
            *(uint16_t *)fbptr ^= col;
         } else {
            *(uint16_t *)fbptr = col;
         }
         fbptr += 2;
#endif
         data <<= 1;
      }
   }
}

void init_colour_table() {
   int i;
   for (i=0; i<16; i++) {
      colour_table[i] = default_colour_table[i];
   }
}

void fb_writec(char ch) {
   // TODO: Deal with overflow
   vdu_queue[vdu_wp] = ch;
   vdu_wp = (vdu_wp + 1) & (VDU_QSIZE - 1);
}


void fb_writec_int(char ch) {
   int invert;
   unsigned char c = (unsigned char) ch;

   static int state = NORMAL;
   static int count = 0;
   static int16_t x_tmp = 0;
   static int16_t y_tmp = 0;
   static int i, j;

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
         i = c;
         j = 0;
         break;
      case 1:
         j = (c&0x1F) << 11;
         break;
      case 2:
         j += (c&0x3F) << 5;
         break;
      case 3:
         j += (c&0x1F);
         colour_table[i] = j;
         break;
      }
      count++;
      if (count == 4) {
         if (i == 255) {
            init_colour_table();
         }
         state = NORMAL;
      }
      return;

   } else if (state == IN_VDU22) {
      count++;
      if (count == 1) {
         fb_clear();
         fb_draw_character(32, 1, 1);
         state = NORMAL;
      }
      return;

   } else if (state == IN_VDU23) {
      vdu23buf[count] = c;
      if (count == 0) {
         vdu23cnt = vdu23buf[0] == 7 ? 258 : 9;
      }
      count++;
      if (count == vdu23cnt) {
         do_vdu23();
         state = NORMAL;
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
      fb_draw_character(32, 1, 1);
      fb_cursor_left();
      fb_draw_character(32, 1, 1);
      break;

   case 9:
      fb_draw_character(32, 1, 1);
      fb_cursor_right();
      fb_draw_character(32, 1, 1);
      break;

   case 10:
      fb_draw_character(32, 1, 1);
      fb_cursor_down();
      fb_draw_character(32, 1, 1);
      break;

   case 11:
      fb_draw_character(32, 1, 1);
      fb_cursor_up();
      fb_draw_character(32, 1, 1);
      break;

   case 12:
      fb_clear();
      fb_draw_character(32, 1, 1);
      break;

   case 13:
      fb_draw_character(32, 1, 1);
      fb_cursor_col0();
      fb_draw_character(32, 1, 1);
      break;

   case 16:
      fb_clear();
      fb_draw_character(32, 1, 1);
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
      fb_draw_character(32, 1, 1);
      fb_cursor_home();
      fb_draw_character(32, 1, 1);
      break;

   case 31:
      state = IN_VDU31;
      count = 0;
      return;

   case 127:
      fb_draw_character(32, 1, 1);
      fb_cursor_left();
      fb_draw_character(32, 1, 0);
      break;

   default:

      if (g_cursor == IN_VDU4) {
         // Convert c to index into 6847 character generator ROM
         // chars 20-3F map to 20-3F
         // chars 40-5F map to 00-1F
         // chars 60-7F map to 40-5F
         invert = c >= 0x80;
         c &= 0x7f;
         if (c < 0x20) {
            return;
         } else if (c >= 0x40 && c < 0x5F) {
            c -= 0x40;
         } else if (c >= 0x60) {
            c -= 0x20;
         }

         // Erase the cursor
         fb_draw_character(32, 1, 1);

         // Draw the next character
         fb_draw_character(c, invert, 0);

         // Advance the drawing position
         fb_cursor_next();

         // Draw the cursor
         fb_draw_character(32, 1, 1);
      } else {
         gr_draw_character(c, g_x_pos, g_y_pos, g_fg_col);
         update_g_cursors(g_x_pos+font_scale_w*font_width+font_spacing, g_y_pos);
      }
   }
}

void fb_writes(char *string) {
   while (*string) {
      fb_writec_int(*string++);
   }
}

/* The difference between fb_putpixel and fb_setpixel is that
 * fp_putpixel gets the colour from the logical colour table
 * and fb_setpixel writes the 16 or 32 bits colour.
 */
void fb_putpixel(int x, int y, unsigned int colour) {
   x = ((x + g_x_origin) * SCREEN_WIDTH)  / BBC_X_RESOLUTION;
   y = ((y + g_y_origin) * SCREEN_HEIGHT) / BBC_Y_RESOLUTION;
   if (x < 0  || x > SCREEN_WIDTH - 1) {
      return;
   }
   if (y < 0 || y > SCREEN_HEIGHT - 1) {
      return;
   }
#ifdef BPP32
   uint32_t *fbptr = (uint32_t *)(fb + (SCREEN_HEIGHT - y - 1) * pitch + x * 4);
#else
   uint16_t *fbptr = (uint16_t *)(fb + (SCREEN_HEIGHT - y - 1) * pitch + x * 2);
#endif
   *fbptr = colour_table[colour];
}

void fb_setpixel(int x, int y, unsigned int colour) {
   x = ((x + g_x_origin) * SCREEN_WIDTH)  / BBC_X_RESOLUTION;
   y = ((y + g_y_origin) * SCREEN_HEIGHT) / BBC_Y_RESOLUTION;
   if (x < 0  || x > SCREEN_WIDTH - 1) {
      return;
   }
   if (y < 0 || y > SCREEN_HEIGHT - 1) {
      return;
   }
#ifdef BPP32
   uint32_t *fbptr = (uint32_t *)(fb + (SCREEN_HEIGHT - y - 1) * pitch + x * 4);
#else
   uint16_t *fbptr = (uint16_t *)(fb + (SCREEN_HEIGHT - y - 1) * pitch + x * 2);
#endif
   *fbptr = colour;
}

unsigned int fb_getpixel(int x, int y) {
   x = ((x + g_x_origin) * SCREEN_WIDTH)  / BBC_X_RESOLUTION;
   y = ((y + g_y_origin) * SCREEN_HEIGHT) / BBC_Y_RESOLUTION;
   if (x < 0  || x > SCREEN_WIDTH - 1) {
      return g_bg_col;
   }
   if (y < 0 || y > SCREEN_HEIGHT - 1) {
      return g_bg_col;
   }
#ifdef BPP32
   uint32_t *fbptr = (uint32_t *)(fb + (SCREEN_HEIGHT - y - 1) * pitch + x * 4);
#else
   uint16_t *fbptr = (uint16_t *)(fb + (SCREEN_HEIGHT - y - 1) * pitch + x * 2);
#endif
   return *fbptr;
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
   x = ((x + g_x_origin) * SCREEN_WIDTH)  / BBC_X_RESOLUTION;
   y = ((y + g_y_origin) * SCREEN_HEIGHT) / BBC_Y_RESOLUTION;
   x2 = ((x2 + g_x_origin) * SCREEN_WIDTH)  / BBC_X_RESOLUTION;
   y2 = ((y2 + g_y_origin) * SCREEN_HEIGHT) / BBC_Y_RESOLUTION;
   x3 = ((x3 + g_x_origin) * SCREEN_WIDTH)  / BBC_X_RESOLUTION;
   y3 = ((y3 + g_y_origin) * SCREEN_HEIGHT) / BBC_Y_RESOLUTION;
   // Flip y axis
   y = SCREEN_HEIGHT - 1 - y;
   y2 = SCREEN_HEIGHT - 1 - y2;
   y3 = SCREEN_HEIGHT - 1 - y3;
   colour = colour_table[colour];
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
         if (fb_getpixel(x_right,y) == colour_table[g_bg_col] && x_right <= BBC_X_RESOLUTION) {
            x_right += BBC_X_RESOLUTION/SCREEN_WIDTH;   // speeds up but might fail if not integer
         } else {
            stop = 1;
         }
      }
      stop = 0;
      x = save_x - 1;
      while (! stop) {
         if (fb_getpixel(x_left,y) == colour_table[g_bg_col] && x_left >= 0) {
            x_left -= BBC_X_RESOLUTION/SCREEN_WIDTH;    // speeds up but might fail if not integer
         } else {
            stop = 1;
         }
      }
      break;

   case HL_RO_BG:
      while (! stop) {
         if (fb_getpixel(x_right,y) != colour_table[g_bg_col] && x_right <= BBC_X_RESOLUTION) {
            x_right += BBC_X_RESOLUTION/SCREEN_WIDTH;   // speeds up but might fail if not integer
         } else {
            stop = 1;
         }
      }
      break;

   case HL_LR_FG:
      while (! stop) {
         if (fb_getpixel(x_right,y) != colour_table[g_fg_col] && x_right <= BBC_X_RESOLUTION) {
            x_right += BBC_X_RESOLUTION/SCREEN_WIDTH;   // speeds up but might fail if not integer
         } else {
            stop = 1;
         }
      }
      stop = 0;
      x = save_x - 1;
      while (! stop) {
         if (fb_getpixel(x_left,y) != colour_table[g_fg_col] && x_left >= 0) {
            x_left -= BBC_X_RESOLUTION/SCREEN_WIDTH;    // speeds up but might fail if not integer
         } else {
            stop = 1;
         }
      }
      break;


   case HL_RO_NF:
      while (! stop) {
         if (fb_getpixel(x_right,y) != colour_table[g_fg_col] && x_right <= BBC_X_RESOLUTION) {
            x_right += BBC_X_RESOLUTION/SCREEN_WIDTH;   // speeds up but might fail if not integer
         } else {
            stop = 1;
         }
      }
      break;

   case AF_NONBG:
      // going up
      while (! stop) {
         if (fb_getpixel(x,y) == colour_table[g_bg_col] && y <= BBC_Y_RESOLUTION) {
            fb_fill_area(x, y, colour, HL_LR_NB);
            // As the BBC_Y_RESOLUTION is not a multiple of SCREEN_HEIGHT we have to increment
            // y until the physical y-coordinate increases. If we don't do that and simply increment
            // y by 2 then at some point the physical y-coordinate is the same as the previous drawn
            // line and the floodings stops. This also speeds up drawing because each physical line
            // is only drawn once.
            real_y = ((y + g_y_origin) * SCREEN_HEIGHT) / BBC_Y_RESOLUTION;
            while(((y + g_y_origin) * SCREEN_HEIGHT) / BBC_Y_RESOLUTION == real_y) {
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
      y = save_y - BBC_Y_RESOLUTION/SCREEN_HEIGHT;
      while (! stop) {
         if (fb_getpixel(x,y) == colour_table[g_bg_col] && y >= 0) {
            fb_fill_area(x, y, colour, HL_LR_NB);
            real_y = ((y + g_y_origin) * SCREEN_HEIGHT) / BBC_Y_RESOLUTION;
            while(((y + g_y_origin) * SCREEN_HEIGHT) / BBC_Y_RESOLUTION == real_y) {
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
         if (fb_getpixel(x,y) != colour_table[g_fg_col] && y <= BBC_Y_RESOLUTION) {
            fb_fill_area(x, y, colour, HL_LR_FG);
            real_y = ((y + g_y_origin) * SCREEN_HEIGHT) / BBC_Y_RESOLUTION;
            while(((y + g_y_origin) * SCREEN_HEIGHT) / BBC_Y_RESOLUTION == real_y) {
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
      y = save_y - BBC_Y_RESOLUTION/SCREEN_HEIGHT;
      while (! stop) {
         if (fb_getpixel(x,y) != colour_table[g_fg_col] && y >= 0) {
            fb_fill_area(x, y, colour, HL_LR_NB);
            real_y = ((y + g_y_origin) * SCREEN_HEIGHT) / BBC_Y_RESOLUTION;
            while(((y + g_y_origin) * SCREEN_HEIGHT) / BBC_Y_RESOLUTION == real_y) {
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
   return width;
}

uint32_t fb_get_height() {
   return height;
}

uint32_t fb_get_bpp32() {
#ifdef BPP32
   return 1;
#else
   return 0;
#endif
}

void fb_process_vdu_queue() {
   _data_memory_barrier();
   RPI_GetArmTimer()->IRQClear = 0;
   while (vdu_rp != vdu_wp) {
      char ch = vdu_queue[vdu_rp];
      fb_writec_int(ch);
      vdu_rp = (vdu_rp + 1) & (VDU_QSIZE - 1);
   }
}
