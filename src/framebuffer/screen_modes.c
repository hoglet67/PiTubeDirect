#include <stdio.h>
#include <string.h>

#include "../startup.h"
#include "../rpi-mailbox-interface.h"
#include "../rpi-base.h"

#include "screen_modes.h"
#include "teletext.h"

#ifdef USE_V3D
#include "v3d.h"
#endif

// Registers to read the physical screen size
#define PIXELVALVE2_HORZB (volatile uint32_t *)(PERIPHERAL_BASE + 0x807010)
#define PIXELVALVE2_VERTB (volatile uint32_t *)(PERIPHERAL_BASE + 0x807018)

unsigned char* fb = NULL;

// Palette for 8bpp modes
#define NUM_COLOURS 256
static pixel_t colour_table[NUM_COLOURS];
static int sync_palette;

typedef struct {
   int x1;
   int y1;
   int x2;
   int y2;
} rectangle_t;

// ==========================================================================
// Screen Mode Definitions
// ==========================================================================

static screen_mode_t screen_modes[] = {
   {
      .mode_num      = 0,
      .width         = 640,
      .height        = 256,
      .xeigfactor    = 1,
      .yeigfactor    = 2,
      .bpp           = 8,
      .ncolour       = 1
   },
   {
      .mode_num      = 1,
      .width         = 320,
      .height        = 256,
      .xeigfactor    = 2,
      .yeigfactor    = 2,
      .bpp           = 8,
      .ncolour       = 3
   },
   {
      .mode_num      = 2,
      .width         = 160,
      .height        = 256,
      .xeigfactor    = 3,
      .yeigfactor    = 2,
      .bpp           = 8,
      .ncolour       = 15
   },
   {
      .mode_num      = 3,
      .width         = 640,
      .height        = 256,
      .xeigfactor    = 1,
      .yeigfactor    = 2,
      .bpp           = 8,
      .ncolour       = 1
   },
   {
      .mode_num      = 4,
      .width         = 320,
      .height        = 256,
      .xeigfactor    = 2,
      .yeigfactor    = 2,
      .bpp           = 8,
      .ncolour       = 1
   },
   {
      .mode_num      = 5,
      .width         = 160,
      .height        = 256,
      .xeigfactor    = 3,
      .yeigfactor    = 2,
      .bpp           = 8,
      .ncolour       = 3
   },
   {
      .mode_num      = 6,
      .width         = 320,
      .height        = 256,
      .xeigfactor    = 2,
      .yeigfactor    = 2,
      .bpp           = 8,
      .ncolour       = 1
   },
   {
      .mode_num      = 7,
      .width         = 320,
      .height        = 256,
      .xeigfactor    = 2,
      .yeigfactor    = 2,
      .bpp           = 8,
      .ncolour       = 15
   },
   {
      .mode_num      = 8,
      .width         = 640,
      .height        = 256,
      .xeigfactor    = 1,
      .yeigfactor    = 2,
      .bpp           = 8,
      .ncolour       = 3
   },
   {
      .mode_num      = 9,
      .width         = 320,
      .height        = 256,
      .xeigfactor    = 2,
      .yeigfactor    = 2,
      .bpp           = 8,
      .ncolour       = 15
   },
   {
      .mode_num      = 10,
      .width         = 160,
      .height        = 256,
      .xeigfactor    = 3,
      .yeigfactor    = 2,
      .bpp           = 8,
      .ncolour       = 255
   },
   {
      .mode_num      = 11,
      .width         = 640,
      .height        = 250,
      .xeigfactor    = 1,
      .yeigfactor    = 2,
      .bpp           = 8,
      .ncolour       = 3
   },
   {
      .mode_num      = 12,
      .width         = 640,
      .height        = 256,
      .xeigfactor    = 1,
      .yeigfactor    = 2,
      .bpp           = 8,
      .ncolour       = 15
   },
   {
      .mode_num      = 13,
      .width         = 320,
      .height        = 256,
      .xeigfactor    = 2,
      .yeigfactor    = 2,
      .bpp           = 8,
      .ncolour       = 255
   },
   {
      .mode_num      = 14,
      .width         = 640,
      .height        = 250,
      .xeigfactor    = 1,
      .yeigfactor    = 2,
      .bpp           = 8,
      .ncolour       = 15
   },
   {
      .mode_num      = 15,
      .width         = 640,
      .height        = 256,
      .xeigfactor    = 1,
      .yeigfactor    = 2,
      .bpp           = 8,
      .ncolour       = 255
   },
   {
      .mode_num      = 16,
      .width         = 1056,
      .height        = 256,
      .xeigfactor    = 1,
      .yeigfactor    = 2,
      .bpp           = 8,
      .ncolour       = 15
   },
   {
      .mode_num      = 17,
      .width         = 1056,
      .height        = 250,
      .xeigfactor    = 1,
      .yeigfactor    = 2,
      .bpp           = 8,
      .ncolour       = 15
   },
   {
      .mode_num      = 18,
      .width         = 640,
      .height        = 512,
      .xeigfactor    = 1,
      .yeigfactor    = 1,
      .bpp           = 8,
      .ncolour       = 1
   },
   {
      .mode_num      = 19,
      .width         = 640,
      .height        = 512,
      .xeigfactor    = 1,
      .yeigfactor    = 1,
      .bpp           = 8,
      .ncolour       = 3
   },
   {
      .mode_num      = 20,
      .width         = 640,
      .height        = 512,
      .xeigfactor    = 1,
      .yeigfactor    = 1,
      .bpp           = 8,
      .ncolour       = 15
   },
   {
      .mode_num      = 21,
      .width         = 640,
      .height        = 512,
      .xeigfactor    = 1,
      .yeigfactor    = 1,
      .bpp           = 8,
      .ncolour       = 255
   },
   {
      .mode_num      = 22,
      .width         = 768,
      .height        = 288,
      .xeigfactor    = 0,
      .yeigfactor    = 1,
      .bpp           = 8,
      .ncolour       = 15
   },
   {
      .mode_num      = 23,
      .width         = 1152,
      .height        = 892,
      .xeigfactor    = 1,
      .yeigfactor    = 1,
      .bpp           = 8,
      .ncolour       = 1
   },
   {
      .mode_num      = 24,
      .width         = 1056,
      .height        = 256,
      .xeigfactor    = 1,
      .yeigfactor    = 2,
      .bpp           = 8,
      .ncolour       = 255
   },
   {
      .mode_num      = 25,
      .width         = 640,
      .height        = 480,
      .xeigfactor    = 1,
      .yeigfactor    = 1,
      .bpp           = 8,
      .ncolour       = 1
   },
   {
      .mode_num      = 26,
      .width         = 640,
      .height        = 480,
      .xeigfactor    = 1,
      .yeigfactor    = 1,
      .bpp           = 8,
      .ncolour       = 3
   },
   {
      .mode_num      = 27,
      .width         = 640,
      .height        = 480,
      .xeigfactor    = 1,
      .yeigfactor    = 1,
      .bpp           = 8,
      .ncolour       = 15
   },
   {
      .mode_num      = 28,
      .width         = 640,
      .height        = 480,
      .xeigfactor    = 1,
      .yeigfactor    = 1,
      .bpp           = 8,
      .ncolour       = 255
   },
   {
      .mode_num      = 29,
      .width         = 800,
      .height        = 600,
      .xeigfactor    = 1,
      .yeigfactor    = 1,
      .bpp           = 8,
      .ncolour       = 1
   },
   {
      .mode_num      = 30,
      .width         = 800,
      .height        = 600,
      .xeigfactor    = 1,
      .yeigfactor    = 1,
      .bpp           = 8,
      .ncolour       = 3
   },
   {
      .mode_num      = 31,
      .width         = 800,
      .height        = 600,
      .xeigfactor    = 1,
      .yeigfactor    = 1,
      .bpp           = 8,
      .ncolour       = 15
   },
   // Mode 32 was never defined
   {
      .mode_num      = 33,
      .width         = 768,
      .height        = 288,
      .xeigfactor    = 1,
      .yeigfactor    = 2,
      .bpp           = 8,
      .ncolour       = 1
   },
   {
      .mode_num      = 34,
      .width         = 768,
      .height        = 288,
      .xeigfactor    = 1,
      .yeigfactor    = 2,
      .bpp           = 8,
      .ncolour       = 3
   },
   {
      .mode_num      = 35,
      .width         = 768,
      .height        = 288,
      .xeigfactor    = 1,
      .yeigfactor    = 2,
      .bpp           = 8,
      .ncolour       = 15
   },
   {
      .mode_num      = 36,
      .width         = 768,
      .height        = 288,
      .xeigfactor    = 1,
      .yeigfactor    = 2,
      .bpp           = 8,
      .ncolour       = 255
   },
   {
      .mode_num      = 37,
      .width         = 896,
      .height        = 352,
      .xeigfactor    = 1,
      .yeigfactor    = 2,
      .bpp           = 8,
      .ncolour       = 1
   },
   {
      .mode_num      = 38,
      .width         = 896,
      .height        = 352,
      .xeigfactor    = 1,
      .yeigfactor    = 2,
      .bpp           = 8,
      .ncolour       = 3
   },
   {
      .mode_num      = 39,
      .width         = 896,
      .height        = 352,
      .xeigfactor    = 1,
      .yeigfactor    = 2,
      .bpp           = 8,
      .ncolour       = 15
   },
   {
      .mode_num      = 40,
      .width         = 896,
      .height        = 352,
      .xeigfactor    = 1,
      .yeigfactor    = 2,
      .bpp           = 8,
      .ncolour       = 255
   },
   {
      .mode_num      = 41,
      .width         = 640,
      .height        = 352,
      .xeigfactor    = 1,
      .yeigfactor    = 2,
      .bpp           = 8,
      .ncolour       = 1
   },
   {
      .mode_num      = 42,
      .width         = 640,
      .height        = 352,
      .xeigfactor    = 1,
      .yeigfactor    = 2,
      .bpp           = 8,
      .ncolour       = 3
   },
   {
      .mode_num      = 43,
      .width         = 640,
      .height        = 352,
      .xeigfactor    = 1,
      .yeigfactor    = 2,
      .bpp           = 8,
      .ncolour       = 15
   },
   {
      .mode_num      = 44,
      .width         = 640,
      .height        = 200,
      .xeigfactor    = 1,
      .yeigfactor    = 2,
      .bpp           = 8,
      .ncolour       = 1
   },
   {
      .mode_num      = 45,
      .width         = 640,
      .height        = 200,
      .xeigfactor    = 1,
      .yeigfactor    = 2,
      .bpp           = 8,
      .ncolour       = 3
   },
   {
      .mode_num      = 46,
      .width         = 640,
      .height        = 200,
      .xeigfactor    = 1,
      .yeigfactor    = 2,
      .bpp           = 8,
      .ncolour       = 15
   },
   {
      .mode_num      = 47,
      .width         = 360,
      .height        = 480,
      .xeigfactor    = 2,
      .yeigfactor    = 1,
      .bpp           = 8,
      .ncolour       = 255
   },
   {
      .mode_num      = 48,
      .width         = 320,
      .height        = 480,
      .xeigfactor    = 2,
      .yeigfactor    = 1,
      .bpp           = 8,
      .ncolour       = 15
   },
   {
      .mode_num      = 49,
      .width         = 320,
      .height        = 480,
      .xeigfactor    = 2,
      .yeigfactor    = 1,
      .bpp           = 8,
      .ncolour       = 255
   },
   {
      .mode_num      = 50,
      .width         = 320,
      .height        = 240,
      .xeigfactor    = 2,
      .yeigfactor    = 2,
      .bpp           = 8,
      .ncolour       = 1
   },
   {
      .mode_num      = 51,
      .width         = 320,
      .height        = 240,
      .xeigfactor    = 2,
      .yeigfactor    = 2,
      .bpp           = 8,
      .ncolour       = 3
   },
   {
      .mode_num      = 52,
      .width         = 320,
      .height        = 240,
      .xeigfactor    = 2,
      .yeigfactor    = 2,
      .bpp           = 8,
      .ncolour       = 15
   },
   {
      .mode_num      = 53,
      .width         = 320,
      .height        = 240,
      .xeigfactor    = 2,
      .yeigfactor    = 2,
      .bpp           = 8,
      .ncolour       = 255
   },
   // 8 bpp
   {
      .mode_num      = 64,
      .width         = 640,
      .height        = 512,
      .xeigfactor    = 1,
      .yeigfactor    = 1,
      .bpp           = 8,
      .ncolour       = 0xff
   },
   {
      .mode_num      = 65,
      .width         = 1280,
      .height        = 1024,
      .xeigfactor    = 0,
      .yeigfactor    = 0,
      .bpp           = 8,
      .ncolour       = 0xff
   },
   // 16 bpp
   {
      .mode_num      = 66,
      .width         = 640,
      .height        = 512,
      .xeigfactor    = 1,
      .yeigfactor    = 1,
      .bpp           = 16,
      .ncolour       = 0xffff
   },
   {
      .mode_num      = 67,
      .width         = 1280,
      .height        = 1024,
      .xeigfactor    = 0,
      .yeigfactor    = 0,
      .bpp           = 16,
      .ncolour       = 0xffff
   },
   // 32 bpp
   {
      .mode_num      = 68,
      .width         = 640,
      .height        = 512,
      .xeigfactor    = 1,
      .yeigfactor    = 1,
      .bpp           = 32,
      .ncolour       = 0xffffff
   },
   {
      .mode_num      = 69,
      .width         = 1280,
      .height        = 1024,
      .xeigfactor    = 0,
      .yeigfactor    = 0,
      .bpp           = 32,
      .ncolour       = 0xffffff
   },
   {
      .mode_num     = CUSTOM_8BPP_SCREEN_MODE,
      .bpp          = 8
   },
   {
      .mode_num     = CUSTOM_16BPP_SCREEN_MODE,
      .bpp          = 16
   },
   {
      .mode_num     = CUSTOM_32BPP_SCREEN_MODE,
      .bpp          = 32
   },
   {
      .mode_num     = -1,
   }
};

