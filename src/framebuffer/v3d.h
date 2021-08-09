#ifndef V3D_H
#define V3D_H

#include "screen_modes.h"

extern int v3d_initialize(screen_mode_t *screen);

extern int v3d_close(screen_mode_t *screen);

extern void v3d_test(screen_mode_t *screen);

extern void v3d_draw_triangle(screen_mode_t *screen, int x1, int y1, int x2, int y2, int x3, int y3, unsigned int colour);

#endif
