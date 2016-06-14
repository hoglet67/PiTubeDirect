#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include "rpi-aux.h"
#include "rpi-mailbox-interface.h"
#include "framebuffer.h"

#define SCREEN_WIDTH    640
#define SCREEN_HEIGHT   480

#define BPP16

#ifdef BPP32
#define SCREEN_DEPTH    32

#define PUTPIXEL fb_putpixel_32bpp

#define ALPHA 0xFF000000
#define RED   0xFFFF0000
#define GREEN 0xFF00FF00
#define BLUE  0xFF0000FF
 
#else

#define SCREEN_DEPTH    16

#define PUTPIXEL fb_putpixel_16bpp
// R4 R3 R2 R1 R0 G5 G4 G3 G2 G1 G0 B4 B3 B2 B1 B0

#define ALPHA 0x0000
#define RED   0xF800
#define GREEN 0x07E0
#define BLUE  0x001F

#endif

#define D_RED   ((((RED & ~ALPHA)   >> 1) & RED)   | ALPHA)
#define D_GREEN ((((GREEN & ~ALPHA) >> 1) & GREEN) | ALPHA)
#define D_BLUE  ((((BLUE & ~ALPHA)  >> 1) & BLUE)  | ALPHA)

static int colour_table[] = {
   0x0000,                   // Black
   D_RED,                    // Dark Red
   D_GREEN,                  // Dark Green
   D_RED | D_GREEN,          // Dark Yellow
   D_BLUE,                   // Dark Blue
   D_RED | D_BLUE,           // Dark Magenta
   D_GREEN | D_BLUE,         // Dark Cyan
   D_RED | D_GREEN | D_BLUE, // Dark White

   0x0000,                   // Dark Black
   RED,                      // Red
   GREEN,                    // Green
   RED | GREEN,              // Yellow
   BLUE,                     // Blue
   RED | BLUE,               // Magenta
   GREEN | BLUE,             // Cyan
   RED | GREEN | BLUE        // White

};

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

// 6847 font data

