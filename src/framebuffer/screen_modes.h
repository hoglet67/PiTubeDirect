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

// Forward reference to the font structure defined in font.h
struct font;

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

typedef enum {
   F_NON_GRAPHICS            = (1 << 0),
   F_TELETEXT                = (1 << 1),
   F_GAP                     = (1 << 2),
   F_BBC_GAP                 = (1 << 3),
   F_HIRES_MONO              = (1 << 4),
   F_DOUBLE_HEIGHT_VDU_CHARS = (1 << 5),
   F_HARDWARE_SCROLL_DISABLE = (1 << 6),
   F_FULL_PALETTE            = (1 << 7),
   F_FULL_RES_INTERLACED     = (1 << 8),
   F_GREYSCALE_PALETTE       = (1 << 9)
} mode_flag_t;

typedef enum {
   M_MODEFLAGS       = 0,   // &00 Assorted flags
   M_SCRRCOL         = 1,   // &01 Number of text columns -1
   M_SCRBROW         = 2,   // &02 Number of text rows -1
   M_NCOLOUR         = 3,   // &03 Maximum logical colour
   M_XEIGFACTOR      = 4,   // &04 Conversion factor between OS units and pixels
   M_YEIGFACTOR      = 5,   // &05 Conversion factor between OS units and pixels
   M_LINELENGTH      = 6,   // &06 Number of bytes per pixel row
   M_SCREENSIZE      = 7,   // &07 Number of bytes for entire screen display
   M_YSHIFTSIZE      = 8,   // &08 Deprecated. Do not use
   M_LOG2BPP         = 9,   // &09 Log base 2 of bits per pixel
   M_LOG2BPC         = 10,  // &0A Log base 2 of bytes per character
   M_XWINDLIMIT      = 11,  // &0B Number of x pixels on screen -1
   M_YWINDLIMIT      = 12,  // &0C Number of y pixels on screen -1
   NUM_MODE_VARS     = 13
} mode_variable_t;

typedef struct screen_mode {
   int mode_num;    // Mode number, used by VDU 22,N

   int mode_flags;  // mode flags describing mode

   int width;       // width in physical pixels
   int height;      // height in physical pixels

   int xeigfactor;  // conversion factor between OS units and pixels
   int yeigfactor;  // conversion factor between OS units and pixels

   int log2bpp;     // log2 of the number bits per pixel (8->3,16->4,32->5)
   int log2bpc;     // log2 of the number bytes per text character (normally same as log2pbb, except in double pixel modes)

   int ncolour;     // maximum logical colour

   float par;       // ideal pixel aspect ratio

   int pitch;       // filled in by init

   uint8_t white;   // the gcol number for white

   struct font *font; // the current font for this screen mode

   void                     (*init)(struct screen_mode *screen);
   void                    (*reset)(struct screen_mode *screen);
   void                    (*clear)(struct screen_mode *screen, t_clip_window_t *text_window, pixel_t bg_col);
   void                   (*scroll)(struct screen_mode *screen, t_clip_window_t *text_window, pixel_t bg_col);
   void                    (*flash)(struct screen_mode *screen, int mark);
   void               (*set_colour)(struct screen_mode *screen, colour_index_t index, int r, int g, int b);
   pixel_t            (*get_colour)(struct screen_mode *screen, uint8_t gcol);
   pixel_t        (*nearest_colour)(struct screen_mode *screen, int r, int g, int b);
   void           (*update_palette)(struct screen_mode *screen, int mark);
   void                (*set_pixel)(struct screen_mode *screen, int x, int y, pixel_t value);
   pixel_t             (*get_pixel)(struct screen_mode *screen, int x, int y);
   void          (*write_character)(struct screen_mode *screen, int c, int col, int row, pixel_t fg_col, pixel_t bg_col);
   int            (*read_character)(struct screen_mode *screen,        int col, int row,                 pixel_t bg_col);
   void              (*unknown_vdu)(struct screen_mode *screen, uint8_t *buf);
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
pixel_t  default_get_colour_8bpp(screen_mode_t *screen, uint8_t gcol);
pixel_t default_get_colour_16bpp(screen_mode_t *screen, uint8_t gcol);
pixel_t default_get_colour_32bpp(screen_mode_t *screen, uint8_t gcol);
void      default_set_pixel_8bpp(screen_mode_t *screen, int x, int y, pixel_t value);
void     default_set_pixel_16bpp(screen_mode_t *screen, int x, int y, pixel_t value);
void     default_set_pixel_32bpp(screen_mode_t *screen, int x, int y, pixel_t value);
pixel_t   default_get_pixel_8bpp(screen_mode_t *screen, int x, int y);
pixel_t  default_get_pixel_16bpp(screen_mode_t *screen, int x, int y);
pixel_t  default_get_pixel_32bpp(screen_mode_t *screen, int x, int y);
void     default_write_character(screen_mode_t *screen, int c, int col, int row, pixel_t fg_col, pixel_t bg_col);
int       default_read_character(screen_mode_t *screen, int col, int row,                        pixel_t bg_col);
void         default_unknown_vdu(screen_mode_t *screen, uint8_t *buf);

// ==========================================================================
// Public methods
// ==========================================================================

screen_mode_t *get_screen_mode(int mode_num);

uint32_t get_fb_address();

int32_t fb_read_mode_variable(mode_variable_t v, screen_mode_t *screen);

#endif
