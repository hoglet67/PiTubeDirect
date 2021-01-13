#ifndef FRAMEBUFFER_H
#define FRAMEBUFFER_H

// #define DEBUG_VDU

typedef enum {
   HL_LR_NB = 1, // Horizontal line fill (left & right) to non-background
   HL_RO_BG = 2, // Horizontal line fill (right only) to background
   HL_LR_FG = 3, // Horizontal line fill (left & right) to foreground
   HL_RO_NF = 4, // Horizontal line fill (right only) to non-foreground
   AF_NONBG = 5, // Flood (area fill) to non-background
   AF_TOFGD = 6  // Flood (area fill) to foreground
} fill_t;

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