uint8_t fontdata[] = 
{
	0x00, 0x00, 0x00, 0x1c, 0x22, 0x02, 0x1a, 0x2a, 0x2a, 0x1c, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x08, 0x14, 0x22, 0x22, 0x3e, 0x22, 0x22, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x3c, 0x12, 0x12, 0x1c, 0x12, 0x12, 0x3c, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x1c, 0x22, 0x20, 0x20, 0x20, 0x22, 0x1c, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x3c, 0x12, 0x12, 0x12, 0x12, 0x12, 0x3c, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x3e, 0x20, 0x20, 0x3c, 0x20, 0x20, 0x3e, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x3e, 0x20, 0x20, 0x3c, 0x20, 0x20, 0x20, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x1e, 0x20, 0x20, 0x26, 0x22, 0x22, 0x1e, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x22, 0x22, 0x22, 0x3e, 0x22, 0x22, 0x22, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x1c, 0x08, 0x08, 0x08, 0x08, 0x08, 0x1c, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x02, 0x02, 0x02, 0x02, 0x22, 0x22, 0x1c, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x22, 0x24, 0x28, 0x30, 0x28, 0x24, 0x22, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x3e, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x22, 0x36, 0x2a, 0x2a, 0x22, 0x22, 0x22, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x22, 0x32, 0x2a, 0x26, 0x22, 0x22, 0x22, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x3e, 0x22, 0x22, 0x22, 0x22, 0x22, 0x3e, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x3c, 0x22, 0x22, 0x3c, 0x20, 0x20, 0x20, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x1c, 0x22, 0x22, 0x22, 0x2a, 0x24, 0x1a, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x3c, 0x22, 0x22, 0x3c, 0x28, 0x24, 0x22, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x1c, 0x22, 0x10, 0x08, 0x04, 0x22, 0x1c, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x3e, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x22, 0x22, 0x22, 0x22, 0x22, 0x22, 0x1c, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x22, 0x22, 0x22, 0x14, 0x14, 0x08, 0x08, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x22, 0x22, 0x22, 0x2a, 0x2a, 0x36, 0x22, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x22, 0x22, 0x14, 0x08, 0x14, 0x22, 0x22, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x22, 0x22, 0x14, 0x08, 0x08, 0x08, 0x08, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x3e, 0x02, 0x04, 0x08, 0x10, 0x20, 0x3e, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x38, 0x20, 0x20, 0x20, 0x20, 0x20, 0x38, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x20, 0x20, 0x10, 0x08, 0x04, 0x02, 0x02, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x0e, 0x02, 0x02, 0x02, 0x02, 0x02, 0x0e, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x08, 0x1c, 0x2a, 0x08, 0x08, 0x08, 0x08, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x08, 0x10, 0x3e, 0x10, 0x08, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x08, 0x08, 0x08, 0x08, 0x08, 0x00, 0x08, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x14, 0x14, 0x14, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x14, 0x14, 0x36, 0x00, 0x36, 0x14, 0x14, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x08, 0x1e, 0x20, 0x1c, 0x02, 0x3c, 0x08, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x32, 0x32, 0x04, 0x08, 0x10, 0x26, 0x26, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x10, 0x28, 0x28, 0x10, 0x2a, 0x24, 0x1a, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x18, 0x18, 0x18, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x08, 0x10, 0x20, 0x20, 0x20, 0x10, 0x08, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x08, 0x04, 0x02, 0x02, 0x02, 0x04, 0x08, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x08, 0x1c, 0x3e, 0x1c, 0x08, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x08, 0x08, 0x3e, 0x08, 0x08, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x30, 0x30, 0x10, 0x20, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3e, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x30, 0x30, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x02, 0x02, 0x04, 0x08, 0x10, 0x20, 0x20, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x18, 0x24, 0x24, 0x24, 0x24, 0x24, 0x18, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x08, 0x18, 0x08, 0x08, 0x08, 0x08, 0x1c, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x1c, 0x22, 0x02, 0x1c, 0x20, 0x20, 0x3e, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x1c, 0x22, 0x02, 0x0c, 0x02, 0x22, 0x1c, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x04, 0x0c, 0x14, 0x3e, 0x04, 0x04, 0x04, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x3e, 0x20, 0x3c, 0x02, 0x02, 0x22, 0x1c, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x1c, 0x20, 0x20, 0x3c, 0x22, 0x22, 0x1c, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x3e, 0x02, 0x04, 0x08, 0x10, 0x20, 0x20, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x1c, 0x22, 0x22, 0x1c, 0x22, 0x22, 0x1c, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x1c, 0x22, 0x22, 0x1e, 0x02, 0x02, 0x1c, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x18, 0x18, 0x00, 0x18, 0x18, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x18, 0x18, 0x00, 0x18, 0x18, 0x08, 0x10, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x04, 0x08, 0x10, 0x20, 0x10, 0x08, 0x04, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x3e, 0x00, 0x3e, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x10, 0x08, 0x04, 0x02, 0x04, 0x08, 0x10, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x18, 0x24, 0x04, 0x08, 0x08, 0x00, 0x08, 0x00, 0x00,
};

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
}


