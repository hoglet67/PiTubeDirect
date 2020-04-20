// framebuffer.h

#ifndef FRAMEBUFFER_H
#define FRAMEBUFFER_H

void fb_initialize();

void fb_writec_buffered(char c);

void fb_writec(char c);

void fb_writes(char *string);

void fb_putpixel(int x, int y, unsigned int color);

void fb_setpixel(int x, int y, unsigned int color);

unsigned int fb_getpixel(int x, int y);

void fb_draw_line(int x, int y, int x2, int y2, unsigned int color);

void fb_fill_triangle(int x, int y, int x2, int y2, int x3, int y3, unsigned int color);

void fb_draw_triangle(int x, int y, int x2, int y2, int x3, int y3, unsigned int color);

void fb_draw_circle(int xc, int yc, int r, unsigned int colour);

void fb_fill_circle(int xc, int yc, int r, unsigned int colour);

void fb_fill_rectangle(int x1, int y1, int x2, int y2, unsigned int colour);

void fb_draw_rectangle(int x1, int y1, int x2, int y2, unsigned int colour);

void fb_fill_parallelogram(int x1, int y1, int x2, int y2, int x3, int y3, unsigned int colour);

void fb_draw_parallelogram(int x1, int y1, int x2, int y2, int x3, int y3, unsigned int colour);

void fb_draw_ellipse(int xc, int yc, int width, int height, unsigned int colour);

void fb_fill_ellipse(int xc, int yc, int width, int height, unsigned int colour);

void fb_fill_area(int x, int y, unsigned int colour, unsigned int mode);

void do_vdu23(void);

void gr_draw_character(int c, int g_x_pos, int g_y_pos, int colour);

void init_colour_table(void);

uint32_t fb_get_address();
uint32_t fb_get_width();
uint32_t fb_get_height();
uint32_t fb_get_bpp32();

void fb_process_vdu_queue();

int fb_get_cursor_x();
int fb_get_cursor_y();
int fb_get_edit_cursor_x();
int fb_get_edit_cursor_y();
int fb_get_edit_cursor_char();

#endif
