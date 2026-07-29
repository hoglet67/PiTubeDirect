#include <stdio.h>
#include <string.h>
#include "tube.h"
#include "tube-ula.h"
#include "cpu80186/cpu80186.h"
#include "cpu80186/mem80186.h"
#include "cpu80186/iop80186.h"
#include "utils.h"
#include "framebuffer/framebuffer.h"
#include "framebuffer/fonts.h"

extern uint8_t Client86_v1_01[];

#define CGA_SCREEN_START 0xB8000
#define CGA_SCREEN_END   0xBD000


static uint32_t screen_start = CGA_SCREEN_START;
static uint32_t screen_end = CGA_SCREEN_END;
static int gemmode = 0;

static screen_mode_t *screen;
static uint8_t *vdu_base;
static int textmode = 1;
static unsigned int textcols = 80;
static unsigned int textrows = 25;
static int graphwidth = 0;

void refresh_vdu_vars() {
   printf("Refreshing VDU variables\r\n");
   screen = fb_get_current_screen_mode();
   initialize_font_by_name("8X8", screen->font);
   vdu_base = (uint8_t *)fb_get_vdu_address(screen);
}

static void copro_80186_poweron_reset() {
   // Wipe memory
   Cleari80186Ram();
   // Patch the OSWORD &FA code to change FEE5 to FCE5 (8 changes expected)
   check_elk_mode_and_patch(Client86_v1_01, 0xE69, 0x1FB, 8);
}

static void copro_80186_reset() {
  // Log ARM performance counters
  tube_log_performance_counters();
  // Re-instate the Tube ROM on reset
  RomCopy();
  // Reset cpu186
  reset();
  // Wait for rst become inactive before continuing to execute
  tube_wait_for_rst_release();
  // Reset ARM performance counters
  tube_reset_performance_counters();
   // If VDU enabled, switch to mode 8 (640x256 in 4 colours)
   if (vdu_enabled) {
      fb_writec(22);
      fb_writec(8);
      refresh_vdu_vars();
   }
}


//command then, address high, then address low (8 bytes)
typedef enum {
   IDLE,
   SKIP_N,
   SKIP_STRING,
   OSWORD_A,
   OSWORD_INLEN,
   OSWORD_BLOCK,
   OSWORD_OUTLEN,
   OSWORD_FF_ADDR_MSB,
   OSWORD_FF_ADDR_LSB,
   OSWORD_FF_CMD,
   OSFIND_A,
   OSFILE_BLOCK,
   OSFILE_STRING
} decode_state_t;

static decode_state_t state = IDLE;

