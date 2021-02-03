#ifndef _SCREEN_MODE_H
#define _SCREEN_MODE_H

#include <inttypes.h>

// Default screen mode
// 640x512 256 colours (80x64 text)
#define DEFAULT_SCREEN_MODE 21

// Custom screen modes (used by VDU 23,22)
#define CUSTOM_8BPP_SCREEN_MODE  96
#define CUSTOM_16BPP_SCREEN_MODE 97
#define CUSTOM_32BPP_SCREEN_MODE 98

#include "fonts.h"

// Uncomment to use V3D triangle fill in 16bpp and 32bpp modes
// This is approx 2x-3x faster for random sized triangles
// #define USE_V3D

typedef uint32_t pixel_t;

typedef unsigned int colour_index_t;

typedef struct screen_mode {
   int mode_num;    // Mode number, used by VDU 22,N

   int width;       // width in physical pixels
   int height;      // height in physical pixels

   int xeigfactor;  // conversion factor between OS units and pixels
   int yeigfactor;  // conversion factor between OS units and pixels

   int bpp;         // bits per pixel (8,16,32)

   int ncolour;     // maximum logical colour

   int pitch;       // filled in by init

   void           (*init)(struct screen_mode *screen);
   void          (*reset)(struct screen_mode *screen);
   void          (*clear)(struct screen_mode *screen, pixel_t colour);
   void         (*scroll)(struct screen_mode *screen, int pixel_rows, pixel_t colour);
   void          (*flash)(struct screen_mode *screen);
   void     (*set_colour)(struct screen_mode *screen, colour_index_t index, int r, int g, int b);
   pixel_t  (*get_colour)(struct screen_mode *screen, colour_index_t index);
   void      (*set_pixel)(struct screen_mode *screen, int x, int y, pixel_t value);
   pixel_t   (*get_pixel)(struct screen_mode *screen, int x, int y);
   void (*draw_character)(struct screen_mode *screen, font_t *font, int c, int col, int row, pixel_t fg_col, pixel_t bg_col);
   int  (*read_character)(struct screen_mode *screen, font_t *font,        int col, int row);

} screen_mode_t;


screen_mode_t *get_screen_mode(int mode_num);

// TODO: get rid of this
// It's only needed for fb_get_address which is only used by v3d
extern unsigned char* fb;

#endif
