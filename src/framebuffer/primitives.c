#include <stdio.h>
#include <stdlib.h>
#include "primitives.h"
#include "framebuffer.h"
#include "v3d.h"

static uint8_t g_plotmode;

static int16_t g_x_origin;
static int16_t g_y_origin;

static int fill_x_pos_last1;
static int fill_y_pos_last1;
static int fill_x_pos_last2;
static int fill_y_pos_last2;

void fb_set_graphics_origin(int16_t x, int16_t y) {
   g_x_origin = x;
   g_y_origin = y;
}

void fb_set_graphics_plotmode(uint8_t plotmode) {
   g_plotmode = plotmode;
}

// TODO: (Maybe... need to consider the implications of working at lower resolution)
// Push the translation to the graphics origin into each primitive
// Push the range test into the screen mode

static pixel_t fb_getpixel(screen_mode_t *screen, int x, int y) {
   x = ((x + g_x_origin) * screen->width)  / BBC_X_RESOLUTION;
   y = ((y + g_y_origin) * screen->height) / BBC_Y_RESOLUTION;
   if (x < 0  || x > screen->width - 1 || y < 0 || y > screen->height - 1) {
      // Return the graphics background colour if off the screen
      return screen->get_colour(screen, fb_get_g_bg_col());
   } else {
      return screen->get_pixel(screen, x, y);
   }
}


