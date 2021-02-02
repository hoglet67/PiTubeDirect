#ifndef _PRIMITIVES_H
#define _PRIMITIVES_H

#include <inttypes.h>
#include "screen_modes.h"
#include "fonts.h"

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


void prim_set_graphics_plotmode(plotmode_t plotmode);
void prim_set_graphics_area(screen_mode_t *screen, int16_t x1, int16_t y1, int16_t x2, int16_t y2);
void prim_clear_graphics_area(screen_mode_t *screen, pixel_t colour);
void prim_set_pixel(screen_mode_t *screen, int x, int y, pixel_t colour);
void prim_draw_line(screen_mode_t *screen, int x1, int y1, int x2, int y2, pixel_t colour, uint8_t linemode);
void prim_flood_fill(screen_mode_t *screen, int x, int y, pixel_t fill_col, pixel_t ref_col, int c);
void prim_fill_area(screen_mode_t *screen, int x, int y, pixel_t colour, fill_t mode);
void prim_draw_circle(screen_mode_t *screen, int xc, int yc, int r, pixel_t colour);
void prim_fill_circle(screen_mode_t *screen, int xc, int yc, int r, pixel_t colour);
void prim_draw_normal_ellipse(screen_mode_t *screen, int xc, int yc, int width, int height, pixel_t colour);
void prim_draw_sheared_ellipse(screen_mode_t *screen, int xc, int yc, int width, int height, int shear, pixel_t colour);
void prim_draw_sheared_ellipse(screen_mode_t *screen, int xc, int yc, int width, int height, int shear, pixel_t colour);
void prim_fill_normal_ellipse(screen_mode_t *screen, int xc, int yc, int width, int height, pixel_t colour);
void prim_fill_sheared_ellipse(screen_mode_t *screen, int xc, int yc, int width, int height, int shear, pixel_t colour);
void prim_fill_sheared_ellipse(screen_mode_t *screen, int xc, int yc, int width, int height, int shear, pixel_t colour);
void prim_fill_triangle(screen_mode_t *screen, int x1, int y1, int x2, int y2, int x3, int y3, pixel_t colour);
void prim_draw_arc(screen_mode_t *screen, int xc, int yc, int x1, int y1, int x2, int y2, unsigned int colour);
void prim_fill_chord(screen_mode_t *screen, int xc, int yc, int x1, int y1, int x2, int y2, pixel_t colour);
void prim_fill_sector(screen_mode_t *screen, int xc, int yc, int x1, int y1, int x2, int y2, pixel_t colour);
void prim_move_copy_rectangle(screen_mode_t *screen, int x1, int y1, int x2, int y2, int x3, int y3, int move);
void prim_fill_rectangle(screen_mode_t *screen, int x1, int y1, int x2, int y2, pixel_t colour);

#endif
