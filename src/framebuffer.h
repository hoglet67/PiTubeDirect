// framebuffer.h

#ifndef FRAMEBUFFER_H
#define FRAMEBUFFER_H

void fb_initialize();

void fb_writec(int c);

void fb_writes(char *string);

void fb_putpixel(int x, int y, unsigned int color);

void fb_draw_line(int x, int y, int x2, int y2, unsigned int color);

void fb_fill_triangle(int x, int y, int x2, int y2, int x3, int y3, unsigned int color);

uint32_t fb_get_address();

uint32_t fb_get_width();

uint32_t fb_get_height();

uint32_t fb_get_mode();

#endif
