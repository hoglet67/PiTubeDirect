#include <stdio.h>
#include <string.h>

#include "../startup.h"
#include "../swi.h"
#include "../tube-lib.h"
#include "../tube-swi.h"

#include "primitives.h"
#include "framebuffer.h"
#include "screen_modes.h"

static SWIHandler_Type base_handler[NUM_SWI_HANDLERS];

static vdu_device_t vdu_device = 1;

// ==========================================================================
// Static methods
// ==========================================================================

static void write_string(char *ptr) {
  char c;
  while ((c = *ptr++) != 0) {
     fb_writec(c);
  }
}

// ==========================================================================
// Implementation of SWIs that write to the VDU
// ==========================================================================

static void OS_WriteC_impl(unsigned int *reg) {
   if (vdu_device & VDU_PI) {
      fb_writec((char)(reg[0] & 0xff));
   }
   if (vdu_device & VDU_BEEB) {
      base_handler[SWI_OS_WriteC](reg);
   }
}

static void OS_WriteS_impl(unsigned int *reg) {
   // On exit, the link register should point to the byte after the terminator
   uint32_t r13 = reg[13];
   if (vdu_device & VDU_PI) {
      write_string((char *)reg[13]);
   }
   if (vdu_device & VDU_BEEB) {
      base_handler[SWI_OS_WriteS](reg);
   }
   // Reg 13 is the stacked link register which points to the string
   reg[13] = r13 + strlen((char *)r13) + 1;
   // Make sure new value of link register is word aligned to the next word boundary
   reg[13] += 3;
   reg[13] &= 0xFFFFFFFC;
}

static void OS_Write0_impl(unsigned int *reg) {
   // On exit, R0 should point to the byte after the terminator
   uint32_t r0 = reg[0];
   if (vdu_device & VDU_PI) {
      write_string((char *)reg[0]);
   }
   if (vdu_device & VDU_BEEB) {
      base_handler[SWI_OS_Write0](reg);
   }
   reg[0] = r0 + strlen((char *)r0) + 1;
}


static void OS_NewLine_impl(unsigned int *reg) {
   if (vdu_device & VDU_PI) {
      fb_writec(0x0A);
      fb_writec(0x0D);
   }
   if (vdu_device & VDU_BEEB) {
      base_handler[SWI_OS_NewLine](reg);
   }
}

static void OS_Plot_impl(unsigned int *reg) {
   if (vdu_device & VDU_PI) {
      fb_writec(25);
      fb_writec((char)reg[0]);
      fb_writec((char)reg[1]);
      fb_writec((char)(reg[1] >> 8));
      fb_writec((char)reg[2]);
      fb_writec((char)(reg[2] >> 8));
   }
   if (vdu_device & VDU_BEEB) {
      base_handler[SWI_OS_Plot](reg);
   }
}

static void OS_WriteN_impl(unsigned int *reg) {
   if (vdu_device & VDU_PI) {
      char *ptr = (char *)reg[0];
      uint32_t len = reg[1];
      while (len-- > 0) {
         fb_writec(*ptr++);
      }
   }
   if (vdu_device & VDU_BEEB) {
      base_handler[SWI_OS_WriteN](reg);
   }
}

// ==========================================================================
// Implementation of OS_Byte SWI
// ==========================================================================

static void OS_Byte_impl(unsigned int *reg) {
#ifdef DEBUG_VDU
    printf("%08x %08x %08x\r\n", reg[0], reg[1], reg[2]);
#endif

  if (vdu_device & VDU_PI) {

     // Handle VDU Specific OSBYTEs if the Pi VDU is enabled

     switch (reg[0] & 0xff) {
     case 9:
        // Set the flash mark time
        reg[2] = fb_get_flash_mark_time();
        fb_set_flash_mark_time(reg[1] & 0xff);
        break; // pass through to host

     case 10:
        // Set the flash mark time
        reg[2] = fb_get_flash_space_time();
        fb_set_flash_space_time(reg[1] & 0xff);
        break; // pass through to host

     case 19:
        // Wait for VSYNC
        _enable_interrupts(); // re-enable interrupts or we'll hang fr ever
        fb_wait_for_vsync();  // wait for the vsync flag to be set by the ISR
        return;

     case 134:
        // Read text cursor position
        reg[1] = (uint32_t)fb_get_text_cursor_x();
        reg[2] = (uint32_t)fb_get_text_cursor_y();
        return;

     case 135:
        // Read character at text cursor position
        reg[1] = (uint32_t)fb_get_text_cursor_char();
        // Also returns current screen mode
        reg[2] = (uint32_t)fb_get_current_screen_mode()->mode_num;
        return;
     }
  }

  // Otherwise pass call to the old handler
  base_handler[SWI_OS_Byte](reg);

}

// ==========================================================================
// Implementation of OS_ReadLine SWI
// ==========================================================================

