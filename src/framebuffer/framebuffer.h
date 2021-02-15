#ifndef FRAMEBUFFER_H
#define FRAMEBUFFER_H

#include <inttypes.h>

// #define DEBUG_VDU

#define FALSE 0
#define TRUE  1

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

void fb_wait_for_vsync();

int fb_get_current_screen_mode();

#endif
