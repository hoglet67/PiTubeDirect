#include <stdio.h>

#include "../startup.h"
#include "../swi.h"
#include "../tube-swi.h"

#include "framebuffer.h"
#include "screen_modes.h"
#include "swi_impl.h"

static SWIHandler_Type old_OS_Byte_impl;

static int logbase2(int i) {
   int log2 = 0;
   while (i > 1) {
      i >>= 1;
      log2++;
   }
   return log2;
}

static void OS_Byte_impl(unsigned int *reg) {
#ifdef DEBUG_VDU
    printf("%08x %08x %08x\r\n", reg[0], reg[1], reg[2]);
#endif

  // Handle VDU Specific OSBYTEs if the Pi VDU is enabled
  if (getVDUDevice() & VDU_PI) {
     switch (reg[0] & 0xff) {
     case 19:
        // Wait for VSYNC
        _enable_interrupts(); // re-enable interrupts or we'll hang fr ever
        fb_wait_for_vsync();  // wait for the vsync flag to be set by the ISR
        return;
     case 135:
        // return character at the cursor, and the current screen mode
        reg[1] = fb_get_edit_cursor_char();
        reg[2] = fb_get_current_screen_mode();
        return;
     }
  }

  // Otherise pass call to the old handler
  old_OS_Byte_impl(reg);
}

static void OS_ScreenMode_impl(unsigned int *reg) {
   if (reg[0] == 0) {
      if (reg[1] < 256) {
         fb_writec(22);
         fb_writec(reg[1]);
         return;
      } else if ((reg[1] & 1) == 0) {
         unsigned int *selector_block = (unsigned int *)reg[1];
         unsigned int xpixels = *(selector_block + 1);
         unsigned int ypixels = *(selector_block + 2);
         unsigned int log2bpp = *(selector_block + 3);
         printf("OS_ScreenMode: x=%08x y=%08x log2bpp=%08x\r\n",
                xpixels, ypixels, log2bpp);
         fb_writec(23);
         fb_writec(22);
         fb_writec(xpixels & 0xff);
         fb_writec((xpixels >> 8) & 0xff);
         fb_writec(ypixels & 0xff);
         fb_writec((ypixels >> 8) & 0xff);
         fb_writec(8);
         fb_writec(8);
         // number of colours:
         /// log2bpp = 0: bpp = 1: n_colours = 2
         /// log2bpp = 1: bpp = 2: n_colours = 4
         /// log2bpp = 2: bpp = 4: n_colours = 16
         /// log2bpp = 3: bpp = 8: n_colours = 256 (sent as 0x00)
         fb_writec((1 << (1 << log2bpp)) & 0xff);
         fb_writec(0);
         return;
      }
   }
   printf("OS_ScreenMode: Unsupported operation: r0=%08x r1=%08x\r\n", reg[0], reg[1]);
}


// OS_ReadModeVariable (SWI &35)
// Entry
//   R0 Screen mode, or -1 for current
//   R1 Variable number
// Exit
//   R0 Preserved
//   R1 Preserved
//   R2 Value of variable
// C flag is set if variable or mode were invalid

static void OS_ReadModeVariable_impl(unsigned int *reg) {
   screen_mode_t *screen = get_screen_mode(reg[0]);

   // Return carry set if the screen mode could not be found
   if (screen == NULL) {
      updateCarry(1, reg);
      return;
   }

   switch (reg[1]) {
   case 0:
      // ModeFlags: Assorted flags
      break;
   case 1:
      // ScrRCol: Number of text columns -1
      reg[2] = screen->width >> 3; // assumes system font is 8x8
      break;
   case 2:
      // ScrBRow: Number of text rows -1
      reg[2] = screen->height >> 3; // assumes system font is 8x8
      break;
   case 3:
      // NColour: Maximum logical colour
      reg[2] = screen->ncolour;
      break;
   case 4:
      // XEigFactor: Conversion factor between OS units and pixels
      reg[2] = screen->xeigfactor;
      break;
   case 5:
      // YEigFactor: Conversion factor between OS units and pixels
      reg[2] = screen->yeigfactor;
      break;
   case 6:
      // LineLength: Number of bytes per pixel row
      reg[2] = screen->width * (screen->bpp >> 3);
      break;
   case 7:
      // ScreenSize: Number of bytes for entire screen display
      reg[2] = screen->height * screen->width * (screen->bpp >> 3);
      break;
   case 8:
      // YShiftSize: Deprecated. Do not use
      break;
   case 9:
      // Log2BPP: Log base 2 of bits per pixel
      reg[2] = logbase2(screen->bpp);
      break;
   case 10:
      // Log2BPC: Log base 2 of bytes per character
      reg[2] = logbase2(screen->bpp); // number of bytes wide for a screen character
      break;
   case 11:
      // XWindLimit: Number of x pixels on screen -1
      reg[2] = screen->width;
      break;
   case 12:
      // YWindLimit: Number of y pixels on screen -1
      reg[2] = screen->height;
      break;
   default:
      // Return carry set if unknown VDU variable
      updateCarry(1, reg);
   }
}

void fb_add_swi_handlers() {

   old_OS_Byte_impl = SWIHandler_Table[SWI_OS_Byte];

   SWIHandler_Table[SWI_OS_Byte] = OS_Byte_impl;
   SWIHandler_Table[SWI_OS_ScreenMode] = OS_ScreenMode_impl;
   SWIHandler_Table[SWI_OS_ReadModeVariable] = OS_ReadModeVariable_impl;

}