static inline void writeVDU(unsigned int c) {
   OS_WriteC_impl(&c);
}

static void OS_ReadLine_impl(unsigned int *reg) {

   if (vdu_device & VDU_PI) {

      // Handle ReadLine if the Pi VDU is enabled

      uint32_t ptr = 0;
      unsigned char esc;

      uint32_t buf_size  = reg[1];
      int min_ascii = reg[2] & 0xff;
      int max_ascii = reg[3] & 0xff;

      while (1) {

         // OSRDCH R2: &00 Cy A
         sendByte(R2_ID, 0x00);
         esc = receiveByte(R2_ID) & 0x80;
         unsigned char c = receiveByte(R2_ID);

         // Handle escape
         if (esc) {
            break;
         }

         // Handle delete
         if (c == 127) {
            if (ptr > 0) {
               writeVDU(127);
               ptr--;
            }
            continue;
         }

         // Handle delete line
         if (c == 0x15) {
            while (ptr > 0) {
               writeVDU(127);
               ptr--;
            }
            continue;
         }

         // Handle copy
         if (c == 0x87) {
            c = (uint8_t)fb_get_edit_cursor_char();
            // Invalid?
            if (c == 0) {
               continue;
            }
            // Delete? replace with square block
            if (c == 0x7f) {
               c = 0xff;
            }
            // move the editing cursor to the right
            writeVDU(0x15);
            writeVDU(0x1B);
            writeVDU(0x89);
            writeVDU(0x06);
         } else if (c >= 0x88 && c <= 0x8b) {
            // Handle cursor keys just by echoing them, but not storing them
            writeVDU(0x15);
            writeVDU(0x1B);
            writeVDU(c);
            writeVDU(0x06);
            continue;
         }

         // Store the character in the buffer
         *(((char *)reg[0]) + ptr) = c;

         // Handle return
         if (c == 13) {
            writeVDU(10);
            writeVDU(13);
            break;
         }

         // Handle the buffer being full
         if (ptr >= buf_size) {
            writeVDU(7);
            continue;
         }

         // Echo the character
         writeVDU(c);

         // Handle invalid characters
         if (c < min_ascii || c > max_ascii) {
            continue;
         }

         // Move to next location in the buffer
         ptr++;
      }

      // Exit with reg[1] to the length (excluding the terminator)
      // and with the carry indicatingthe escape condition
      reg[1] = ptr;
      updateCarry(esc, reg);

   } else {

      // Otherwise pass call to the old handler
      base_handler[SWI_OS_ReadLine](reg);

   }
}

// ==========================================================================
// Implementation of OS_ScreenMode SWI
// ==========================================================================

static void OS_ScreenMode_impl(unsigned int *reg) {
   if (reg[0] == 0) {
      if (reg[1] < 256) {
         fb_writec(22);
         fb_writec((uint8_t)reg[1]);
         return;
      } else if ((reg[1] & 1) == 0) {
         int *selector_block = (int *)reg[1];
         int x_pixels = *(selector_block + 1);
         int y_pixels = *(selector_block + 2);
         int log2bpp = *(selector_block + 3);
         printf("OS_ScreenMode: x=%08x y=%08x log2bpp=%08x\r\n",
                x_pixels, y_pixels, log2bpp);

         // Number of colours:
         // log2bpp = 0: bpp =  1: n_colours = 2
         // log2bpp = 1: bpp =  2: n_colours = 4
         // log2bpp = 2: bpp =  4: n_colours = 16
         // log2bpp = 3: bpp =  8: n_colours = 256
         // log2bpp = 4: bpp = 16: n_colours = 65536
         // log2bpp = 5: bpp = 32: n_colours = 16777216 (2^24)
         unsigned int n_colours;
         if (log2bpp < 5) {
            n_colours = 1 << (1 << log2bpp);
         } else {
            n_colours = 1 << 24;
         }
         // Use private method here rather than VDU 23,22 to allow selection of high colour modes
         fb_custom_mode(x_pixels, y_pixels, n_colours);
         return;
      }
   }
   printf("OS_ScreenMode: Unsupported operation: r0=%08x r1=%08x\r\n", reg[0], reg[1]);
}


// ==========================================================================
// Implementation of OS_ReadModeVariable SWI
// ==========================================================================

// Entry
//   R0 Screen mode, or -1 for current
//   R1 Variable number
// Exit
//   R0 Preserved
//   R1 Preserved
//   R2 Value of variable
// C flag is set if variable or mode were invalid

static void OS_ReadModeVariable_impl(unsigned int *reg) {
   int mode = (int) reg[0];
   screen_mode_t *screen = (mode < 0) ? fb_get_current_screen_mode() : get_screen_mode(mode);
   // Return carry set if the screen mode could not be found, or unkonw VDU variable
   if (screen == NULL || reg[1] >= NUM_MODE_VARS) {
      updateCarry(1, reg);
   } else {
      reg[2] = (uint32_t)fb_read_mode_variable(reg[1], screen);
   }
}