// ==========================================================================
// Static methods
// ==========================================================================

static void update_palette(int offset, int num_colours) {
   RPI_PropertyInit();
   RPI_PropertyAddTag(TAG_SET_PALETTE, offset, num_colours, colour_table);
#ifdef USE_DOORBELL
   // Call the Check version as doorbell and mailboxes are seperate
   //LOG_INFO("Calling TAG_SET_PALETTE\r\n");
   RPI_PropertyProcess();
   //rpi_mailbox_property_t *buf = RPI_PropertyGet(TAG_SET_PALETTE);
   //if (buf) {
   //   LOG_INFO("TAG_SET_PALETTE returned %08x\r\n", buf->data.buffer_32[0]);
   //} else {
   //   LOG_INFO("TAG_SET_PALETTE returned ?\r\n");
   //}
#else
   // Call the NoCheck version as our mailbox FIQ handler swallows the response
   RPI_PropertyProcessNoCheck();
#endif
}

static void init_colour_table(screen_mode_t *screen) {
   if (screen->ncolour == 1) {
      // Default 2-Colour Palette
      // Colour  0 = Black
      // Colour  1 = White
      for (int i = 0; i < 256; i++) {
         switch (i & 1) {
         case 0:
            screen->set_colour(screen, i, 0x00, 0x00, 0x00);
            break;
         case 1:
            screen->set_colour(screen, i, 0xff, 0xff, 0xff);
            break;
         }
      }
   } else if (screen->ncolour == 3) {
      // Default 4-Colour Palette
      // Colour  0 = Black
      // Colour  1 = Red
      // Colour  2 = Yellow
      // Colour  3 = White
      for (int i = 0; i < 256; i++) {
         switch (i & 3) {
         case 0:
            screen->set_colour(screen, i, 0x00, 0x00, 0x00);
            break;
         case 1:
            screen->set_colour(screen, i, 0xff, 0x00, 0x00);
            break;
         case 2:
            screen->set_colour(screen, i, 0xff, 0xff, 0x00);
            break;
         case 3:
            screen->set_colour(screen, i, 0xff, 0xff, 0xff);
            break;
         }
      }
   } else if (screen->ncolour == 15) {
      // Default 16-Colour Palette
      // Colour  0 = Black
      // Colour  1 = Dark Red
      // Colour  2 = Dark Green
      // Colour  3 = Dark Yellow
      // Colour  4 = Dark Blue
      // Colour  5 = Dark Magenta
      // Colour  6 = Dark Cyan
      // Colour  7 = Dark White
      // Colour  8 = Dark Black
      // Colour  9 = Red
      // Colour 10 = Green
      // Colour 11 = Yellow
      // Colour 12 = Blue
      // Colour 13 = Magenta
      // Colour 14 = Cyan
      // Colour 15 = White
      for (int i = 0; i < 256; i++) {
         int intensity = (i & 8) ? 127 : 255;
         int b = (i & 4) ? intensity : 0;
         int g = (i & 2) ? intensity : 0;
         int r = (i & 1) ? intensity : 0;
         if ((i & 0x0F) == 0x08) {
            r = g = b = 63;
         }
         screen->set_colour(screen, i, r, g, b);
      }
   } else {
      // Default 256-colour mode palette
      // Bits 1..0 = R    (0x00, 0x44, 0x88, 0xCC)
      // Bits 3..2 = G    (0x00, 0x44, 0x88, 0xCC)
      // Bits 5..4 = B    (0x00, 0x44, 0x88, 0xCC)
      // Bits 7..6 = Tint (0x00, 0x11, 0x22, 0x33) added
      for (int i = 0; i < 256; i++) {
         // Tint
         int tint = ((i >> 6) & 0x03) * 0x11;
         // Colour
         int r = ((i     ) & 0x03) * 0x44 + tint;
         int g = ((i >> 2) & 0x03) * 0x44 + tint;
         int b = ((i >> 4) & 0x03) * 0x44 + tint;
         screen->set_colour(screen, i, r, g, b);
      }
   }
}


