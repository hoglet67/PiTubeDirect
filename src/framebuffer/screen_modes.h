#ifndef _SCREEN_MODE_H
#define _SCREEN_MODE_H

#include <inttypes.h>

typedef uint32_t pixel_t;

typedef struct screen_mode {
   int mode_num;    // Mode number, used by VDU 22,N

   int width;       // width in physical pixels
   int height;      // height in physical pixels
   int bpp;         // bits per pixel (8,16,32)

   int num_colours; // number of colours

   int text_width;  // width in text characters
   int text_height; // height in text characters

   int pitch;       // filled in by init

   void           (*init)(struct screen_mode *screen);
   void          (*clear)(struct screen_mode *screen, int value);
   void         (*scroll)(struct screen_mode *screen);
   void     (*set_colour)(struct screen_mode *screen, unsigned int index, int r, int g, int b);
   uint32_t (*get_colour)(struct screen_mode *screen, unsigned int index);
   void      (*set_pixel)(struct screen_mode *screen, int x, int y, pixel_t value);
   pixel_t   (*get_pixel)(struct screen_mode *screen, int x, int y);

} screen_mode_t;


screen_mode_t *get_screen_mode(int mode_num);

// TODO: get rid of this
// It's only needed for fb_get_address which is only used by v3d
extern unsigned char* fb;

#endif
