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

int16_t fb_get_g_bg_col();

int16_t fb_get_g_fg_col();

#endif