static int get_hdisplay() {
    return (*PIXELVALVE2_HORZB) & 0xFFFF;
}

static int get_vdisplay() {
    return (*PIXELVALVE2_VERTB) & 0xFFFF;
}

static void to_rectangle(screen_mode_t *screen, t_clip_window_t *text_window, rectangle_t *r) {
   if (text_window == NULL) {
      r->x1 = 0;
      r->y1 = 0;
      r->x2 = screen->width - 1;
      r->y2 = screen->height - 1;;
   } else {
      font_t *font = screen->font;
      int font_width  = font->width  * font->scale_w + font->spacing;
      int font_height = font->height * font->scale_h + font->spacing;
      r->x1 = text_window->left * font_width;
      r->y1 = screen->height - 1 - (text_window->bottom * font_height + font_height - 1);
      r->x2 = (text_window->right + 1) * font_width - 1;
      r->y2 = screen->height - 1 - text_window->top * font_height;
   }
}

static int is_full_screen(screen_mode_t *screen, rectangle_t *r) {
   return (r->x1 == 0 && r->y1 == 0 && r->x2 == screen->width - 1 && r->y2 == screen->height - 1);
}

// ==========================================================================
// Default handlers
// ==========================================================================

// These are non static so it can be called by custom modes

void default_init_screen(screen_mode_t *screen) {

    rpi_mailbox_property_t *mp;

    // Calculate optimal overscan
    int h_display = get_hdisplay();
    int v_display = get_vdisplay();

    // TODO: this can be greatly improved!
    // It assumes you want to fill (or nearly fill) a 1280x1024 window on your physical display
    // It will work really badly with an 800x600 screen mode, say on a 1600x1200 monitor
    int h_scale = 2 * 1280 / screen->width;
    int v_scale = 2 * 1024 / screen->height;
    int h_overscan = (h_display - h_scale * screen->width  / 2) / 2;
    int v_overscan = (v_display - v_scale * screen->height / 2) / 2;

    printf(" display: %d x %d\r\n", h_display, v_display);
    printf("   scale: %d x %d\r\n", h_scale, v_scale);
    printf("overscan: %d x %d\r\n", h_overscan, v_overscan);

    /* Initialise a framebuffer... */
    RPI_PropertyInit();
    RPI_PropertyAddTag(TAG_ALLOCATE_BUFFER);
    RPI_PropertyAddTag(TAG_SET_PHYSICAL_SIZE, screen->width, screen->height );
    RPI_PropertyAddTag(TAG_SET_VIRTUAL_SIZE,  screen->width, screen->height * 2 ); // TODO: FIX ME (remove the * 2)
    RPI_PropertyAddTag(TAG_SET_DEPTH,         screen->bpp );
    RPI_PropertyAddTag(TAG_GET_PITCH );
    RPI_PropertyAddTag(TAG_GET_PHYSICAL_SIZE );
    RPI_PropertyAddTag(TAG_GET_DEPTH );
    RPI_PropertyAddTag(TAG_SET_OVERSCAN, v_overscan, v_overscan, h_overscan, h_overscan);
    RPI_PropertyProcess();

    if( ( mp = RPI_PropertyGet( TAG_GET_PHYSICAL_SIZE ) ) )
    {
        int width = mp->data.buffer_32[0];
        int height = mp->data.buffer_32[1];

        printf( "Initialised Framebuffer: %dx%d ", width, height );
    }

    if( ( mp = RPI_PropertyGet( TAG_GET_DEPTH ) ) )
    {
        int bpp = mp->data.buffer_32[0];
        printf( "%dbpp\r\n", bpp );
    }

    if( ( mp = RPI_PropertyGet( TAG_GET_PITCH ) ) )
    {
        screen->pitch = mp->data.buffer_32[0];
        printf( "Pitch: %d bytes\r\n", screen->pitch );
    }

    if( ( mp = RPI_PropertyGet( TAG_ALLOCATE_BUFFER ) ) )
    {
        fb = (unsigned char*)mp->data.buffer_32[0];
        printf( "Framebuffer address: %8.8X\r\n", (unsigned int)fb );
    }

    // On the Pi 2/3 the mailbox returns the address with bits 31..30 set, which is wrong
    fb = (unsigned char *)(((unsigned int) fb) & 0x3fffffff);

    // Initialize colour table and palette
    screen->reset(screen);

    /* Clear the screen to the background colour */
    screen->clear(screen, NULL, 0);

#ifdef USE_V3D
    if (screen->bpp > 8) {
       v3d_initialize(NULL);
    }
#endif
};

