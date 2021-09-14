#include <stdio.h>
#include <string.h>

#include "../startup.h"
#include "../rpi-mailbox.h"
#include "../rpi-mailbox-interface.h"
#include "../rpi-base.h"

#include "screen_modes.h"
#include "fonts.h"
#include "teletext.h"
#include "framebuffer.h"

#ifdef USE_V3D
#include "v3d.h"
#endif

// Registers to read the physical screen size
#ifdef RPI4
#define PIXELVALVE2_HORZB (volatile uint32_t *)(PERIPHERAL_BASE + 0x20A010)
#define PIXELVALVE2_VERTB (volatile uint32_t *)(PERIPHERAL_BASE + 0x20A018)
#else
#define PIXELVALVE2_HORZB (volatile uint32_t *)(PERIPHERAL_BASE + 0x807010)
#define PIXELVALVE2_VERTB (volatile uint32_t *)(PERIPHERAL_BASE + 0x807018)
#endif

unsigned char* fb = NULL;

// Maximum number of logical colours
#define NUM_COLOURS 256

typedef struct {
   int x1;
   int y1;
   int x2;
   int y2;
} rectangle_t;

// Colour palette request blocks

#define PALETTE_DATA_OFFSET 7

__attribute__((aligned(64))) static uint32_t palette0_base[PROP_BUFFER_SIZE];
__attribute__((aligned(64))) static uint32_t palette1_base[PROP_BUFFER_SIZE];

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
      .log2bpp       = 3,
      .log2bpc       = 3,
      .ncolour       = 1,
      .par           = 0.0f
   },
   {
      .mode_num      = 1,
      .width         = 320,
      .height        = 256,
      .xeigfactor    = 2,
      .yeigfactor    = 2,
      .log2bpp       = 3,
      .log2bpc       = 3,
      .ncolour       = 3,
      .par           = 0.0f
   },
   {
      .mode_num      = 2,
      .width         = 160,
      .height        = 256,
      .xeigfactor    = 3,
      .yeigfactor    = 2,
      .log2bpp       = 3,
      .log2bpc       = 4,
      .ncolour       = 15,
      .par           = 0.0f
   },
   {
      .mode_num      = 3,
      .mode_flags    = F_NON_GRAPHICS | F_BBC_GAP,
      .width         = 640,
      .height        = 250,
      .xeigfactor    = 1,
      .yeigfactor    = 2,
      .log2bpp       = 3,
      .log2bpc       = 3,
      .ncolour       = 1,
      .par           = 0.0f
   },
   {
      .mode_num      = 4,
      .width         = 320,
      .height        = 256,
      .xeigfactor    = 2,
      .yeigfactor    = 2,
      .log2bpp       = 3,
      .log2bpc       = 3,
      .ncolour       = 1,
      .par           = 0.0f
   },
   {
      .mode_num      = 5,
      .width         = 160,
      .height        = 256,
      .xeigfactor    = 3,
      .yeigfactor    = 2,
      .log2bpp       = 3,
      .log2bpc       = 4,
      .ncolour       = 3,
      .par           = 0.0f
   },
   {
      .mode_num      = 6,
      .mode_flags    = F_NON_GRAPHICS | F_BBC_GAP,
      .width         = 320,
      .height        = 250,
      .xeigfactor    = 2,
      .yeigfactor    = 2,
      .log2bpp       = 3,
      .log2bpc       = 3,
      .ncolour       = 1,
      .par           = 0.0f
   },
   // mode 7 is implemented in teletext.c
   {
      .mode_num      = 8,
      .width         = 640,
      .height        = 256,
      .xeigfactor    = 1,
      .yeigfactor    = 2,
      .log2bpp       = 3,
      .log2bpc       = 3,
      .ncolour       = 3,
      .par           = 0.0f
   },
   {
      .mode_num      = 9,
      .width         = 320,
      .height        = 256,
      .xeigfactor    = 2,
      .yeigfactor    = 2,
      .log2bpp       = 3,
      .log2bpc       = 3,
      .ncolour       = 15,
      .par           = 0.0f
   },
   {
      .mode_num      = 10,
      .mode_flags    = F_FULL_PALETTE,
      .width         = 160,
      .height        = 256,
      .xeigfactor    = 3,
      .yeigfactor    = 2,
      .log2bpp       = 3,
      .log2bpc       = 4,
      .ncolour       = 255,
      .par           = 0.0f
   },
   {
      .mode_num      = 11,
      .mode_flags    = F_GAP,
      .width         = 640,
      .height        = 250,
      .xeigfactor    = 1,
      .yeigfactor    = 2,
      .log2bpp       = 3,
      .log2bpc       = 3,
      .ncolour       = 3,
      .par           = 0.0f
   },
   {
      .mode_num      = 12,
      .width         = 640,
      .height        = 256,
      .xeigfactor    = 1,
      .yeigfactor    = 2,
      .log2bpp       = 3,
      .log2bpc       = 3,
      .ncolour       = 15,
      .par           = 0.0f
   },
   {
      .mode_num      = 13,
      .mode_flags    = F_FULL_PALETTE,
      .width         = 320,
      .height        = 256,
      .xeigfactor    = 2,
      .yeigfactor    = 2,
      .log2bpp       = 3,
      .log2bpc       = 3,
      .ncolour       = 255,
      .par           = 0.0f
   },
   {
      .mode_num      = 14,
      .mode_flags    = F_GAP,
      .width         = 640,
      .height        = 250,
      .xeigfactor    = 1,
      .yeigfactor    = 2,
      .log2bpp       = 3,
      .log2bpc       = 3,
      .ncolour       = 15,
      .par           = 0.0f
   },
   {
      .mode_num      = 15,
      .mode_flags    = F_FULL_PALETTE,
      .width         = 640,
      .height        = 256,
      .xeigfactor    = 1,
      .yeigfactor    = 2,
      .log2bpp       = 3,
      .log2bpc       = 3,
      .ncolour       = 255,
      .par           = 0.0f
   },
   {
      .mode_num      = 16,
      .width         = 1056,
      .height        = 256,
      .xeigfactor    = 1,
      .yeigfactor    = 2,
      .log2bpp       = 3,
      .log2bpc       = 3,
      .ncolour       = 15,
      .par           = 0.0f
   },
   {
      .mode_num      = 17,
      .mode_flags    = F_GAP,
      .width         = 1056,
      .height        = 250,
      .xeigfactor    = 1,
      .yeigfactor    = 2,
      .log2bpp       = 3,
      .log2bpc       = 3,
      .ncolour       = 15,
      .par           = 0.0f
   },
   {
      .mode_num      = 18,
      .width         = 640,
      .height        = 512,
      .xeigfactor    = 1,
      .yeigfactor    = 1,
      .log2bpp       = 3,
      .log2bpc       = 3,
      .ncolour       = 1,
      .par           = 0.0f
   },
   {
      .mode_num      = 19,
      .width         = 640,
      .height        = 512,
      .xeigfactor    = 1,
      .yeigfactor    = 1,
      .log2bpp       = 3,
      .log2bpc       = 3,
      .ncolour       = 3,
      .par           = 0.0f
   },
   {
      .mode_num      = 20,
      .width         = 640,
      .height        = 512,
      .xeigfactor    = 1,
      .yeigfactor    = 1,
      .log2bpp       = 3,
      .log2bpc       = 3,
      .ncolour       = 15,
      .par           = 0.0f
   },
   {
      .mode_num      = 21,
      .mode_flags    = F_FULL_PALETTE,
      .width         = 640,
      .height        = 512,
      .xeigfactor    = 1,
      .yeigfactor    = 1,
      .log2bpp       = 3,
      .log2bpc       = 3,
      .ncolour       = 255,
      .par           = 0.0f
   },
   {
      .mode_num      = 22,
      .width         = 768,
      .height        = 288,
      .xeigfactor    = 0,
      .yeigfactor    = 1,
      .log2bpp       = 3,
      .log2bpc       = 3,
      .ncolour       = 15,
      .par           = 0.0f
   },
   {
      .mode_num      = 23,
      .mode_flags    = F_HIRES_MONO | F_DOUBLE_HEIGHT_VDU_CHARS,
      .width         = 1152,
      .height        = 896,
      .xeigfactor    = 1,
      .yeigfactor    = 1,
      .log2bpp       = 3,
      .log2bpc       = 3,
      .ncolour       = 1,
      .par           = 0.0f
   },
   {
      .mode_num      = 24,
      .mode_flags    = F_FULL_PALETTE,
      .width         = 1056,
      .height        = 256,
      .xeigfactor    = 1,
      .yeigfactor    = 2,
      .log2bpp       = 3,
      .log2bpc       = 3,
      .ncolour       = 255,
      .par           = 0.0f
   },
   {
      .mode_num      = 25,
      .width         = 640,
      .height        = 480,
      .xeigfactor    = 1,
      .yeigfactor    = 1,
      .log2bpp       = 3,
      .log2bpc       = 3,
      .ncolour       = 1,
      .par           = 0.0f
   },
   {
      .mode_num      = 26,
      .width         = 640,
      .height        = 480,
      .xeigfactor    = 1,
      .yeigfactor    = 1,
      .log2bpp       = 3,
      .log2bpc       = 3,
      .ncolour       = 3,
      .par           = 0.0f
   },
   {
      .mode_num      = 27,
      .width         = 640,
      .height        = 480,
      .xeigfactor    = 1,
      .yeigfactor    = 1,
      .log2bpp       = 3,
      .log2bpc       = 3,
      .ncolour       = 15,
      .par           = 0.0f
   },
   {
      .mode_num      = 28,
      .mode_flags    = F_FULL_PALETTE,
      .width         = 640,
      .height        = 480,
      .xeigfactor    = 1,
      .yeigfactor    = 1,
      .log2bpp       = 3,
      .log2bpc       = 3,
      .ncolour       = 255,
      .par           = 0.0f
   },
   {
      .mode_num      = 29,
      .width         = 800,
      .height        = 600,
      .xeigfactor    = 1,
      .yeigfactor    = 1,
      .log2bpp       = 3,
      .log2bpc       = 3,
      .ncolour       = 1,
      .par           = 0.0f
   },
   {
      .mode_num      = 30,
      .width         = 800,
      .height        = 600,
      .xeigfactor    = 1,
      .yeigfactor    = 1,
      .log2bpp       = 3,
      .log2bpc       = 3,
      .ncolour       = 3,
      .par           = 0.0f
   },
   {
      .mode_num      = 31,
      .width         = 800,
      .height        = 600,
      .xeigfactor    = 1,
      .yeigfactor    = 1,
      .log2bpp       = 3,
      .log2bpc       = 3,
      .ncolour       = 15,
      .par           = 0.0f
   },
   {
      .mode_num      = 32,
      .mode_flags    = F_FULL_PALETTE,
      .width         = 800,
      .height        = 600,
      .xeigfactor    = 1,
      .yeigfactor    = 1,
      .log2bpp       = 3,
      .log2bpc       = 3,
      .ncolour       = 255,
      .par           = 0.0f
   },
   {
      .mode_num      = 33,
      .width         = 768,
      .height        = 288,
      .xeigfactor    = 1,
      .yeigfactor    = 2,
      .log2bpp       = 3,
      .log2bpc       = 3,
      .ncolour       = 1,
      .par           = 0.0f
   },
   {
      .mode_num      = 34,
      .width         = 768,
      .height        = 288,
      .xeigfactor    = 1,
      .yeigfactor    = 2,
      .log2bpp       = 3,
      .log2bpc       = 3,
      .ncolour       = 3,
      .par           = 0.0f
   },
   {
      .mode_num      = 35,
      .width         = 768,
      .height        = 288,
      .xeigfactor    = 1,
      .yeigfactor    = 2,
      .log2bpp       = 3,
      .log2bpc       = 3,
      .ncolour       = 15,
      .par           = 0.0f
   },
   {
      .mode_num      = 36,
      .mode_flags    = F_FULL_PALETTE,
      .width         = 768,
      .height        = 288,
      .xeigfactor    = 1,
      .yeigfactor    = 2,
      .log2bpp       = 3,
      .log2bpc       = 3,
      .ncolour       = 255,
      .par           = 0.0f
   },
   {
      .mode_num      = 37,
      .width         = 896,
      .height        = 352,
      .xeigfactor    = 1,
      .yeigfactor    = 2,
      .log2bpp       = 3,
      .log2bpc       = 3,
      .ncolour       = 1,
      .par           = 0.0f
   },
   {
      .mode_num      = 38,
      .width         = 896,
      .height        = 352,
      .xeigfactor    = 1,
      .yeigfactor    = 2,
      .log2bpp       = 3,
      .log2bpc       = 3,
      .ncolour       = 3,
      .par           = 0.0f
   },
   {
      .mode_num      = 39,
      .width         = 896,
      .height        = 352,
      .xeigfactor    = 1,
      .yeigfactor    = 2,
      .log2bpp       = 3,
      .log2bpc       = 3,
      .ncolour       = 15,
      .par           = 0.0f
   },
   {
      .mode_num      = 40,
      .mode_flags    = F_FULL_PALETTE,
      .width         = 896,
      .height        = 352,
      .xeigfactor    = 1,
      .yeigfactor    = 2,
      .log2bpp       = 3,
      .log2bpc       = 3,
      .ncolour       = 255,
      .par           = 0.0f
   },
   {
      .mode_num      = 41,
      .width         = 640,
      .height        = 352,
      .xeigfactor    = 1,
      .yeigfactor    = 2,
      .log2bpp       = 3,
      .log2bpc       = 3,
      .ncolour       = 1,
      .par           = 0.0f
   },
   {
      .mode_num      = 42,
      .width         = 640,
      .height        = 352,
      .xeigfactor    = 1,
      .yeigfactor    = 2,
      .log2bpp       = 3,
      .log2bpc       = 3,
      .ncolour       = 3,
      .par           = 0.0f
   },
   {
      .mode_num      = 43,
      .width         = 640,
      .height        = 352,
      .xeigfactor    = 1,
      .yeigfactor    = 2,
      .log2bpp       = 3,
      .log2bpc       = 3,
      .ncolour       = 15,
      .par           = 0.0f
   },
   {
      .mode_num      = 44,
      .width         = 640,
      .height        = 200,
      .xeigfactor    = 1,
      .yeigfactor    = 2,
      .log2bpp       = 3,
      .log2bpc       = 3,
      .ncolour       = 1,
      .par           = 0.0f
   },
   {
      .mode_num      = 45,
      .width         = 640,
      .height        = 200,
      .xeigfactor    = 1,
      .yeigfactor    = 2,
      .log2bpp       = 3,
      .log2bpc       = 3,
      .ncolour       = 3,
      .par           = 0.0f
   },
   {
      .mode_num      = 46,
      .width         = 640,
      .height        = 200,
      .xeigfactor    = 1,
      .yeigfactor    = 2,
      .log2bpp       = 3,
      .log2bpc       = 3,
      .ncolour       = 15,
      .par           = 0.0f
   },
   {
      .mode_num      = 47,
      .mode_flags    = F_FULL_PALETTE,
      .width         = 360,
      .height        = 480,
      .xeigfactor    = 2,
      .yeigfactor    = 1,
      .log2bpp       = 3,
      .log2bpc       = 3,
      .ncolour       = 255,
      .par           = 0.0f
   },
   {
      .mode_num      = 48,
      .width         = 320,
      .height        = 480,
      .xeigfactor    = 2,
      .yeigfactor    = 1,
      .log2bpp       = 3,
      .log2bpc       = 3,
      .ncolour       = 15,
      .par           = 0.0f
   },
   {
      .mode_num      = 49,
      .mode_flags    = F_FULL_PALETTE,
      .width         = 320,
      .height        = 480,
      .xeigfactor    = 2,
      .yeigfactor    = 1,
      .log2bpp       = 3,
      .log2bpc       = 3,
      .ncolour       = 255,
      .par           = 0.0f
   },
   {
      .mode_num      = 50,
      .width         = 320,
      .height        = 240,
      .xeigfactor    = 2,
      .yeigfactor    = 2,
      .log2bpp       = 3,
      .log2bpc       = 3,
      .ncolour       = 1,
      .par           = 0.0f
   },
   {
      .mode_num      = 51,
      .width         = 320,
      .height        = 240,
      .xeigfactor    = 2,
      .yeigfactor    = 2,
      .log2bpp       = 3,
      .log2bpc       = 3,
      .ncolour       = 3,
      .par           = 0.0f
   },
   {
      .mode_num      = 52,
      .width         = 320,
      .height        = 240,
      .xeigfactor    = 2,
      .yeigfactor    = 2,
      .log2bpp       = 3,
      .log2bpc       = 3,
      .ncolour       = 15,
      .par           = 0.0f
   },
   {
      .mode_num      = 53,
      .mode_flags    = F_FULL_PALETTE,
      .width         = 320,
      .height        = 240,
      .xeigfactor    = 2,
      .yeigfactor    = 2,
      .log2bpp       = 3,
      .log2bpc       = 3,
      .ncolour       = 255,
      .par           = 0.0f
   },
   // 8 bpp
   {
      .mode_num      = 64,
      .mode_flags    = F_FULL_PALETTE,
      .width         = 640,
      .height        = 512,
      .xeigfactor    = 1,
      .yeigfactor    = 1,
      .log2bpp       = 3,
      .log2bpc       = 3,
      .ncolour       = 255,
      .par           = 0.0f
   },
   {
      .mode_num      = 65,
      .mode_flags    = F_FULL_PALETTE,
      .width         = 1280,
      .height        = 1024,
      .xeigfactor    = 0,
      .yeigfactor    = 0,
      .log2bpp       = 3,
      .log2bpc       = 3,
      .ncolour       = 255,
      .par           = 0.0f
   },
   // 16 bpp
   {
      .mode_num      = 66,
      .width         = 640,
      .height        = 512,
      .xeigfactor    = 1,
      .yeigfactor    = 1,
      .log2bpp       = 4,
      .log2bpc       = 4,
      .ncolour       = 0xffff,
      .par           = 0.0f
   },
   {
      .mode_num      = 67,
      .width         = 1280,
      .height        = 1024,
      .xeigfactor    = 0,
      .yeigfactor    = 0,
      .log2bpp       = 4,
      .log2bpc       = 4,
      .ncolour       = 0xffff,
      .par           = 0.0f
   },
   // 32 bpp
   {
      .mode_num      = 68,
      .width         = 640,
      .height        = 512,
      .xeigfactor    = 1,
      .yeigfactor    = 1,
      .log2bpp       = 5,
      .log2bpc       = 5,
      .ncolour       = 0xffffff,
      .par           = 0.0f
   },
   {
      .mode_num      = 69,
      .width         = 1280,
      .height        = 1024,
      .xeigfactor    = 0,
      .yeigfactor    = 0,
      .log2bpp       = 5,
      .log2bpc       = 5,
      .ncolour       = 0xffffff,
      .par           = 0.0f
   },
   {
      .mode_num      = CUSTOM_8BPP_SCREEN_MODE,
      .mode_flags    = F_FULL_PALETTE,
      .log2bpp       = 3,
      .log2bpc       = 3,
      .par           = 0.0f
   },
   {
      .mode_num      = CUSTOM_16BPP_SCREEN_MODE,
      .log2bpp       = 4,
      .log2bpc       = 4,
      .par           = 0.0f
   },
   {
      .mode_num      = CUSTOM_32BPP_SCREEN_MODE,
      .log2bpp       = 5,
      .log2bpc       = 5,
      .par           = 0.0f
   },
   {
      .mode_num      = -1,
   }
};