void fb_scroll() {
   printf("scroll\r\n");
   memmove(fb, fb + 12 * pitch, (height - 12) * pitch);
   memset(fb + (height - 12) * pitch, 0, 12 * pitch);
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

void fb_initialize() {

   int col, x, y;
    rpi_mailbox_property_t *mp;

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

    fb_writes("\r\n\r\nACORN ATOM\r\n>");
    
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
    

    // Uncomment this for testing from the serial port
    // while (1) {
    //    int c = RPI_AuxMiniUartRead();
    //    fb_writec(c);
    // }
}

#define NORMAL    0
#define IN_VDU17  1
#define IN_VDU18  2
#define IN_VDU25  3
#define IN_VDU29  4

void update_g_cursors(int16_t x, int16_t y) {
   g_x_pos_last2 = g_x_pos_last1;
   g_x_pos_last1 = g_x_pos;
   g_x_pos       = x;
   g_y_pos_last2 = g_y_pos_last1;
   g_y_pos_last1 = g_y_pos;
   g_y_pos       = y;
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

void fb_writec(int c) {
   int invert;
   
   static int state = NORMAL;
   static int count = 0;
   static int16_t x_tmp = 0;
   static int16_t y_tmp = 0;

   if (state == IN_VDU17) {
      state = NORMAL;
      if (c & 128) {
         c_bg_col = c & 15;
         printf("bg = %d\r\n", c_bg_col);
      } else {
         c_fg_col = c & 15;
         printf("fg = %d\r\n", c_fg_col);
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
         switch (g_mode & 7) {
         case 0:
            // Move relative to the last point.
            update_g_cursors(g_x_pos + x_tmp, g_y_pos + y_tmp);
            break;
         case 1:
            // Draw a line, in the current graphics foreground colour, relative to the last point.
            update_g_cursors(g_x_pos + x_tmp, g_y_pos + y_tmp);
            fb_draw_line(g_x_pos_last1, g_y_pos_last1, g_x_pos, g_y_pos, g_fg_col);
            break;
         case 2:
            // Draw a line, in the logical inverse colour, relative to the last point.
            update_g_cursors(g_x_pos + x_tmp, g_y_pos + y_tmp);
            fb_draw_line(g_x_pos_last1, g_y_pos_last1, g_x_pos, g_y_pos, g_fg_col ^ 15);
            break;
         case 3:
            // Draw a line, in the background colour, relative to the last point.
            update_g_cursors(g_x_pos + x_tmp, g_y_pos + y_tmp);
            fb_draw_line(g_x_pos_last1, g_y_pos_last1, g_x_pos, g_y_pos, g_bg_col);
            break;
         case 4:            
            // Move to the absolute position X, Y.
            update_g_cursors(x_tmp, y_tmp);
            break;
         case 5:
            // Draw a line, in the current foreground colour, to the absolute coordinates specified by X and Y.
            update_g_cursors(x_tmp, y_tmp);
            fb_draw_line(g_x_pos_last1, g_y_pos_last1, g_x_pos, g_y_pos, g_fg_col);
            break;
         case 6:
            // Draw a line, in the logical inverse colour, to the absolute coordinates specified by X and Y.
            update_g_cursors(x_tmp, y_tmp);
            fb_draw_line(g_x_pos_last1, g_y_pos_last1, g_x_pos, g_y_pos, g_fg_col ^ 15);
            break;
         case 7:
            // Draw a line, in the current background colour, to the absolute coordinates specified by X and Y.
            update_g_cursors(x_tmp, y_tmp);
            fb_draw_line(g_x_pos_last1, g_y_pos_last1, g_x_pos, g_y_pos, g_bg_col);
            break;
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
         printf("graphics origin %d %d\r\n", g_x_origin, g_y_origin);
      }
      count++;
      if (count == 4) {
         state = NORMAL;
      }
      return;
   }


   switch(c) {

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

   case 17:
      state = IN_VDU17;
      count = 0;
      return;

   case 18:
      state = IN_VDU18;
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

   case 127:
      fb_draw_character(32, 1, 1);
      fb_cursor_left();
      fb_draw_character(32, 1, 0);
      break;

   default:

      // Convert c to index into 6847 character generator ROM
      // chars 20-3F map to 20-3F
      // chars 40-5F map to 00-1F
      // chars 60-7F map to 00-1F inverted
      invert = 0;
      c &= 0x7f;
      if (c < 0x20) {
         return;
      } else if (c >= 0x40) {
         invert = c >= 0x60;
         c &= 0x1F;
      }

      // Erase the cursor
      fb_draw_character(32, 1, 1);

      // Draw the next character
      fb_draw_character(c, invert, 0);

      // Advance the drawing position
      fb_cursor_next();
      
      // Draw the cursor
      fb_draw_character(32, 1, 1);
   }
}

void fb_writes(char *string) {
   while (*string) {
      fb_writec(*string++);
   }
}

void fb_putpixel_16bpp(int16_t x, int16_t y, unsigned int colour) {
   x += g_x_origin;
   y += g_y_origin;
   uint16_t *fbptr = (uint16_t *)(fb + (height - y - 1) * pitch + x * 2);
   *fbptr = colour_table[colour];
}

void fb_putpixel_32bpp(int16_t x, int16_t y, unsigned int colour) {
   x += g_x_origin;
   y += g_y_origin;
   uint32_t *fbptr = (uint32_t *)(fb + (height - y - 1) * pitch + x * 4);
   *fbptr = colour_table[colour];
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
      PUTPIXEL(x, y, color);
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