static void decode_r2_byte(uint8_t data) {

   static uint8_t n = 0;
   static uint8_t cmd = 0;

   // Need to parse the input side of the R2 protocol to identify OSWORD &FF
   //
   // Input parameters                               Output parameters
   // OSRDCH   R2: &00                               Cy A
   // OSCLI    R2: &02 string &0D                    &7F or &80
   // OSBYTELO R2: &04 X A                           X
   // OSBYTEHI R2: &06 X Y A                         Cy Y X
   // OSWORD   R2: &08 A in_length block out_length  block
   // OSWORD0  R2: &0A block                         &FF or &7F string &0D
   // OSARGS   R2: &0C Y block A                     A block
   // OSBGET   R2: &0E Y                             Cy A
   // OSBPUT   R2: &10 Y A                           &7F
   // OSFIND   R2: &12 &00 Y                         &7F
   // OSFIND   R2: &12 A string &0D                  A
   // OSFILE   R2: &14 block string &0D A            A block
   // OSGBPB   R2: &16 block A                       block Cy A


   switch (state) {
   case IDLE:
      switch (data) {
      case 0x00:
         // OSRDCH   R2: &00
         break;
      case 0x02:
         // OSCLI    R2: &02 string &0D
         state = SKIP_STRING;
         break;
      case 0x04:
         // OSBYTELO R2: &04 X A
         n = 1 + 1;
         state = SKIP_N;
         break;
      case 0x06:
         // OSBYTEHI R2: &06 X Y A
         n = 1 + 1 + 1;
         state = SKIP_N;
         break;
      case 0x08:
         // OSWORD   R2: &08 A in_length block out_length
         state = OSWORD_A;
         break;
      case 0x0A:
         // OSWORD0  R2: &0A block
         n = 5;
         state = SKIP_N;
         break;
      case 0x0C:
         // OSARGS   R2: &0C Y block A
         n = 1 + 4 + 1;
         state = SKIP_N;
         break;
      case 0x0E:
         // OSBGET   R2: &0E Y
         break;
         n = 1;
         state = SKIP_N;
         break;
      case 0x10:
         // OSBPUT   R2: &10 Y A
         n = 1 + 1;
         state = SKIP_N;
         break;
      case 0x12:
         // OSFIND   R2: &12 A string &0D
         state = OSFIND_A;
         break;
      case 0x14:
         // OSFILE   R2: &14 block string &0D A
         n = 16;
         state = OSFILE_BLOCK;
         break;
      case 0x16:
         // OSGBPB   R2: &16 block A
         n = 13 + 1;
         state = SKIP_N;
         break;
      default:
         // Protocol error
         printf("R2 protocol error: %02x\r\n", data);
      }
      break;
   case SKIP_N:
      n--;
      if (!n) {
         state = IDLE;
      }
      break;
   case SKIP_STRING:
      if (data == 0x0d) {
         state = IDLE;
      }
      break;
   case OSWORD_A:
      cmd = data;
      state = OSWORD_INLEN;
      break;
   case OSWORD_INLEN:
      n = data;
      if (cmd == 0xff) {
         state = OSWORD_FF_ADDR_MSB;
      } else {
         state = OSWORD_BLOCK;
      }
      break;
   case OSWORD_BLOCK:
      n--;
      if (!n) {
         state = OSWORD_OUTLEN;
      }
      break;
   case OSWORD_OUTLEN:
      state = IDLE;
      break;
   case OSWORD_FF_ADDR_MSB:
      if (data == 0x00) {
         state = OSWORD_OUTLEN;
      } else {
         state = OSWORD_FF_ADDR_LSB;
      }
      break;
   case OSWORD_FF_ADDR_LSB:
      state = OSWORD_FF_CMD;
      break;
   case OSWORD_FF_CMD:
      if (data == 0xff) {
         state = OSWORD_FF_ADDR_MSB;
      }
      break;
   case OSFIND_A:
      state = SKIP_STRING;
      break;
   case OSFILE_BLOCK:
      n--;
      if (!n) {
         state = OSFILE_STRING;
      }
      break;
   case OSFILE_STRING:
      if (data == 0x0d) {
         n = 1;
         state = SKIP_N;
      }
      break;
   }
}

static void decode_r1_byte(uint8_t data) {
   static int modechange = 0;
   // forward normal (i.e. none block) data to VDU driver
   if (state != OSWORD_FF_CMD) {
      // remap VDU 22,1 to VDU 22,8
      if (modechange && data == 1) {
         data = 8;
      }
      printf("VDU: %c (%d)\r\n", (data >= 32 && data < 127) ? ((char) data) : '?', data);
      fb_writec(data);
      if (modechange) {
         // Refresh the locally cached VDU variables on each mode change
         refresh_vdu_vars();
         modechange = 0;
      } else if (data == 22) {
         modechange = 1;
      }
   }
}

unsigned int copro_80186_tube_read(uint16_t addr) {
  return tube_parasite_read(addr);
}

void copro_80186_tube_write(uint16_t addr, uint8_t data) {
   if (vdu_enabled) {
      if (addr == 3) {
         decode_r2_byte(data);
      }
      if (addr == 1) {
         decode_r1_byte(data);
      }
   }
   tube_parasite_write(addr, data);
}

