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

typedef struct {
   int16_t left;
   int16_t bottom;
   int16_t right;
   int16_t top;
} g_clip_window_t;

typedef struct {
   uint8_t left;
   uint8_t bottom;
   uint8_t right;
   uint8_t top;
} t_clip_window_t;

typedef struct screen_mode {
   int mode_num;    // Mode number, used by VDU 22,N

   int width;       // width in physical pixels
   int height;      // height in physical pixels

   int xeigfactor;  // conversion factor between OS units and pixels
   int yeigfactor;  // conversion factor between OS units and pixels

   int bpp;         // bits per pixel (8,16,32)

   int ncolour;     // maximum logical colour

   int pitch;       // filled in by init

   font_t *font;    // the current font for this screen mode

   void            (*init)(struct screen_mode *screen);
   void           (*reset)(struct screen_mode *screen);
   void           (*clear)(struct screen_mode *screen, t_clip_window_t *text_window, pixel_t bg_col);
   void          (*scroll)(struct screen_mode *screen, t_clip_window_t *text_window, pixel_t bg_col);
   void           (*flash)(struct screen_mode *screen);
   void      (*set_colour)(struct screen_mode *screen, colour_index_t index, int r, int g, int b);
   pixel_t   (*get_colour)(struct screen_mode *screen, colour_index_t index);
   void  (*update_palette)(struct screen_mode *screen, colour_index_t offset, unsigned int num_colours);
   void       (*set_pixel)(struct screen_mode *screen, int x, int y, pixel_t value);
   pixel_t    (*get_pixel)(struct screen_mode *screen, int x, int y);
   void (*write_character)(struct screen_mode *screen, int c, int col, int row, pixel_t fg_col, pixel_t bg_col);
   int   (*read_character)(struct screen_mode *screen,        int col, int row);
   void     (*unknown_vdu)(struct screen_mode *screen, uint8_t *buf);

} screen_mode_t;


// ==========================================================================
// Default handlers
// ==========================================================================

// These are non static so it can be called by custom modes

void         default_init_screen(screen_mode_t *screen);
void        default_reset_screen(screen_mode_t *screen);
void        default_clear_screen(screen_mode_t *screen, t_clip_window_t *text_window, pixel_t bg_col);
void       default_scroll_screen(screen_mode_t *screen, t_clip_window_t *text_window, pixel_t bg_col);
void     default_set_colour_8bpp(screen_mode_t *screen, colour_index_t index, int r, int g, int b);
void    default_set_colour_16bpp(screen_mode_t *screen, colour_index_t index, int r, int g, int b);
void    default_set_colour_32bpp(screen_mode_t *screen, colour_index_t index, int r, int g, int b);
pixel_t  default_get_colour_8bpp(screen_mode_t *screen, colour_index_t index);
pixel_t default_get_colour_16bpp(screen_mode_t *screen, colour_index_t index);
pixel_t default_get_colour_32bpp(screen_mode_t *screen, colour_index_t index);
void      default_set_pixel_8bpp(screen_mode_t *screen, int x, int y, pixel_t value);
void     default_set_pixel_16bpp(screen_mode_t *screen, int x, int y, pixel_t value);
void     default_set_pixel_32bpp(screen_mode_t *screen, int x, int y, pixel_t value);
pixel_t   default_get_pixel_8bpp(screen_mode_t *screen, int x, int y);
pixel_t  default_get_pixel_16bpp(screen_mode_t *screen, int x, int y);
pixel_t  default_get_pixel_32bpp(screen_mode_t *screen, int x, int y);
void     default_write_character(screen_mode_t *screen, int c, int col, int row, pixel_t fg_col, pixel_t bg_col);
int       default_read_character(screen_mode_t *screen, int col, int row);
void         default_unknown_vdu(screen_mode_t *screen, uint8_t *buf);

// ==========================================================================
// Public methods
// ==========================================================================

screen_mode_t *get_screen_mode(int mode_num);

uint32_t get_fb_address();

#endif
