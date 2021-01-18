#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "primitives.h"
#include "framebuffer.h"

#ifdef USE_V3D
#include "v3d.h"
#endif

static uint8_t g_plotmode;

static int16_t g_x_origin;
static int16_t g_y_origin;

static int16_t g_x_min;
static int16_t g_y_min;
static int16_t g_x_max;
static int16_t g_y_max;

#define FLOOD_QUEUE_SIZE 16384

static int16_t flood_queue_x[FLOOD_QUEUE_SIZE];
static int16_t flood_queue_y[FLOOD_QUEUE_SIZE];
static int flood_queue_wr;
static int flood_queue_rd;

// TODO List
// - line drawing: support dot patterns
// - line drawing: support omission of end points
// - draw circular arcs
// - fill chord segments
// - fill sector
// - move/copy rectangle
// - fill patterns
// - non-axis aligned elipses
// - bug: horizontal line fills overwrite the terminating pixel

// ==========================================================================
// Static methods (operate at screen resolution)
// ==========================================================================

static int calc_radius(int x1, int y1, int x2, int y2) {
   return (int)(sqrt((x2-x1)*(x2-x1)+(y2-y1)*(y2-y1)) + 0.5);
}

static pixel_t get_pixel(screen_mode_t *screen, int x, int y) {
   if (x < g_x_min  || x > g_x_max || y < g_y_min || y > g_y_max) {
      // Return the graphics background colour if off the screen
      return screen->get_colour(screen, fb_get_g_bg_col());
   } else {
      return screen->get_pixel(screen, x, y);
   }
}

static void set_pixel(screen_mode_t *screen, int x, int y, pixel_t colour) {
   if (x < g_x_min  || x > g_x_max || y < g_y_min || y > g_y_max) {
      return;
   }
   if (g_plotmode != PM_NORMAL) {
      pixel_t existing = screen->get_pixel(screen, x, y);
      switch (g_plotmode) {
      case PM_OR:
         colour |= existing;
         break;
      case PM_AND:
         colour &= existing;
         break;
      case PM_XOR:
         colour ^= existing;
         break;
      case PM_INVERT:
         colour = existing ^ 0xFF;
         break;
      }
   }
   screen->set_pixel(screen, x, y, colour);
}

static void draw_hline(screen_mode_t *screen, int x1, int x2, int y, pixel_t colour) {
   if (x1 > x2) {
      int tmp = x1;
      x1 = x2;
      x2 = tmp;
   }
   for (int x = x1; x <= x2; x++) {
      set_pixel(screen, x, y, colour);
   }
}