void fb_setpixel(screen_mode_t *screen, int x, int y, pixel_t colour) {
   x = ((x + g_x_origin) * screen->width)  / BBC_X_RESOLUTION;
   y = ((y + g_y_origin) * screen->height) / BBC_Y_RESOLUTION;
   if (x < 0  || x > screen->width - 1) {
      return;
   }
   if (y < 0 || y > screen->height - 1) {
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


// Implementation of Bresenham's line drawing algorithm from here:
// http://tech-algorithm.com/articles/drawing-line-using-bresenham-algorithm/
void fb_draw_line(screen_mode_t *screen, int x,int y,int x2, int y2, pixel_t colour) {
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
      fb_setpixel(screen, x, y, colour);
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

void fb_fill_triangle(screen_mode_t *screen, int x, int y, int x2, int y2, int x3, int y3, pixel_t colour) {
   x = ((x + g_x_origin) * screen->width)  / BBC_X_RESOLUTION;
   y = ((y + g_y_origin) * screen->height) / BBC_Y_RESOLUTION;
   x2 = ((x2 + g_x_origin) * screen->width)  / BBC_X_RESOLUTION;
   y2 = ((y2 + g_y_origin) * screen->height) / BBC_Y_RESOLUTION;
   x3 = ((x3 + g_x_origin) * screen->width)  / BBC_X_RESOLUTION;
   y3 = ((y3 + g_y_origin) * screen->height) / BBC_Y_RESOLUTION;
   // Flip y axis
   y = screen->height - 1 - y;
   y2 = screen->height - 1 - y2;
   y3 = screen->height - 1 - y3;
   v3d_draw_triangle(screen, x, y, x2, y2, x3, y3, colour);
}

void fb_draw_triangle(screen_mode_t *screen, int x1, int y1, int x2, int y2, int x3, int y3, pixel_t colour) {
   fb_draw_line(screen, x1, y1, x2, y2, colour);
   fb_draw_line(screen, x2, y2, x3, y3, colour);
   fb_draw_line(screen, x3, y3, x1, y1, colour);
}

void fb_draw_circle(screen_mode_t *screen, int xc, int yc, int r, pixel_t colour) {
   int x=0;
   int y=r;
   int p=3-(2*r);

   fb_setpixel(screen, xc+x,yc-y,colour);

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

         fb_setpixel(screen, xc+x,yc-y,colour);
         fb_setpixel(screen, xc-x,yc-y,colour);
         fb_setpixel(screen, xc+x,yc+y,colour);
         fb_setpixel(screen, xc-x,yc+y,colour);
         fb_setpixel(screen, xc+y,yc-x,colour);
         fb_setpixel(screen, xc-y,yc-x,colour);
         fb_setpixel(screen, xc+y,yc+x,colour);
         fb_setpixel(screen, xc-y,yc+x,colour);
      }
}

void fb_fill_circle(screen_mode_t *screen, int xc, int yc, int r, pixel_t colour) {
   int x=0;
   int y=r;
   int p=3-(2*r);

   fb_setpixel(screen, xc+x,yc-y,colour);

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

         fb_draw_line(screen, xc+x,yc-y,xc-x,yc-y,colour);
         fb_draw_line(screen, xc+x,yc+y,xc-x,yc+y,colour);
         fb_draw_line(screen, xc+y,yc-x,xc-y,yc-x,colour);
         fb_draw_line(screen, xc+y,yc+x,xc-y,yc+x,colour);
      }
}
/* Bad rectangle due to triangle bug
   void fb_fill_rectangle(int x1, int y1, int x2, int y2, pixel_t colour) {
   fb_fill_triangle(x1, y1, x1, y2, x2, y2, colour);
   fb_fill_triangle(x1, y1, x2, y1, x2, y2, colour);
   }
*/

void fb_fill_rectangle(screen_mode_t *screen, int x1, int y1, int x2, int y2, pixel_t colour) {
   int y;
   for (y = y1; y <= y2; y++) {
      fb_draw_line(screen, x1, y, x2, y, colour);
   }
}

void fb_draw_rectangle(screen_mode_t *screen, int x1, int y1, int x2, int y2, pixel_t colour) {
   fb_draw_line(screen, x1, y1, x2, y1, colour);
   fb_draw_line(screen, x2, y1, x2, y2, colour);
   fb_draw_line(screen, x2, y2, x1, y2, colour);
   fb_draw_line(screen, x1, y2, x1, y1, colour);
}

void fb_fill_parallelogram(screen_mode_t *screen, int x1, int y1, int x2, int y2, int x3, int y3, pixel_t colour) {
   int x4 = x3 - x2 + x1;
   int y4 = y3 - y2 + y1;
   fb_fill_triangle(screen, x1, y1, x2, y2, x3, y3, colour);
   fb_fill_triangle(screen, x1, y1, x4, y4, x3, y3, colour);
}

void fb_draw_parallelogram(screen_mode_t *screen, int x1, int y1, int x2, int y2, int x3, int y3, pixel_t colour) {
   int x4 = x3 - x2 + x1;
   int y4 = y3 - y2 + y1;
   fb_draw_line(screen, x1, y1, x2, y2, colour);
   fb_draw_line(screen, x2, y2, x3, y3, colour);
   fb_draw_line(screen, x3, y3, x4, y4, colour);
   fb_draw_line(screen, x4, y4, x1, y1, colour);
}

void fb_draw_ellipse(screen_mode_t *screen, int xc, int yc, int width, int height, pixel_t colour) {
   int a2 = width * width;
   int b2 = height * height;
   int fa2 = 4 * a2, fb2 = 4 * b2;
   int x, y, sigma;

   /* first half */
   for (x = 0, y = height, sigma = 2*b2+a2*(1-2*height); b2*x <= a2*y; x++)
      {
         fb_setpixel(screen, xc + x, yc + y, colour);
         fb_setpixel(screen, xc - x, yc + y, colour);
         fb_setpixel(screen, xc + x, yc - y, colour);
         fb_setpixel(screen, xc - x, yc - y, colour);
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
         fb_setpixel(screen, xc + x, yc + y, colour);
         fb_setpixel(screen, xc - x, yc + y, colour);
         fb_setpixel(screen, xc + x, yc - y, colour);
         fb_setpixel(screen, xc - x, yc - y, colour);
         if (sigma >= 0)
            {
               sigma += fb2 * (1 - x);
               x--;
            }
         sigma += a2 * ((4 * y) + 6);
      }
}

void fb_fill_ellipse(screen_mode_t *screen, int xc, int yc, int width, int height, pixel_t colour) {
   int a2 = width * width;
   int b2 = height * height;
   int fa2 = 4 * a2, fb2 = 4 * b2;
   int x, y, sigma;

   /* first half */
   for (x = 0, y = height, sigma = 2*b2+a2*(1-2*height); b2*x <= a2*y; x++)
      {
         fb_draw_line(screen, xc + x, yc + y, xc - x, yc + y, colour);
         fb_draw_line(screen, xc + x, yc - y, xc - x, yc - y, colour);
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
         fb_draw_line(screen, xc + x, yc + y, xc - x, yc + y, colour);
         fb_draw_line(screen, xc + x, yc - y, xc - x, yc - y, colour);
         if (sigma >= 0)
            {
               sigma += fb2 * (1 - x);
               x--;
            }
         sigma += a2 * ((4 * y) + 6);
      }
}

void fb_fill_area(screen_mode_t *screen, int x, int y, pixel_t colour, fill_t mode) {

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

   pixel_t fg_col = screen->get_colour(screen, fb_get_g_fg_col());
   pixel_t bg_col = screen->get_colour(screen, fb_get_g_bg_col());

   switch(mode) {
   case HL_LR_NB:
      while (! stop) {
         if (fb_getpixel(screen, x_right,y) == bg_col && x_right <= BBC_X_RESOLUTION) {
            x_right += BBC_X_RESOLUTION/screen->width;   // speeds up but might fail if not integer
         } else {
            stop = 1;
         }
      }
      stop = 0;
      x = save_x - 1;
      while (! stop) {
         if (fb_getpixel(screen, x_left,y) == bg_col && x_left >= 0) {
            x_left -= BBC_X_RESOLUTION/screen->width;    // speeds up but might fail if not integer
         } else {
            stop = 1;
         }
      }
      break;

   case HL_RO_BG:
      while (! stop) {
         if (fb_getpixel(screen, x_right,y) != bg_col && x_right <= BBC_X_RESOLUTION) {
            x_right += BBC_X_RESOLUTION/screen->width;   // speeds up but might fail if not integer
         } else {
            stop = 1;
         }
      }
      break;

   case HL_LR_FG:
      while (! stop) {
         if (fb_getpixel(screen, x_right,y) != fg_col && x_right <= BBC_X_RESOLUTION) {
            x_right += BBC_X_RESOLUTION/screen->width;   // speeds up but might fail if not integer
         } else {
            stop = 1;
         }
      }
      stop = 0;
      x = save_x - 1;
      while (! stop) {
         if (fb_getpixel(screen, x_left,y) != fg_col && x_left >= 0) {
            x_left -= BBC_X_RESOLUTION/screen->width;    // speeds up but might fail if not integer
         } else {
            stop = 1;
         }
      }
      break;


   case HL_RO_NF:
      while (! stop) {
         if (fb_getpixel(screen, x_right,y) != fg_col && x_right <= BBC_X_RESOLUTION) {
            x_right += BBC_X_RESOLUTION/screen->width;   // speeds up but might fail if not integer
         } else {
            stop = 1;
         }
      }
      break;

   case AF_NONBG:
      // going up
      while (! stop) {
         if (fb_getpixel(screen, x,y) == bg_col && y <= BBC_Y_RESOLUTION) {
            fb_fill_area(screen, x, y, colour, HL_LR_NB);
            // As the BBC_Y_RESOLUTION is not a multiple of screen->height we have to increment
            // y until the physical y-coordinate increases. If we don't do that and simply increment
            // y by 2 then at some point the physical y-coordinate is the same as the previous drawn
            // line and the floodings stops. This also speeds up drawing because each physical line
            // is only drawn once.
            real_y = ((y + g_y_origin) * screen->height) / BBC_Y_RESOLUTION;
            while(((y + g_y_origin) * screen->height) / BBC_Y_RESOLUTION == real_y) {
               y++;
            }
            x = (fill_x_pos_last1 + fill_x_pos_last2) / 2;
         } else {
            stop = 1;
         }
      }
      // going down
      stop = 0;
      x = save_x;
      y = save_y - BBC_Y_RESOLUTION/screen->height;
      while (! stop) {
         if (fb_getpixel(screen, x,y) == bg_col && y >= 0) {
            fb_fill_area(screen, x, y, colour, HL_LR_NB);
            real_y = ((y + g_y_origin) * screen->height) / BBC_Y_RESOLUTION;
            while(((y + g_y_origin) * screen->height) / BBC_Y_RESOLUTION == real_y) {
               y--;
            }
            x = (fill_x_pos_last1 + fill_x_pos_last2) / 2;
         } else {
            stop = 1;
         }
      }
      return; // prevents any additional line drawing

   case AF_TOFGD:
      // going up
      while (! stop) {
         if (fb_getpixel(screen, x,y) != fg_col && y <= BBC_Y_RESOLUTION) {
            fb_fill_area(screen, x, y, colour, HL_LR_FG);
            real_y = ((y + g_y_origin) * screen->height) / BBC_Y_RESOLUTION;
            while(((y + g_y_origin) * screen->height) / BBC_Y_RESOLUTION == real_y) {
               y++;
            }
            x = (fill_x_pos_last1 + fill_x_pos_last2) / 2;
         } else {
            stop = 1;
         }
      }
      // going down
      stop = 0;
      x = save_x;
      y = save_y - BBC_Y_RESOLUTION/screen->height;
      while (! stop) {
         if (fb_getpixel(screen, x,y) != fg_col && y >= 0) {
            fb_fill_area(screen, x, y, colour, HL_LR_NB);
            real_y = ((y + g_y_origin) * screen->height) / BBC_Y_RESOLUTION;
            while(((y + g_y_origin) * screen->height) / BBC_Y_RESOLUTION == real_y) {
               y--;
            }
            x = (fill_x_pos_last1 + fill_x_pos_last2) / 2;
         } else {
            stop = 1;
         }
      }
      return; // prevents any additional line drawing

   default:
      printf( "Unknown fill mode %d\r\n", mode);
      break;
   }

   fb_draw_line(screen, x_left, y_left, x_right, y_right, colour);
   fill_x_pos_last1 = x_left;
   fill_y_pos_last1 = y_left;
   fill_x_pos_last2 = x_right;
   fill_y_pos_last2 = y_right;
}
