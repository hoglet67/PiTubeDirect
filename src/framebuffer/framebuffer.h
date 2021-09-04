#ifndef FRAMEBUFFER_H
#define FRAMEBUFFER_H

#include <inttypes.h>

#include "screen_modes.h"

// #define DEBUG_VDU

#define FALSE 0
#define TRUE  1

typedef enum {
   VDU_BEEB = (1 << 0),
   VDU_PI   = (1 << 1)
} vdu_device_t;

typedef enum {
   V_GWLCOL          = 128, // &80 Graphics Window – Lefthand Column (ic)
   V_GWBROW          = 129, // &81 Graphics Window – Bottom Row  (ic)
   V_GWRCOL          = 130, // &82 Graphics Window – Righthand Column  (ic)
   V_GWTROW          = 131, // &83 Graphics Window – Top Row (ic)
   V_TWLCOL          = 132, // &84 Text Window – Lefthand Column
   V_TWBROW          = 133, // &85 Text Window – Bottom Row
   V_TWRCOL          = 134, // &86 Text Window – Righthand Column
   V_TWTROW          = 135, // &87 Text Window – Top Row
   V_ORGX            = 136, // &88 X coord of graphics Origin  (ec)
   V_ORGY            = 137, // &89 Y coord of graphics Origin  (ec)
   V_GCSX            = 138, // &8A Graphics Cursor X coord (ec)
   V_GCSY            = 139, // &8B Graphics Cursor Y coord (ec)
   V_OLDERCSX        = 140, // &8C Oldest gr. Cursor X coord (ic)
   V_OLDERCSY        = 141, // &8D Oldest gr. Cursor Y coord (ic)
   V_OLDCSX          = 142, // &8E Previous gr. Cursor X coord (ic)
   V_OLDCSY          = 143, // &8F Previous gr. Cursor Y coord (ic)
   V_GCSIX           = 144, // &90 Graphics Cursor X coord (ic)
   V_GCSIY           = 145, // &91 Graphics Cursor Y coord (ic)
   V_NEWPTX          = 146, // &92 New point X coord (ic)
   V_NEWPTY          = 147, // &93 New point Y coord (ic)
   V_SCREENSTART     = 148, // &94 As used by VDU drivers
   V_DISPLAYSTART    = 149, // &95 As used by display hardware
   V_TOTALSCREENSIZE = 150, // &96 Memory allocated to screen
   V_GPLFMD          = 151, // &97 GCOL action for foreground col
   V_GPLBMD          = 152, // &98 GCOL action for background col
   V_GFCOL           = 153, // &99 Graphics foreground col
   V_GBCOL           = 154, // &9A Graphics background col
   V_TFORECOL        = 155, // &9B Text foreground col
   V_TBACKCOL        = 156, // &9C Text background col
   V_GFTINT          = 157, // &9D Graphics foreground tint
   V_GBTINT          = 158, // &9E Graphics background tint
   V_TFTINT          = 159, // &9F Text foreground tint
   V_TBTINT          = 160, // &A0 Text background tint
   V_MAXMODE         = 161, // &A1 Highest built-in numbered mode known to kernel
   V_GCHARSIZEX      = 162, // &A2 X size of VDU5 chars (pixels)
   V_GCHARSIZEY      = 163, // &A3 Y size of VDU5 chars (pixels)
   V_GCHARSPACEX     = 164, // &A4 X spacing of VDU5 chars (pixels)
   V_GCHARSPACEY     = 165, // &A5 Y spacing of VDU5 chars (pixels)
   V_HLINEADDR       = 166, // &A6 Address of horizontal line-draw routine
   V_TCHARSIZEX      = 167, // &A7 X size of VDU4 chars (pixels)
   V_TCHARSIZEY      = 168, // &A8 Y size of VDU4 chars (pixels)
   V_TCHARSPACEX     = 169, // &A9 X spacing of VDU4 chars (pixels)
   V_TCHARSPACEY     = 170, // &AA Y spacing of VDU4 chars (pixels)
   V_GCOLORAEORADDR  = 171, // &AB Addr of colour blocks for current GCOLs
   V_VIDCCLOCKSPEED  = 172, // &AC VIDC clock speed in kHz3
   V_LEFT            = 174, // &AE border size1
   V_BOTTOM          = 175, // &AF border size1
   V_RIGHT           = 176, // &B0 border size1
   V_TOP             = 177, // &B1 border size1
   V_CURRENT         = 192, // &C0 GraphicsV driver number2
   V_WINDOWWIDTH     = 256, // &100 Width of text window in chars
   V_WINDOWHEIGHT    = 257  // &101 Height of text window in chars
} vdu_variable_t;

void fb_initialize();

void fb_destroy();

void fb_custom_mode(int16_t x_pixels, int16_t y_pixels, unsigned int n_colours);

void fb_writec_buffered(char c);

void fb_process_vdu_queue();

void fb_writec(char c);

void fb_writes(const char *string);

uint32_t fb_get_address();

int fb_get_edit_cursor_x();

int fb_get_edit_cursor_y();

int fb_get_edit_cursor_char();

int fb_get_text_cursor_x();

int fb_get_text_cursor_y();

int fb_get_text_cursor_char();

uint8_t fb_get_g_bg_col();

uint8_t fb_get_g_fg_col();

void fb_wait_for_vsync();

screen_mode_t *fb_get_current_screen_mode();

void fb_set_vdu_device(vdu_device_t device);

void fb_add_swi_handlers();

void fb_remove_swi_handlers();

int32_t fb_read_vdu_variable(vdu_variable_t v);

void fb_set_flash_mark_time(uint8_t time);

void fb_set_flash_space_time(uint8_t time);

uint8_t fb_get_flash_mark_time();

uint8_t fb_get_flash_space_time();

int fb_point(int16_t x, int16_t y, pixel_t *colour);

void fb_set_g_fg_col(uint8_t action, colour_index_t gcol);

void fb_set_g_bg_col(uint8_t action, colour_index_t gcol);

void fb_set_c_fg_col(colour_index_t gcol);

void fb_set_c_bg_col(colour_index_t gcol);

#endif