void default_reset_screen(screen_mode_t *screen) {
    /* Copy default colour table */
    sync_palette = 0;
    init_colour_table(screen);
    sync_palette = 1;

    /* Update the palette (only in 8-bpp modes) */
    if (screen->bpp == 8) {
       update_palette(0, NUM_COLOURS);
    }
}

void default_clear_screen(screen_mode_t *screen, t_clip_window_t *text_window, pixel_t bg_col) {
   rectangle_t r;
   // Convert text window to screen graphics coordinates (0,0 = bottom left)
   to_rectangle(screen, text_window, &r);
   // Clear to the background colour
   for (int y = r.y1; y <= r.y2; y++) {
      for (int x = r.x1; x <= r.x2; x++) {
         screen->set_pixel(screen, x, y, bg_col);
      }
   }
}

void default_scroll_screen(screen_mode_t *screen, t_clip_window_t *text_window, pixel_t bg_col) {
   rectangle_t r;
   font_t *font = screen->font;
   int font_height = font->height * font->scale_h + font->spacing;
   // Convert text window to screen graphics coordinates (0,0 = bottom left)
   to_rectangle(screen, text_window, &r);
   // Scroll the screen upwards one row, and clear the bottom text line to the background colour
   if (is_full_screen(screen, &r)) {
      _fast_scroll(fb, fb + font_height * screen->pitch, (screen->height - font_height) * screen->pitch);
      _fast_clear(fb + (screen->height - font_height) * screen->pitch, bg_col, font_height * screen->pitch);
   } else {
      // Scroll from top to bottom
      int y = r.y2;
      for ( ; y >= r.y1 + font_height; y--) {
         int z = y - font_height;
         for (int x = r.x1; x <= r.x2; x++) {
            screen->set_pixel(screen, x, y, screen->get_pixel(screen, x, z));
         }
      }
      // Blank the bottom line
      for ( ; y >= r.y1; y--) {
         for (int x = r.x1; x <= r.x2; x++) {
            screen->set_pixel(screen, x, y, bg_col);
         }
      }
   }
}

