// framebuffer.h

#ifndef FRAMEBUFFER_H
#define FRAMEBUFFER_H

void fb_initialize();

void fb_writec(int c);

void fb_writes(char *string);

void fb_draw_line(int x,int y,int x2, int y2, unsigned int color);

#endif
