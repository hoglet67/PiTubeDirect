#ifndef _PRIMITIVES_H
#define _PRIMITIVES_H

#include <inttypes.h>
#include "screen_modes.h"

typedef enum {
   HL_LR_NB = 1, // Horizontal line fill (left & right) to non-background
   HL_RO_BG = 2, // Horizontal line fill (right only) to background
   HL_LR_FG = 3, // Horizontal line fill (left & right) to foreground
   HL_RO_NF = 4, // Horizontal line fill (right only) to non-foreground
   AF_NONBG = 5, // Flood (area fill) to non-background
   AF_TOFGD = 6  // Flood (area fill) to foreground
} fill_t;

typedef enum {
   PM_NORMAL = 0,
   PM_OR     = 1,
   PM_AND    = 2,
   PM_XOR    = 3,
   PM_INVERT = 4
} plotmode_t;

void fb_set_graphics_origin   (int16_t x, int16_t y);
void fb_set_graphics_plotmode (uint8_t plotmode);
void fb_set_graphics_area     (screen_mode_t *screen, int16_t x1, int16_t y1, int16_t x2, int16_t y2);
void fb_clear_graphics_area   (screen_mode_t *screen, pixel_t colour);
void fb_set_pixel             (screen_mode_t *screen, int x, int y, pixel_t colour);
void fb_draw_line             (screen_mode_t *screen, int x1, int y1, int x2, int y2, pixel_t colour, uint8_t g_mode);
void fb_fill_triangle         (screen_mode_t *screen, int x1, int y1, int x2, int y2, int x3, int y3, pixel_t colour);
void fb_draw_triangle         (screen_mode_t *screen, int x1, int y1, int x2, int y2, int x3, int y3, pixel_t colour);
void fb_draw_circle           (screen_mode_t *screen, int xc, int yc, int xr, int yr, pixel_t colour);
void fb_fill_circle           (screen_mode_t *screen, int xc, int yc, int xr, int yr, pixel_t colour);
void fb_fill_rectangle        (screen_mode_t *screen, int x1, int y1, int x2, int y2, pixel_t colour);
void fb_draw_rectangle        (screen_mode_t *screen, int x1, int y1, int x2, int y2, pixel_t colour);
void fb_fill_parallelogram    (screen_mode_t *screen, int x1, int y1, int x2, int y2, int x3, int y3, pixel_t colour);
void fb_draw_parallelogram    (screen_mode_t *screen, int x1, int y1, int x2, int y2, int x3, int y3, pixel_t colour);
void fb_draw_ellipse          (screen_mode_t *screen, int xc, int yc, int width, int height, int shear, pixel_t colour);
void fb_fill_ellipse          (screen_mode_t *screen, int xc, int yc, int width, int height, int shear, pixel_t colour);
void fb_fill_area             (screen_mode_t *screen, int x, int y, pixel_t colour, fill_t mode);
void fb_draw_arc              (screen_mode_t *screen, int xc, int yc, int x1, int y1, int x2, int y2, pixel_t colour);
void fb_fill_chord            (screen_mode_t *screen, int xc, int yc, int x1, int y1, int x2, int y2, pixel_t colour);
void fb_fill_sector           (screen_mode_t *screen, int xc, int yc, int x1, int y1, int x2, int y2, pixel_t colour);

#endif