void default_set_colour_8bpp(screen_mode_t *screen, colour_index_t index, int r, int g, int b) {
   colour_table[index] = 0xFF000000 | ((b & 0xFF) << 16) | ((g & 0xFF) << 8) | (r & 0xFF);
   if (sync_palette) {
      update_palette(index, 1);
   }
}

void default_set_colour_16bpp(screen_mode_t *screen, colour_index_t index, int r, int g, int b) {
   // 15 14 13 12 11 10  9  8  7  6  5  4  3  2  1  0
   // R4 R3 R2 R1 R0 G5 G4 G3 G2 G1 G0 B4 B3 B2 B1 B0
   colour_table[index] = ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | ((b & 0xF8) >> 3);
}

void default_set_colour_32bpp(screen_mode_t *screen, colour_index_t index, int r, int g, int b) {
   colour_table[index] = 0xFF000000 | ((r & 0xFF) << 16) | ((g & 0xFF) << 8) | (b & 0xFF);
}

pixel_t default_get_colour_8bpp(screen_mode_t *screen, colour_index_t index) {
   return index;
}

pixel_t default_get_colour_16bpp(screen_mode_t *screen, colour_index_t index) {
   return colour_table[index];
}

pixel_t default_get_colour_32bpp(screen_mode_t *screen, colour_index_t index) {
   return colour_table[index];
}