void copro_80186_int10_hook(uint16_t ax, uint16_t bx, uint16_t cx) {
   printf("INT10: ax=%04x bx=%04x cx=%04x\r\n", ax, bx, cx);
   // AH of 0x00 indicates change to a standard CGA screen mode (mode indicated in AL)
   if (ax < 0x0008) {
      // Cancel GEM mode and reset the screen to the standard CGA locations
      gemmode = 0;
      screen_start = CGA_SCREEN_START;
      screen_end = CGA_SCREEN_END;
      switch (ax) {
      case 0x0000:
      case 0x0001:
         // On BBC DOSPlus, CGA modes 0,1 are 40x25 text modes
         textmode = 1;
         textcols = 40;
         textrows = 25;
         break;
      case 0x0002:
      case 0x0003:
      case 0x0007:
         // On BBC DOSPlus, CGA modes 2,3,7 are 80x25 text modes
         textmode = 1;
         textcols = 80;
         textrows = 25;
         break;
      case 0x0004:
      case 0x0005:
         // On BBC DOSPlus, CGA modes 4,5 are 320x200 graphics modes
         textmode = 0;
         graphwidth = 320;
         break;
      case 0x0006:
         // On BBC DOSPlus, CGA mode 6 is a 640x200 graphics modes
         textmode = 0;
         graphwidth = 640;
         break;
      }
   }
}

void copro_80186_xios_hook(uint16_t ax, uint16_t bx, uint16_t cx) {
   if ((ax & 0xff) == 0x83) {
      printf("XIOS 83: bx=%04x cx=%04x\r\n", bx, cx);
      if (cx == 1) {
         // Monochrome GEM
         gemmode = 1;
         screen_start = bx << 4;
         screen_end = screen_start + 0x5000;
      } else if (cx == 2) {
         // Colour GEM
         gemmode = 2;
         screen_start = bx << 4;
         screen_end = screen_start + 0xA000;
      } else {
         // None-GEM
         gemmode = 0;
         screen_start = CGA_SCREEN_START;
         screen_end = CGA_SCREEN_END;
      }
   }
}

void copro_80186_write_hook(uint32_t addr32, uint8_t value) {
   if (vdu_enabled) {
      if (addr32 >= screen_start && addr32 < screen_end) {
         addr32 -= screen_start;
         // Deal with text modes first
         if (gemmode == 1 || (graphwidth == 640 && !textmode)) {
            // Monochrome GEM and DOS Mode 6 (640x256 2-colour graphics)
            uint8_t *vdu_ptr = vdu_base + (addr32 << 3);
            for (int i = 0; i < 8; i++) {
               if (value & 128) {
                  *vdu_ptr++ = 1;
               } else {
                  *vdu_ptr++ = 0;
               }
               value <<= 1;
            }
         } else if (gemmode == 2 || (graphwidth == 320 && !textmode)) {
            // Colour GEM and DOS Modes 4/5 (320x256 4-colour graphics)
            uint8_t mask = 1;
            if (addr32 >= 0x5000) {
               mask = 2;
               addr32 -= 0x5000;
            }
            uint8_t *vdu_ptr = vdu_base + (addr32 << 3);
            for (int i = 0; i < 8; i++) {
               *vdu_ptr &= 0xFF - mask;
               if (value & 128) {
                  *vdu_ptr |= mask;
               }
               vdu_ptr++;
               value <<= 1;
            }
         } else if (textmode) {
            if (!(addr32 & 1)) {
               // Ignore the attribute byte for now
               addr32 >>= 1;
               if (addr32 < textcols * textrows) {
                  screen->write_character(screen, value, (int) (addr32 % textcols), (int) (addr32 / textcols), 1, 0);
               }
            }
         }
      }
   }
}

void copro_80186_emulator()
{
   // Remember the current copro so we can exit if it changes
   unsigned int last_copro = copro;

   copro_80186_poweron_reset();
   copro_80186_reset();

   while (1)
   {
      exec86(1);
      int tube_irq_copy = tube_irq & ( RESET_BIT + NMI_BIT + IRQ_BIT) ;
      if (tube_irq_copy) {
         // Reset the processor on active edge of rst
         if (tube_irq_copy & RESET_BIT) {
            // Exit if the copro has changed
            if (copro != last_copro) {
               break;
            }
            copro_80186_reset();
         }

         // NMI is edge sensitive, so only check after mailbox activity
         if (tube_irq_copy & NMI_BIT) {
            // intcall86(2);
            x86_dma();
            tube_ack_nmi();
         }

         // IRQ is level sensitive, so check between every instruction
         if (tube_irq_copy & IRQ_BIT) {
            if (ifl) {
               intcall86(12);
            }
         }
      }
   }
}
