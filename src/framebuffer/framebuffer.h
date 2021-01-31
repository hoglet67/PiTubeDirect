#ifndef FRAMEBUFFER_H
#define FRAMEBUFFER_H

// #define DEBUG_VDU

void fb_initialize();

void fb_writec_buffered(char c);

void fb_process_vdu_queue();

void fb_writec(char c);

void fb_writes(char *string);

uint32_t fb_get_address();

int fb_get_edit_cursor_x();

int fb_get_edit_cursor_y();

int fb_get_edit_cursor_char();

uint8_t fb_get_g_bg_col();

uint8_t fb_get_g_fg_col();

// TODO: The graphics cursor probably should live in primitives
// (in fact, possibly the whole of the vdu25 should live there)

int16_t fb_get_g_cursor_x();

int16_t fb_get_g_cursor_y();

void fb_set_g_cursor(int16_t x, int16_t y);

#endif