void default_set_pixel_8bpp(screen_mode_t *screen, int x, int y, pixel_t value) {
   uint8_t *fbptr = (uint8_t *)(fb + (screen->height - y - 1) * screen->pitch + x);
   *fbptr = value;
}

void default_set_pixel_16bpp(screen_mode_t *screen, int x, int y, pixel_t value) {
   uint16_t *fbptr = (uint16_t *)(fb + (screen->height - y - 1) * screen->pitch + x * 2);
   *fbptr = value;

}
void default_set_pixel_32bpp(screen_mode_t *screen, int x, int y, pixel_t value) {
   uint32_t *fbptr = (uint32_t *)(fb + (screen->height - y - 1) * screen->pitch + x * 4);
   *fbptr = value;
}

pixel_t default_get_pixel_8bpp(screen_mode_t *screen, int x, int y) {
   uint8_t *fbptr = (uint8_t *)(fb + (screen->height - y - 1) * screen->pitch + x);
   return *fbptr;
}

pixel_t default_get_pixel_16bpp(screen_mode_t *screen, int x, int y) {
   uint16_t *fbptr = (uint16_t *)(fb + (screen->height - y - 1) * screen->pitch + x * 2);
   return *fbptr;
}

pixel_t default_get_pixel_32bpp(screen_mode_t *screen, int x, int y) {
   uint32_t *fbptr = (uint32_t *)(fb + (screen->height - y - 1) * screen->pitch + x * 4);
   return *fbptr;
}