// ==========================================================================
// Static methods
// ==========================================================================

static void update_palette(screen_mode_t *screen, int mark) {
   static int last_mark = 0;
   if (mark < 0) {
      mark = last_mark;
   }
   uint32_t *pt = mark ? palette0_base : palette1_base;
   // These are overwritten by the previous response
   pt[1] = 0;
   pt[4] = 0;
   // Don't block waiting for the response
   RPI_Mailbox0Write( MB0_TAGS_ARM_TO_VC, pt );
   // Remember the currently selected palette
   last_mark = mark;
}

static void init_colour_table(screen_mode_t *screen) {
   if (screen->ncolour == 1) {
      // Default 2-Colour Palette
      // Colour  0 = Black
      // Colour  1 = White
      for (uint32_t i = 0; i < NUM_COLOURS * 2; i++) {
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
      for (uint32_t i = 0; i < NUM_COLOURS * 2; i++) {
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
      // Colour  1 = Red
      // Colour  2 = Green
      // Colour  3 = Yellow
      // Colour  4 = Blue
      // Colour  5 = Magenta
      // Colour  6 = Cyan
      // Colour  7 = White
      // Colour  8 = Black/White
      // Colour  9 = Red/Cyan
      // Colour 10 = Green/Magenta
      // Colour 11 = Yellow/Blue
      // Colour 12 = Blue/Yellow
      // Colour 13 = Magenta/Green
      // Colour 14 = Cyan/Red
      // Colour 15 = White/Black
      for (uint32_t i = 0; i < NUM_COLOURS * 2; i++) {
         int c = ((i & 0x108) == 0x108) ? 7 - (i & 7) : (i & 7);
         int b = (c & 4) ? 0xff : 0x00;
         int g = (c & 2) ? 0xff : 0x00;
         int r = (c & 1) ? 0xff : 0x00;
         screen->set_colour(screen, i, r, g, b);
      }
   } else {
      // Default 256-colour mode palette
      // Bits 1,0 = Tint (0x00, 0x11, 0x22, 0x33) added
      // Bits 4,2 = R    (0x00, 0x44, 0x88, 0xCC)
      // Bits 6,5 = G    (0x00, 0x44, 0x88, 0xCC)
      // Bits 7,3 = B    (0x00, 0x44, 0x88, 0xCC)
      for (int i = 0; i < NUM_COLOURS * 2; i++) {
         // Tint
         int tint = (i & 0x03) * 0x11;
         // Colour
         int r = (((i >> 3) & 0x02) | ((i >> 2) & 0x01)) * 0x44 + tint;
         int g = (((i >> 5) & 0x02) | ((i >> 5) & 0x01)) * 0x44 + tint;
         int b = (((i >> 6) & 0x02) | ((i >> 3) & 0x01)) * 0x44 + tint;
         screen->set_colour(screen, (uint32_t)i, r, g, b);
      }
   }
   // Prepare the palette request messages, to make flashing efficient
   if (screen->log2bpp == 3) {
      uint32_t n = NUM_COLOURS;
      for (int i = 0; i < 2; i++) {
         uint32_t *p = i ? palette1_base : palette0_base;
         p[0]     = (n + 8) * 4;         // 0: property buffer: length (bytes)
         p[1]     = 0;                   // 1: property byffer: 0=request
         p[2]     = TAG_SET_PALETTE;     // 2: tag header: tag
         p[3]     = (n + 2) * 4;         // 3: tag header: length of body (bytes)
         p[4]     = 0;                   // 4: tag header: 0=request
         p[5]     = 0;                   // 5: tag body: offset
         p[6]     = n;                   // 6: tag body: number of colours
         p[7 + n] = 0;                   // 7: property buffer: terminator
      }
   }
}

static int get_hdisplay() {
#ifdef RPI4
   return  ((*PIXELVALVE2_HORZB) & 0xFFFF) * 2;
#else
    return (*PIXELVALVE2_HORZB) & 0xFFFF;
#endif
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
      int font_width  = font->get_overall_w(font);
      int font_height = font->get_overall_h(font);
      r->x1 = text_window->left * font_width;
      r->y1 = screen->height - 1 - (text_window->bottom * font_height + font_height - 1);
      r->x2 = (text_window->right + 1) * font_width - 1;
      r->y2 = screen->height - 1 - text_window->top * font_height;
   }
}

static int is_full_screen(screen_mode_t *screen, rectangle_t *r) {
   return (r->x1 == 0 && r->y1 == 0 && r->x2 == screen->width - 1 && r->y2 == screen->height - 1);
}

static void null_handler() {
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

    int h_corrected;
    int v_corrected;

    if (screen->par == 1.0f) {
       // Square pixels
       h_corrected = screen->width;
       v_corrected = screen->height;
    } else if (screen->par > 1.0f) {
       // Wide pixels
       h_corrected = (int) (((float)screen->width) * screen->par);
       v_corrected = screen->height;
    } else {
       // Narrow pixels
       h_corrected = screen->width;
       v_corrected = (int) (((float)screen->height) / screen->par);
    }

    int h_scale = 2 * h_display / h_corrected;
    int v_scale = 2 * v_display / v_corrected;

    int scale = (h_scale < v_scale) ? h_scale : v_scale;

    int h_window = scale * h_corrected / 2;
    int v_window = scale * v_corrected / 2;

    int h_overscan = (h_display - h_window) / 2;
    int v_overscan = (v_display - v_window) / 2;

#ifdef DEBUG_VDU
    printf("         display: %d x %d\r\n", h_display, v_display);
    printf("     framebuffer: %d x %d\r\n", screen->width, screen->height);
    printf("aspect corrected: %d x %d\r\n", h_corrected, v_corrected);
    printf("         scaling: %1.1f x %1.1f\r\n", (double)(((float) h_window) / ((float) screen->width)), (double)(((float) v_window) / ((float) screen->height)));
    printf("  display window: %d x %d\r\n", h_window, v_window);
    printf("display overscan: %d x %d\r\n", h_overscan, v_overscan);
#endif

    /* Initialise a framebuffer... */
    RPI_PropertyInit();
    RPI_PropertyAddTag(TAG_ALLOCATE_BUFFER);
    RPI_PropertyAddTag(TAG_SET_PHYSICAL_SIZE, screen->width, screen->height );
    RPI_PropertyAddTag(TAG_SET_VIRTUAL_SIZE,  screen->width, screen->height * 2 ); // TODO: FIX ME (remove the * 2)
    RPI_PropertyAddTag(TAG_SET_DEPTH, (1 << screen->log2bpp));
    RPI_PropertyAddTag(TAG_GET_PITCH );
    RPI_PropertyAddTag(TAG_GET_PHYSICAL_SIZE );
    RPI_PropertyAddTag(TAG_GET_DEPTH );
    RPI_PropertyAddTag(TAG_SET_OVERSCAN, v_overscan, v_overscan, h_overscan, h_overscan);
    RPI_PropertyProcess();

#ifdef DEBUG_VDU
    if( ( mp = RPI_PropertyGet( TAG_GET_PHYSICAL_SIZE ) ) )
    {
        uint32_t width = mp->data.buffer_32[0];
        uint32_t height = mp->data.buffer_32[1];
        printf( "Initialised Framebuffer: %"PRId32"x%"PRId32, width, height );
    }

    if( ( mp = RPI_PropertyGet( TAG_GET_DEPTH ) ) )
    {
        uint32_t bpp = mp->data.buffer_32[0];
        printf( " %"PRId32"bpp\r\n", bpp );
    }
#endif

    if( ( mp = RPI_PropertyGet( TAG_GET_PITCH ) ) )
    {
        screen->pitch = (int)mp->data.buffer_32[0];
#ifdef DEBUG_VDU
        printf( "Pitch: %d bytes\r\n", screen->pitch );
#endif
    }

    if( ( mp = RPI_PropertyGet( TAG_ALLOCATE_BUFFER ) ) )
    {
        fb = (unsigned char*)mp->data.buffer_32[0];
#ifdef DEBUG_VDU
        printf( "Framebuffer address: %8.8X\r\n", (unsigned int)fb );
#endif
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
}

void default_reset_screen(screen_mode_t *screen) {
    /* Copy default colour table */
    init_colour_table(screen);

    /* Update the palette (this is a no-op in 8-bpp modes) */
    screen->update_palette(screen, 0);

    /* Initialize the font */
    font_t *font = get_font_by_number(DEFAULT_FONT);
    font->set_spacing_h(font, (screen->mode_flags & F_BBC_GAP) ? 2 : 0);
    screen->font = font;
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
   int font_height = font->get_overall_h(font);
   // Convert text window to screen graphics coordinates (0,0 = bottom left)
   to_rectangle(screen, text_window, &r);
   // Scroll the screen upwards one row, and clear the bottom text line to the background colour
   if (is_full_screen(screen, &r)) {
      if (screen->log2bpp == 3) {
         bg_col = bg_col | (bg_col << 8) | (bg_col << 16) | (bg_col << 24);
      } else if (screen->log2bpp == 4) {
         bg_col = bg_col | (bg_col << 16);
      }
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
   pixel_t *colour_t = ((index & 0x100) ? palette1_base : palette0_base) + PALETTE_DATA_OFFSET;
   colour_t[index & 0xff] = 0xFF000000 | ((b & 0xFF) << 16) | ((g & 0xFF) << 8) | (r & 0xFF);
}

void default_set_colour_16bpp(screen_mode_t *screen, colour_index_t index, int r, int g, int b) {
}

void default_set_colour_32bpp(screen_mode_t *screen, colour_index_t index, int r, int g, int b) {
}

pixel_t default_get_colour_8bpp(screen_mode_t *screen, uint8_t gcol) {
   //                                     7  6  5  4  3  2  1  0
   // The          GCOL number format is B3 B2 G3 G2 R3 R2 T1 T0
   // The  8-bit colour number format is B3 G3 G2 R3 B2 R2 T1 T0
   if (screen->ncolour == 255) {
      return (pixel_t)((gcol & 0x87) | ((gcol & 0x38) << 1) | ((gcol & 0x40) >> 3));
   } else {
      return (pixel_t)(gcol & screen->ncolour);
   }
}

pixel_t default_get_colour_16bpp(screen_mode_t *screen, uint8_t gcol) {
   //                                     7  6  5  4  3  2  1  0
   // The          GCOL number format is B3 B2 G3 G2 R3 R2 T1 T0
   //                                    15 14 13 12 11 10  9  8  7  6  5  4  3  2  1  0
   // The 16-bit colour number format is R4 R3 R2 R1 R0 G5 G4 G3 G2 G1 G0 B4 B3 B2 B1 B0
   int r = ((gcol & 0x0C)     ) | (gcol & 0x03);
   int g = ((gcol & 0x30) >> 2) | (gcol & 0x03);
   int b = ((gcol & 0xC0) >> 4) | (gcol & 0x03);
   return (pixel_t)((r << 12) | (g << 7) | (b << 1));
}

pixel_t default_get_colour_32bpp(screen_mode_t *screen, uint8_t gcol) {
   //                                     7  6  5  4  3  2  1  0
   // The          GCOL number format is B3 B2 G3 G2 R3 R2 T1 T0
   // The 32-bit colour number format is xxBBGGRR i.e. the same as RISC OS
   // (this requires framebuffer_swap=0 in config.txt)
   int r = ((gcol & 0x0C)     ) | (gcol & 0x03);
   int g = ((gcol & 0x30) >> 2) | (gcol & 0x03);
   int b = ((gcol & 0xC0) >> 4) | (gcol & 0x03);
   r |= r << 4;
   g |= g << 4;
   b |= b << 4;
   return (pixel_t)((b << 16) | (g << 8) | r);
}

pixel_t default_nearest_colour_8bpp(struct screen_mode *screen, uint8_t r, uint8_t g, uint8_t b) {
   // Max distance is 7 * 255 * 255 which fits easily in an int
   int distance = 0x7fffffff;
   colour_index_t best = 0;
   for (colour_index_t i = 0; i <= screen->ncolour && distance != 0; i++) {
      pixel_t colour = palette0_base[i + PALETTE_DATA_OFFSET];
      // xxBBGGRR
      int dr = r - (colour & 0xff);
      int dg = g - ((colour >> 8) & 0xff);
      int db = b - ((colour >> 16) & 0xff);
      int d = 2 * dr * dr + 4 * dg * dg + db * db;
      if (d < distance) {
         distance = d;
         best = i;
      }
   }
   return best;
}

pixel_t default_nearest_colour_16bpp(struct screen_mode *screen, uint8_t r, uint8_t g, uint8_t b) {
   //                                    15 14 13 12 11 10  9  8  7  6  5  4  3  2  1  0
   // The 16-bit colour number format is R4 R3 R2 R1 R0 G5 G4 G3 G2 G1 G0 B4 B3 B2 B1 B0
   return (pixel_t)(((r & 0xF8) << 8) | ((g & 0xF8) << 3) | ((b & 0xF8) >> 3));
}

pixel_t default_nearest_colour_32bpp(struct screen_mode *screen, uint8_t r, uint8_t g, uint8_t b) {
   // The 32-bit colour number format is xxBBGGRR i.e. the same as RISC OS
   // (this requires framebuffer_swap=0 in config.txt)
   return (pixel_t)((b << 16) | (g << 8) | r);
}


void default_set_pixel_8bpp(screen_mode_t *screen, int x, int y, pixel_t value) {
   uint8_t *fbptr = (uint8_t *)(fb + (screen->height - y - 1) * screen->pitch + x);
   *fbptr = (uint8_t)value;
}

void default_set_pixel_16bpp(screen_mode_t *screen, int x, int y, pixel_t value) {
   uint16_t *fbptr = (uint16_t *)(fb + (screen->height - y - 1) * screen->pitch + x * 2);
   *fbptr = (uint16_t)value;

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
   // Convert Row/Col to screen coordinates
   int x = col * font->get_overall_w(font);
   int y = screen->height - row * font->get_overall_h(font) - 1;
   // Pass down to font to do the drawing
   font->write_char(font, screen, c, x, y, fg_col, bg_col);
}

int default_read_character(screen_mode_t *screen, int col, int row, pixel_t bg_col) {
   font_t *font = screen->font;
   // Convert Row/Col to screen coordinates
   int x = col * font->get_overall_w(font);
   int y = screen->height - row * font->get_overall_h(font) - 1;
   // Pass down to font to do the reading
   return font->read_char(font, screen, x, y, bg_col);
}

void default_unknown_vdu(screen_mode_t *screen, uint8_t *buf) {
}

void default_flash(screen_mode_t *screen, int mark) {
   update_palette(screen, mark);
}

// ==========================================================================
// Public methods
// ==========================================================================

screen_mode_t *get_screen_mode(int mode_num) {
   // Ask the teletext mode to resolve the screen first
   screen_mode_t *sm = tt_get_screen_mode(mode_num);

   // Then search the the screen mode table in this file
   if (sm == NULL) {
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
      if (!sm->unknown_vdu) {
         sm->unknown_vdu = default_unknown_vdu;
      }
      switch (sm->log2bpp) {
      case 4:
         sm->set_colour     = default_set_colour_16bpp;
         sm->get_colour     = default_get_colour_16bpp;
         sm->nearest_colour = default_nearest_colour_16bpp;
         sm->update_palette = null_handler;
         sm->set_pixel      = default_set_pixel_16bpp;
         sm->get_pixel      = default_get_pixel_16bpp;
         break;
      case 5:
         sm->set_colour     = default_set_colour_32bpp;
         sm->get_colour     = default_get_colour_32bpp;
         sm->nearest_colour = default_nearest_colour_32bpp;
         sm->update_palette = null_handler;
         sm->set_pixel      = default_set_pixel_32bpp;
         sm->get_pixel      = default_get_pixel_32bpp;
         break;
      default:
         sm->set_colour     = default_set_colour_8bpp;
         sm->get_colour     = default_get_colour_8bpp;
         sm->nearest_colour = default_nearest_colour_8bpp;
         sm->update_palette = update_palette;
         sm->set_pixel      = default_set_pixel_8bpp;
         sm->get_pixel      = default_get_pixel_8bpp;
         break;
      }
      if (sm->par == 0.0F) {
         sm->par = ((float) (1 << sm->xeigfactor)) / ((float) (1 << sm->yeigfactor));
      }
      if (!sm->flash && sm->log2bpp == 3) {
         sm->flash = default_flash;
      }

      // Set the colour index for while, avoiding flashing colours in 16-colour modes
      if (sm->ncolour == 1) {
         sm->white = 1;
      } else if (sm->ncolour == 3) {
         sm->white = 3;
      } else if (sm->ncolour == 15) {
         sm->white = 7;
      } else {
         sm->white = 255;
      }
   }
   return sm;
}

uint32_t get_fb_address() {
   return (uint32_t) fb;
}

int32_t fb_read_mode_variable(mode_variable_t v, screen_mode_t *screen) {
   switch (v) {
   case M_MODEFLAGS:
      // Assorted flags
      return screen->mode_flags;
   case M_SCRRCOL:
      // Number of text columns -1, assumes system font is 8x8
      return (screen->width >> 3) - 1;
   case M_SCRBROW:
      // Number of text rows -1, assumes system font is 8x8
      return (screen->height >> 3) - 1;
   case M_NCOLOUR:
      // Maximum logical colour
      return screen->ncolour;
   case M_XEIGFACTOR:
      // Conversion factor between OS units and pixels
      return screen->xeigfactor;
   case M_YEIGFACTOR:
      // Conversion factor between OS units and pixels
      return screen->yeigfactor;
   case M_LINELENGTH:
      // Number of bytes per pixel row
      return screen->width * (1 << (screen->log2bpp - 3));
   case M_SCREENSIZE:
      // Number of bytes for entire screen display
      return screen->height * screen->width * (1 << (screen->log2bpp - 3));
   case M_YSHIFTSIZE:
      // Deprecated. Do not use
      return 0;
   case M_LOG2BPP:
      // Log base 2 of bits per pixel
      return screen->log2bpp;
   case M_LOG2BPC:
      // Log base 2 of bytes per character
      return screen->log2bpc;
   case M_XWINDLIMIT:
      // Number of x pixels on screen -1
      return screen->width - 1;
   case M_YWINDLIMIT:
      // Number of y pixels on screen -1
      return screen->height - 1;
   default:
      return 0;
   }
   return 0;
}
