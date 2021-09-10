#include <stdio.h>

#include "screen_modes.h"
#include "framebuffer.h"
#include "module_colourtrans.h"

#define COLOURTRANS_MIN 0x40740
#define COLOURTRANS_MAX 0x40763

static SWIDescriptor_Type colourtrans_table[] = {

    { NULL, "SelectTable" },                  // &40740
    { NULL, "SelectGCOLTable" },              // &40741
    { NULL, "ReturnGCOL" },                   // &40742
    { NULL, "SetGCOL" },                      // &40743
    { NULL, "ReturnColourNumber" },           // &40744
    { NULL, "ReturnGCOLForMode" },            // &40745
    { NULL, "ReturnColourNumberForMode" },    // &40746
    { NULL, "ReturnOppGCOL" },                // &40747
    { NULL, "SetOppGCOL" },                   // &40748
    { NULL, "ReturnOppColourNumber" },        // &40749
    { NULL, "ReturnOppGCOLForMode" },         // &4074A
    { NULL, "ReturnOppColourNumberForMode" }, // &4074B
    { NULL, "GCOLToColourNumber" },           // &4074C
    { NULL, "ColourNumberToGCOL" },           // &4074D
    { NULL, "ReturnFontColours" },            // &4074E
    { NULL, "SetFontColours" },               // &4074F
    { NULL, "InvalidateCache" },              // &40750
    { NULL, "SetCalibration" },               // &40751
    { NULL, "ReadCalibration" },              // &40752
    { NULL, "ConvertDeviceColour" },          // &40753
    { NULL, "ConvertDevicePalette" },         // &40754
    { NULL, "ConvertRGBToCIE" },              // &40755
    { NULL, "ConvertCIEToRGB" },              // &40756
    { NULL, "WriteCalibrationToFile" },       // &40757
    { NULL, "ConvertRGBToHSV" },              // &40758
    { NULL, "ConvertHSVToRGB" },              // &40759
    { NULL, "ConvertRGBToCMYK" },             // &4075A
    { NULL, "ConvertCMYKToRGB" },             // &4075B
    { NULL, "ReadPalette" },                  // &4075C
    { NULL, "WritePalette" },                 // &4075D
    { NULL, "SetColour" },                    // &4075E
    { NULL, "MiscOp" },                       // &4075F
    { NULL, "WriteLoadingsToFile" },          // &40760
    { NULL, "SetTextColour" },                // &40761
    { NULL, "SetOppTextColour" },             // &40762
    { NULL, "SelectTable" }                   // &40763
};

// Entry
//   R0	Palette entry
//   R3	Flags
//   R4	GCOL action (only the bottom 3 bits are valid)
// Exit
//   R0	GCOL
//   R2	Log2 of bits-per-pixel for current mode
//   R3	Initial value AND &80
//   R4	Preserved
static void colourtrans_setgcol(unsigned int *reg) {
   uint8_t r = (uint8_t)((reg[0] >>  8) & 0xff);
   uint8_t g = (uint8_t)((reg[0] >> 16) & 0xff);
   uint8_t b = (uint8_t)((reg[0] >> 24) & 0xff);
   unsigned int flags = reg[3];
   uint8_t action = reg[4] & 7;
   screen_mode_t *screen = fb_get_current_screen_mode();
   pixel_t col = screen->nearest_colour(screen, r, g, b);
   if (flags & 0x80) {
      fb_set_g_bg_col(action, col);
   } else {
      fb_set_g_fg_col(action, col);
   }
   reg[0] = (unsigned int)col;
   reg[2] = (unsigned int)screen->log2bpp;
   reg[3] = (unsigned int)(reg[3] & 0x80);
}

// Entry
//   R0	Palette entry
//   R3	Flags
// Exit
//   R0	GCOL
//   R3	Preserved
static void colourtrans_settextcolour(unsigned int *reg) {
   uint8_t r = (uint8_t)((reg[0] >>  8) & 0xff);
   uint8_t g = (uint8_t)((reg[0] >> 16) & 0xff);
   uint8_t b = (uint8_t)((reg[0] >> 24) & 0xff);
   unsigned int flags = reg[3];
   screen_mode_t *screen = fb_get_current_screen_mode();
   pixel_t col = screen->nearest_colour(screen, r, g, b);
   if (flags & 0x80) {
      fb_set_c_bg_col(col);
   } else {
      fb_set_c_fg_col(col);
   }
   reg[0] = (unsigned int )col;
}

static void colourtrans_init(int vdu) {
   colourtrans_table[SWI_COLOURTRANS_SETGCOL       - COLOURTRANS_MIN].handler = vdu ? colourtrans_setgcol       : NULL;
   colourtrans_table[SWI_COLOURTRANS_SETTEXTCOLOUR - COLOURTRANS_MIN].handler = vdu ? colourtrans_settextcolour : NULL;
}

module_t module_colourtrans = {
   .name            = "ColourTrans",
   .swi_num_min     = COLOURTRANS_MIN,
   .swi_num_max     = COLOURTRANS_MAX,
   .swi_table       = colourtrans_table,
   .init            = colourtrans_init
};