void default_write_character(screen_mode_t *screen, int c, int col, int row, pixel_t fg_col, pixel_t bg_col) {
   font_t *font = screen->font;
   int p = c * font->bytes_per_char;
   // Convert Row/Col to screen coordinates
   int x_pos = col * (font->width * font->scale_w + font->spacing);
   int y_pos = screen->height - row * (font->height * font->scale_h + font->spacing) - 1;
   int x = x_pos;
   int y = y_pos;
   for (int i = 0; i < font->height; i++) {
      int data = font->buffer[p++];
      for (int j = 0; j < font->width; j++) {
         int col = (data & 0x80) ? fg_col : bg_col;
         for (int sx = 0; sx < font->scale_w; sx++) {
            for (int sy = 0; sy < font->scale_h; sy++) {
               screen->set_pixel(screen, x + sx, y + sy, col);
            }
         }
         x += font->scale_w;
         data <<= 1;
      }
      x = x_pos;
      y -= font->scale_h;
   }
}

int default_read_character(screen_mode_t *screen, int col, int row) {
   font_t *font = screen->font;
   int screendata[MAX_FONT_HEIGHT];
   // Read the character from screen memory
   // Convert Row/Col to screen coordinates
   int x = col * (font->width * font->scale_w + font->spacing);
   int y = screen->height - row * (font->height * font->scale_h + font->spacing) - 1;
   int *dp = screendata;
   for (int i = 0; i < font->height * font->scale_h; i += font->scale_h) {
      int row = 0;
      for (int j = 0; j < font->width * font->scale_w; j += font->scale_w) {
         row <<= 1;
         if (screen->get_pixel(screen, x + j, y - i)) {
            row |= 1;
         }
      }
      *dp++ = row;
   }
   // Match against font
   for (int c = 0x20; c < font->num_chars; c++) {
       for (y = 0; y < font->height; y++) {
         int xor = font->buffer[c * font->bytes_per_char + y] ^ screendata[y];
         if (xor != 0x00 && xor != 0xff) {
            break;
         }
      }
      if (y == font->height) {
         return c;
      }
   }
   return 0;
}