// ==========================================================================
// Implementation of OS_ReadVduVariables SWI
// ==========================================================================

// Entry
//   R0 Pointer to input block
//   R1 Pointer to output block (can be the same as R0)
// Exit
//   R0 Preserved
//   R1 Preserved

static void OS_ReadVduVariables_impl(unsigned int *reg) {
   int32_t *iblock = (int32_t *)reg[0];
   int32_t *oblock = (int32_t *)reg[1];
   while (*iblock != -1) {
      *oblock++ = fb_read_vdu_variable(*iblock++);
   }
}

// ==========================================================================
// Implementation of OS_ReadPoint SWI
// ==========================================================================

// Entry
//   R0	X co-ordinate
//   R1	Y co-ordinate
// Exit
//   R0	Preserved
//   R1	Preserved
//   R2	Colour
//   R3	Tint
//   R4	-1 if point was off screen, else 0
// Notes
//   If in a 16 or 32 bpp mode, this will return the 15 or 24 bit
//   colour number in R2 and tint will be 0.

static void OS_ReadPoint_impl(unsigned int *reg) {
   // Make sure both the coordinates are in the range of a 16-bit value
   if (((reg[0] | reg[1]) & 0xFFFF0000) == 0) {
      int16_t x = (int16_t)reg[0];
      int16_t y = (int16_t)reg[1];
      pixel_t colour;
      if (!fb_point(x, y, &colour)) {
         if (fb_read_vdu_variable((vdu_variable_t)M_LOG2BPP) == 3) {
            uint8_t colnum = (uint8_t)(colour & 0xff);
            reg[2] = fb_get_col_from_colnum(colnum);
            reg[3] = fb_get_tint_from_colnum(colnum);
         } else {
            reg[2] = colour;
            reg[3] = 0;
         }
         reg[4] = 0;
         return;
      }
   }
   // Otherwise return off screen
   reg[2] = 0xFFFFFFFF;
   reg[3] = 0;
   reg[4] = 0xFFFFFFFF;
}

// On Entry:
//   R0 - flags
//        bits 2..0 = action
//        bit 3 = use transparency (ignored)
//        bit 4 = 0:foreground, 1:background
//        bit 5 = 0:r1 = colour number, 1:r1 = ECF pattern (ignored)
//        bit 6 = 0:graphics, 1:text
//        bit 7 = 0:write, 1:read (ginored)

static void OS_SetColour_impl(unsigned int *reg) {
   unsigned int flags = reg[0];
   pixel_t colour     = reg[1];
   if (flags & 0x40) {
      // Text
      if (flags & 0x10) {
         // background
         fb_set_c_bg_col(colour);
      } else {
         // foreground
         fb_set_c_fg_col(colour);
      }
   } else {
      // Graphics
      if (flags & 0x10) {
         // background
         fb_set_g_bg_col(flags & 7, colour);
      } else {
         // foreground
         fb_set_g_fg_col(flags & 7, colour);
      }
   }
}

// ==========================================================================
// Public methods
// ==========================================================================

void fb_set_vdu_device(vdu_device_t device) {
   vdu_device = device;
}


static int initialized = 0;

void fb_add_swi_handlers() {

   // Only need to setup the base handlers once
   if (!initialized) {
      for (int i = 0; i < NUM_SWI_HANDLERS; i++) {
         base_handler[i] = os_table[i].handler;
      }
      initialized = 1;
   }

   os_table[SWI_OS_WriteC].handler           = OS_WriteC_impl;
   os_table[SWI_OS_WriteS].handler           = OS_WriteS_impl;
   os_table[SWI_OS_Write0].handler           = OS_Write0_impl;
   os_table[SWI_OS_NewLine].handler          = OS_NewLine_impl;
   os_table[SWI_OS_Plot].handler             = OS_Plot_impl;
   os_table[SWI_OS_WriteN].handler           = OS_WriteN_impl;
   os_table[SWI_OS_Byte].handler             = OS_Byte_impl;
   os_table[SWI_OS_ReadLine].handler         = OS_ReadLine_impl;
   os_table[SWI_OS_ScreenMode].handler       = OS_ScreenMode_impl;
   os_table[SWI_OS_ReadPoint].handler        = OS_ReadPoint_impl;
   os_table[SWI_OS_ReadModeVariable].handler = OS_ReadModeVariable_impl;
   os_table[SWI_OS_ReadVduVariables].handler = OS_ReadVduVariables_impl;
   os_table[SWI_OS_SetColour].handler        = OS_SetColour_impl;

}

void fb_remove_swi_handlers() {
   if (initialized) {
      for (int i = 0; i < NUM_SWI_HANDLERS; i++) {
         os_table[i].handler = base_handler[i];
      }
   }
}