static void draw_line(screen_mode_t *screen, int x1, int y1, int x2, int y2, pixel_t colour) {
   int w = x2 - x1;
   int h = y2 - y1;
   int dx1 = 0, dy1 = 0, dx2 = 0, dy2 = 0;
   if (w < 0) {
      dx1 = -1;
   } else if (w > 0) {
      dx1 = 1;
   }
   if (h < 0) {
      dy1 = -1;
   } else if (h > 0) {
      dy1 = 1;
   }
   if (w < 0) {
      dx2 = -1;
   } else if (w > 0) {
      dx2 = 1;
   }
   int longest = abs(w);
   int shortest = abs(h);
   if (!(longest > shortest)) {
      longest = abs(h);
      shortest = abs(w);
      if (h < 0) dy2 = -1 ; else if (h > 0) dy2 = 1;
      dx2 = 0;
   }
   int numerator = longest >> 1 ;
   int x = x1;
   int y = y1;
   for (int i = 0; i <= longest; i++) {
      set_pixel(screen, x, y, colour);
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

static void flood_fill(screen_mode_t *screen, int x, int y, pixel_t fill_col, pixel_t ref_col, int c) {
#ifdef DEBUG_VDU
   int max = 0;
   printf("Flood fill @ %d,%d with col %"PRIx32"; initial pixel %"PRIx32"\r\n", x, y, fill_col, get_pixel(screen, x, y));
#endif
   if (c && (fill_col == ref_col)) {
      return;
   }
   if (((get_pixel(screen, x, y) == ref_col) ? !c : c)) {
      return;
   }
   flood_queue_x[0] = x;
   flood_queue_y[0] = y;
   flood_queue_rd = 0;
   flood_queue_wr = 1;
   while (flood_queue_rd != flood_queue_wr) {

      x = flood_queue_x[flood_queue_rd];
      y = flood_queue_y[flood_queue_rd];
      flood_queue_rd ++;
      flood_queue_rd &= FLOOD_QUEUE_SIZE - 1;

      if ((get_pixel(screen, x, y) == ref_col) ? !c : c) {
         continue;
      }

      int xl = x;
      int xr = x;
      while (xl > g_x_min && ((get_pixel(screen, xl - 1, y) == ref_col) ? c : !c)) {
         xl--;
      }
      while (xr < g_x_max && ((get_pixel(screen, xr + 1, y) == ref_col) ? c : !c)) {
         xr++;
      }
      for (x = xl; x <= xr; x++) {
         set_pixel(screen, x, y, fill_col);
         if (y > g_y_min && ((get_pixel(screen, x, y - 1) == ref_col) ? c : !c)) {
            flood_queue_x[flood_queue_wr] = x;
            flood_queue_y[flood_queue_wr] = y - 1;
            flood_queue_wr ++;
            flood_queue_wr &= FLOOD_QUEUE_SIZE - 1;
#ifdef DEBUG_VDU
            if (flood_queue_wr == flood_queue_rd) {
               printf("queue wrapped\r\n");
            }
#endif
         }
         if (y < g_y_max && ((get_pixel(screen, x, y + 1) == ref_col) ? c : !c)) {
            flood_queue_x[flood_queue_wr] = x;
            flood_queue_y[flood_queue_wr] = y + 1;
            flood_queue_wr ++;
            flood_queue_wr &= FLOOD_QUEUE_SIZE - 1;
#ifdef DEBUG_VDU
            if (flood_queue_wr == flood_queue_rd) {
               printf("queue wrapped\r\n");
            }
#endif
         }
      }
#ifdef DEBUG_VDU
      int size = (FLOOD_QUEUE_SIZE + flood_queue_wr - flood_queue_rd) & (FLOOD_QUEUE_SIZE - 1);
      if (size > max) {
         max = size;
      }
#endif
   }
#ifdef DEBUG_VDU
   printf("Max queue size = %d\r\n", max);
#endif
}

static void fill_area(screen_mode_t *screen, int x, int y, pixel_t colour, fill_t mode) {
   int x_left = x;
   int x_right = x;

   // printf("Plot (%d,%d), colour %d, mode %d\r\n", x, y, colour, mode);
   // printf("g_bg_col = %d, g_fg_col = %d\n\r", g_bg_col, g_fg_col);

   pixel_t fg_col = screen->get_colour(screen, fb_get_g_fg_col());
   pixel_t bg_col = screen->get_colour(screen, fb_get_g_bg_col());

   if (x < g_x_min  || x > g_x_max || y < g_y_min || y > g_y_max) {
      return;
   }

   switch(mode) {
   case HL_LR_NB:
      if (get_pixel(screen, x, y) != bg_col) {
         return;
      }
      while (get_pixel(screen, x_right, y) == bg_col && x_right < g_x_max) {
         x_right++;
      }
      while (get_pixel(screen, x_left, y) == bg_col && x_left > g_x_min) {
         x_left--;
      }
      draw_hline(screen, x_left, x_right, y, colour);
      break;

   case HL_RO_BG:
      if (get_pixel(screen, x, y) == bg_col) {
         return;
      }
      while (get_pixel(screen, x_right, y) != bg_col && x_right < g_x_max) {
         x_right++;
      }
      draw_hline(screen, x_left, x_right, y, colour);
      break;

   case HL_LR_FG:
      if (get_pixel(screen, x, y) == fg_col) {
         return;
      }
      while (get_pixel(screen, x_right, y) != fg_col && x_right < g_x_max) {
         x_right++;
      }
      while (get_pixel(screen, x_left, y) != fg_col && x_left > g_x_min) {
         x_left--;
      }
      draw_hline(screen, x_left, x_right, y, colour);
      break;

   case HL_RO_NF:
      if (get_pixel(screen, x, y) != fg_col) {
         return;
      }
      while (get_pixel(screen, x_right, y) == fg_col && x_right < g_x_max) {
         x_right++;
      }
      draw_hline(screen, x_left, x_right, y, colour);
      break;

   case AF_NONBG:
      flood_fill(screen, x, y, colour, bg_col, 1);
      break;

   case AF_TOFGD:
      flood_fill(screen, x, y, colour, fg_col, 0);
      break;

   default:
      printf( "Unknown fill mode %d\r\n", mode);
      break;
   }
}


static void draw_circle(screen_mode_t *screen, int xc, int yc, int r, pixel_t colour) {
   int x = 0;
   int y = r;
   int p = 3 - (2 * r);
   while (x < y) {
      set_pixel(screen, xc + x, yc + y, colour);
      set_pixel(screen, xc + x, yc - y, colour);
      set_pixel(screen, xc + y, yc + x, colour);
      set_pixel(screen, xc - y, yc + x, colour);
      if (x > 0) {
         set_pixel(screen, xc - x, yc + y, colour);
         set_pixel(screen, xc - x, yc - y, colour);
         set_pixel(screen, xc + y, yc - x, colour);
         set_pixel(screen, xc - y, yc - x, colour);
      }
      if (p < 0) {
         p += 4 * x + 6;
         x++;
      } else {
         p += 4 * (x - y) + 10;
         x++;
         y--;
      }
   }
   if (x == y) {
      set_pixel(screen, xc + x, yc + y, colour);
      set_pixel(screen, xc - x, yc + y, colour);
      set_pixel(screen, xc + x, yc - y, colour);
      set_pixel(screen, xc - x, yc - y, colour);
   }
}

static void fill_circle(screen_mode_t *screen, int xc, int yc, int r, pixel_t colour) {
   int x = 0;
   int y = r;
   int p = 3 - (2 * r);
   while (x < y) {
      draw_hline(screen, xc + y, xc - y, yc + x, colour);
      if (x > 0) {
         draw_hline(screen, xc + y, xc - y, yc - x, colour);
      }
      if (p < 0) {
         p += 4 * x + 6;
         x++;
      } else {
         draw_hline(screen, xc + x, xc - x, yc - y, colour);
         draw_hline(screen, xc + x, xc - x, yc + y, colour);
         p += 4 * (x - y) + 10;
         x++;
         y--;
      }
   }
   if (x == y) {
      draw_hline(screen, xc + x, xc - x, yc - y, colour);
      draw_hline(screen, xc + x, xc - x, yc + y, colour);
   }
}

static void draw_axis_aligned_ellipse(screen_mode_t *screen, int xc, int yc, int width, int height, pixel_t colour) {
   // Draw the ellipse
   int a2 = width * width;
   int b2 = height * height;
   int fa2 = 4 * a2, fb2 = 4 * b2;
   int x, y, sigma;
   /* First half */
   for (x = 0, y = height, sigma = 2 * b2 + a2 * (1 - 2 * height); b2 * x <= a2 * y; x++) {
      set_pixel(screen, xc + x, yc + y, colour);
      set_pixel(screen, xc + x, yc - y, colour);
      if (x > 0) {
         set_pixel(screen, xc - x, yc + y, colour);
         set_pixel(screen, xc - x, yc - y, colour);
      }
      if (sigma >= 0) {
         sigma += fa2 * (1 - y);
         y--;
      }
      sigma += b2 * ((4 * x) + 6);
   }
   /* Second half */
   for (x = width, y = 0, sigma = 2 * a2 + b2 * (1 - 2 * width); a2 * y <= b2 * x; y++) {
      set_pixel(screen, xc + x, yc + y, colour);
      set_pixel(screen, xc - x, yc + y, colour);
      if (y > 0) {
         set_pixel(screen, xc + x, yc - y, colour);
         set_pixel(screen, xc - x, yc - y, colour);
      }
      if (sigma >= 0) {
         sigma += fb2 * (1 - x);
         x--;
      }
      sigma += a2 * ((4 * y) + 6);
   }
}

static void fill_axis_aligned_ellipse(screen_mode_t *screen, int xc, int yc, int width, int height, pixel_t colour) {
   int a2 = width * width;
   // Fill the ellipse
   int b2 = height * height;
   int fa2 = 4 * a2, fb2 = 4 * b2;
   int x, y, sigma;
   /* First half */
   for (x = 0, y = height, sigma = 2 * b2 + a2 * (1 - 2 * height); b2 * x <= a2 * y; x++) {
      if (sigma >= 0) {
         draw_hline(screen, xc + x, xc - x, yc + y, colour);
         draw_hline(screen, xc + x, xc - x, yc - y, colour);
         sigma += fa2 * (1 - y);
         y--;
      }
      sigma += b2 * ((4 * x) + 6);
   }
   /* Second half */
   for (x = width, y = 0, sigma = 2 * a2 + b2 * (1 - 2 * width); a2 * y <= b2 * x; y++) {
      draw_hline(screen, xc + x, xc - x, yc + y, colour);
      if (y > 0) {
         draw_hline(screen, xc + x, xc - x, yc - y, colour);
      }
      if (sigma >= 0) {
         sigma += fb2 * (1 - x);
         x--;
      }
      sigma += a2 * ((4 * y) + 6);
   }
}

static void fill_bottom_flat_triangle(screen_mode_t *screen, int x1, int y1, int x2, int y2, int x3, int y3, pixel_t colour) {
   float invslope1 = ((float) (x2 - x1)) / ((float) (y1 - y2));
   float invslope2 = ((float) (x3 - x1)) / ((float) (y1 - y3));
   float curx1 = x1;
   float curx2 = x1;
   for (int scanlineY = y1; scanlineY >= y2; scanlineY--) {
      draw_hline(screen, (int)curx1, (int)curx2, scanlineY, colour);
      curx1 += invslope1;
      curx2 += invslope2;
   }
}

static void fill_top_flat_triangle(screen_mode_t *screen, int x1, int y1, int x2, int y2, int x3, int y3, pixel_t colour) {
   float invslope1 = ((float) (x3 - x1)) / ((float) (y1 - y3));
   float invslope2 = ((float) (x3 - x2)) / ((float) (y2 - y3));
   float curx1 = x3;
   float curx2 = x3;
   for (int scanlineY = y3; scanlineY < y1; scanlineY++) {
      draw_hline(screen, (int)curx1, (int)curx2, scanlineY, colour);
      curx1 -= invslope1;
      curx2 -= invslope2;
   }
}

// ==========================================================================
// Public methods - run at external resolution
// ==========================================================================

void fb_set_graphics_origin(int16_t x, int16_t y) {
   g_x_origin = x;
   g_y_origin = y;
}

void fb_set_graphics_plotmode(uint8_t plotmode) {
   g_plotmode = plotmode;
}

void fb_set_graphics_area(screen_mode_t *screen, int16_t x1, int16_t y1, int16_t x2, int16_t y2) {
   // Transform to screen coordinates
   x1 = (x1 + g_x_origin) >> screen->xeigfactor;
   y1 = (y1 + g_y_origin) >> screen->yeigfactor;
   x2 = (x2 + g_x_origin) >> screen->xeigfactor;
   y2 = (y2 + g_y_origin) >> screen->yeigfactor;
   // Reject illegal windows (this is what OS 1.20 does)
   if (x1 < 0 || x1 >= screen->width || y1 < 0 || y1 >= screen->height) {
      return;
   }
   if (x2 < 0 || x2 >= screen->width || y2 < 0 || y2 >= screen->height) {
      return;
   }
   if (x1 >= x2 || y1 >= y2) {
      return;
   }
   // Update the window
   g_x_min = x1;
   g_y_min = y1;
   g_x_max = x2;
   g_y_max = y2;
}

void fb_set_pixel(screen_mode_t *screen, int x, int y, pixel_t colour) {
   // Transform to screen coordinates
   x = (x + g_x_origin) >> screen->xeigfactor;
   y = (y + g_y_origin) >> screen->yeigfactor;
   // Set the pixel
   set_pixel(screen, x, y, colour);
}

void fb_clear_graphics_area(screen_mode_t *screen, pixel_t colour) {
   // g_x_min/max and g_y_min/max are in screen cordinates
   for (int y = g_y_min; y <= g_y_max; y++) {
      for (int x = g_x_min; x <= g_x_max; x++) {
         set_pixel(screen, x, y, colour);
      }
   }
}

// Implementation of Bresenham's line drawing algorithm from here:
// http://tech-algorithm.com/articles/drawing-line-using-bresenham-algorithm/
void fb_draw_line(screen_mode_t *screen, int x1, int y1, int x2, int y2, pixel_t colour) {
   // Transform to screen coordinates
   x1 = (x1 + g_x_origin) >> screen->xeigfactor;
   y1 = (y1 + g_y_origin) >> screen->yeigfactor;
   x2 = (x2 + g_x_origin) >> screen->xeigfactor;
   y2 = (y2 + g_y_origin) >> screen->yeigfactor;
   // Draw the line
   draw_line(screen, x1, y1, x2, y2, colour);
}

void fb_fill_triangle(screen_mode_t *screen, int x1, int y1, int x2, int y2, int x3, int y3, pixel_t colour) {
   int tmp;
   // Transform to screen coordinates
   x1 = (x1 + g_x_origin) >> screen->xeigfactor;
   y1 = (y1 + g_y_origin) >> screen->yeigfactor;
   x2 = (x2 + g_x_origin) >> screen->xeigfactor;
   y2 = (y2 + g_y_origin) >> screen->yeigfactor;
   x3 = (x3 + g_x_origin) >> screen->xeigfactor;
   y3 = (y3 + g_y_origin) >> screen->yeigfactor;

#ifdef USE_V3D
   if (screen->bpp > 8) {
      // Flip y axis
      y1 = screen->height - 1 - y1;
      y2 = screen->height - 1 - y2;
      y3 = screen->height - 1 - y3;
      // Draw the triangle
      v3d_draw_triangle(screen, x1, y1, x2, y2, x3, y3, colour);
   } else {
#endif
      // Use Standard Triangle Fill
      // http://www.sunshine2k.de/coding/java/TriangleRasterization/TriangleRasterization.html
      // sort the three vertices by y-coordinate ascending so v1 is the topmost vertice
      if (y2 > y1) {
         tmp = x1; x1 = x2; x2 = tmp;
         tmp = y1; y1 = y2; y2 = tmp;
      }
      if (y3 > y1) {
         tmp = x1; x1 = x3; x3 = tmp;
         tmp = y1; y1 = y3; y3 = tmp;
      }
      if (y3 > y2) {
         tmp = x2; x2 = x3; x3 = tmp;
         tmp = y2; y2 = y3; y3 = tmp;
      }
      // here we know that y1 >= y2 >= y3
      if (y2 == y3) {
         // trivial case of bottom-flat triangle
         fill_bottom_flat_triangle(screen, x1, y1, x2, y2, x3, y3, colour);
      } else if (y1 == y2) {
         // trivial case of top-flat triangle
         fill_top_flat_triangle(screen, x1, y1, x2, y2, x3, y3, colour);
      } else {
         // general case - split the triangle in a topflat and bottom-flat one
         int x4 = (int)(x1 + ((float)(y1 - y2) / (float)(y1 - y3)) * (x3 - x1));
         int y4 = y2;
         fill_bottom_flat_triangle(screen, x1, y1, x2, y2, x4, y4, colour);
         fill_top_flat_triangle(screen, x2, y2, x4, y4, x3, y3, colour);
      }
#ifdef USE_V3D
   }
#endif
}

void fb_draw_triangle(screen_mode_t *screen, int x1, int y1, int x2, int y2, int x3, int y3, pixel_t colour) {
   // Transform to screen coordinates
   x1 = (x1 + g_x_origin) >> screen->xeigfactor;
   y1 = (y1 + g_y_origin) >> screen->yeigfactor;
   x2 = (x2 + g_x_origin) >> screen->xeigfactor;
   y2 = (y2 + g_y_origin) >> screen->yeigfactor;
   x3 = (x3 + g_x_origin) >> screen->xeigfactor;
   y3 = (y3 + g_y_origin) >> screen->yeigfactor;
   // Draw the triangle
   draw_line(screen, x1, y1, x2, y2, colour);
   draw_line(screen, x2, y2, x3, y3, colour);
   draw_line(screen, x3, y3, x1, y1, colour);
}

void fb_draw_circle(screen_mode_t *screen, int xc, int yc, int xr, int yr, pixel_t colour) {
   int r = calc_radius(xc, yc, xr, yr);
   // Transform to screen coordinates
   xc = (xc + g_x_origin) >> screen->xeigfactor;
   yc = (yc + g_y_origin) >> screen->yeigfactor;
   // Draw the circle
   if (screen->xeigfactor == screen->yeigfactor) {
      // Square pixels
      r >>= screen->xeigfactor;
      draw_circle(screen, xc, yc, r, colour);
   } else {
      int width  = r >> screen->xeigfactor;
      int height = r >> screen->yeigfactor;
      // Rectangular pixels
      draw_axis_aligned_ellipse(screen, xc, yc, width, height, colour);
   }
}

void fb_fill_circle(screen_mode_t *screen, int xc, int yc, int xr, int yr, pixel_t colour) {
   int r = calc_radius(xc, yc, xr, yr);
   // Transform to screen coordinates
   xc = (xc + g_x_origin) >> screen->xeigfactor;
   yc = (yc + g_y_origin) >> screen->yeigfactor;
   // Fill the circle
   if (screen->xeigfactor == screen->yeigfactor) {
      // Square pixels
      r >>= screen->xeigfactor;
      fill_circle(screen, xc, yc, r, colour);
   } else {
      int width  = r >> screen->xeigfactor;
      int height = r >> screen->yeigfactor;
      // Rectangular pixels
      fill_axis_aligned_ellipse(screen, xc, yc, width, height, colour);
   }
}

void fb_draw_rectangle(screen_mode_t *screen, int x1, int y1, int x2, int y2, pixel_t colour) {
   // Transform to screen coordinates
   x1 = (x1 + g_x_origin) >> screen->xeigfactor;
   y1 = (y1 + g_y_origin) >> screen->yeigfactor;
   x2 = (x2 + g_x_origin) >> screen->xeigfactor;
   y2 = (y2 + g_y_origin) >> screen->yeigfactor;
   // Draw the rectangle
   draw_line(screen, x1, y1, x2, y1, colour);
   draw_line(screen, x2, y1, x2, y2, colour);
   draw_line(screen, x2, y2, x1, y2, colour);
   draw_line(screen, x1, y2, x1, y1, colour);
}

void fb_fill_rectangle(screen_mode_t *screen, int x1, int y1, int x2, int y2, pixel_t colour) {
   // Transform to screen coordinates
   x1 = (x1 + g_x_origin) >> screen->xeigfactor;
   y1 = (y1 + g_y_origin) >> screen->yeigfactor;
   x2 = (x2 + g_x_origin) >> screen->xeigfactor;
   y2 = (y2 + g_y_origin) >> screen->yeigfactor;
   // Fill the rectangle
   int y;
   for (y = y1; y <= y2; y++) {
      draw_hline(screen, x1, x2, y, colour);
   }
}

void fb_draw_parallelogram(screen_mode_t *screen, int x1, int y1, int x2, int y2, int x3, int y3, pixel_t colour) {
   // Transform to screen coordinates
   x1 = (x1 + g_x_origin) >> screen->xeigfactor;
   y1 = (y1 + g_y_origin) >> screen->yeigfactor;
   x2 = (x2 + g_x_origin) >> screen->xeigfactor;
   y2 = (y2 + g_y_origin) >> screen->yeigfactor;
   x3 = (x3 + g_x_origin) >> screen->xeigfactor;
   y3 = (y3 + g_y_origin) >> screen->yeigfactor;
   int x4 = x3 - x2 + x1;
   int y4 = y3 - y2 + y1;
   // Draw the parallelogram
   draw_line(screen, x1, y1, x2, y2, colour);
   draw_line(screen, x2, y2, x3, y3, colour);
   draw_line(screen, x3, y3, x4, y4, colour);
   draw_line(screen, x4, y4, x1, y1, colour);
}

void fb_fill_parallelogram(screen_mode_t *screen, int x1, int y1, int x2, int y2, int x3, int y3, pixel_t colour) {
   // Transform to screen coordinates
   x1 = (x1 + g_x_origin) >> screen->xeigfactor;
   y1 = (y1 + g_y_origin) >> screen->yeigfactor;
   x2 = (x2 + g_x_origin) >> screen->xeigfactor;
   y2 = (y2 + g_y_origin) >> screen->yeigfactor;
   x3 = (x3 + g_x_origin) >> screen->xeigfactor;
   y3 = (y3 + g_y_origin) >> screen->yeigfactor;
   int x4 = x3 - x2 + x1;
   int y4 = y3 - y2 + y1;
   // Fill the parallelogram
   fb_fill_triangle(screen, x1, y1, x2, y2, x3, y3, colour);
   fb_fill_triangle(screen, x1, y1, x4, y4, x3, y3, colour);
}

void fb_draw_ellipse(screen_mode_t *screen, int xc, int yc, int width, int height, pixel_t colour) {
   // Transform to screen coordinates
   xc = (xc + g_x_origin) >> screen->xeigfactor;
   yc = (yc + g_y_origin) >> screen->yeigfactor;
   width =         width  >> screen->xeigfactor;
   height =       height  >> screen->yeigfactor;
   // Draw the ellipse
   draw_axis_aligned_ellipse(screen, xc, yc, width, height, colour);
}

void fb_fill_ellipse(screen_mode_t *screen, int xc, int yc, int width, int height, pixel_t colour) {
   // Transform to screen coordinates
   xc = (xc + g_x_origin) >> screen->xeigfactor;
   yc = (yc + g_y_origin) >> screen->yeigfactor;
   width =         width  >> screen->xeigfactor;
   height =       height  >> screen->yeigfactor;
   // Fill the ellipse
   fill_axis_aligned_ellipse(screen, xc, yc, width, height, colour);
}


/*   Modes:
 * HL_LR_NB: horizontal line fill (left & right) to non-background - done
 * HL_RO_BG: Horizontal line fill (right only) to background - done
 * HL_LR_FG: Horizontal line fill (left & right) to foreground
 * HL_RO_NF: Horizontal line fill (right only) to non-foreground - done
 * AF_NONBG: Flood (area fill) to non-background
 * AF_TOFGD: Flood (area fill) to foreground
 */

void fb_fill_area(screen_mode_t *screen, int x, int y, pixel_t colour, fill_t mode) {
   // Transform to screen coordinates
   x = (x + g_x_origin) >> screen->xeigfactor;
   y = (y + g_y_origin) >> screen->yeigfactor;
   // Fill the area
   fill_area(screen, x, y, colour, mode);
}