// ==========================================================================
// Public methods
// ==========================================================================

screen_mode_t *get_screen_mode(int mode_num) {
   screen_mode_t *sm = NULL;
   if (mode_num == 7) {
      // Special case the teletext mode
      sm = tt_get_screen_mode();
   } else {
      // Otherwise just search the screen mode table
      screen_mode_t *tmp = screen_modes;
      while (tmp->mode_num >= 0) {
         if (tmp->mode_num == mode_num) {
            sm = tmp;
            break;
         }
         tmp++;
      }
   }
   // Fill in any default functions
   if (sm) {
      if (!sm->init) {
         sm->init = default_init_screen;
      }
      if (!sm->reset) {
         sm->reset = default_reset_screen;
      }
      if (!sm->clear) {
         sm->clear = default_clear_screen;
      }
      if (!sm->scroll) {
         sm->scroll = default_scroll_screen;
      }
      if (!sm->write_character) {
         sm->write_character = default_write_character;
      }
      if (!sm->read_character) {
         sm->read_character = default_read_character;
      }
      switch (sm->bpp) {
      case 16:
         sm->set_colour = default_set_colour_16bpp;
         sm->get_colour = default_get_colour_16bpp;
         sm->set_pixel  = default_set_pixel_16bpp;
         sm->get_pixel  = default_get_pixel_16bpp;
         break;
      case 32:
         sm->set_colour = default_set_colour_32bpp;
         sm->get_colour = default_get_colour_32bpp;
         sm->set_pixel  = default_set_pixel_32bpp;
         sm->get_pixel  = default_get_pixel_32bpp;
         break;
      default:
         sm->set_colour = default_set_colour_8bpp;
         sm->get_colour = default_get_colour_8bpp;
         sm->set_pixel  = default_set_pixel_8bpp;
         sm->get_pixel  = default_get_pixel_8bpp;
         break;
      }
      if (!sm->font) {
         sm->font = get_font_by_number(DEFAULT_FONT);
      }
   }
   return sm;
}

uint32_t get_fb_address() {
   return (uint32_t) fb;
}
