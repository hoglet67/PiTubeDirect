#include <stdio.h>
#include <string.h>
#include "tube.h"
#include "tube-ula.h"
#include "cpu80186/cpu80186.h"
#include "cpu80186/mem80186.h"
#include "cpu80186/iop80186.h"
#include "utils.h"
#include "framebuffer/framebuffer.h"

extern uint8_t Client86_v1_01[];

static uint8_t *vdu_base;

static int xios83_type = 0;
static uint32_t xios83_screen_start = 0xB800; // Default to standard framebuffer segment
static uint32_t xios83_screen_end = 0xBD00;


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
   // If VDU enabled, switch to mode 12
   if (vdu_enabled) {
      fb_writec(22);
      fb_writec(12);
      vdu_base = (uint8_t *)fb_get_vdu_address(fb_get_current_screen_mode());
   }
}

unsigned int copro_80186_tube_read(uint16_t addr) {
  return tube_parasite_read(addr);
}

void copro_80186_tube_write(uint16_t addr, uint8_t data) {
  tube_parasite_write(addr, data);
}

void copro_80186_xios_hook(uint16_t ax, uint16_t bx, uint16_t cx) {
   if ((ax & 0xff) == 0x83) {
      printf("XIOS 83: bx=%04x cx=%04x\r\n", bx, cx);
      xios83_type = cx;
      xios83_screen_start = bx << 4;
      switch (xios83_type) {
      case 2:
         xios83_screen_end = xios83_screen_start + 0xA000;
         break;
      default:
         xios83_screen_end = xios83_screen_start + 0x5000;
         break;
      }
   }
}

void copro_80186_write_hook(uint32_t addr32, uint8_t value) {
   if (vdu_enabled) {
      // MODE 12 is 640x256 with 16 colours (black = 0; white = 7)
      if (addr32 >= xios83_screen_start && addr32 < xios83_screen_end) {
         uint8_t mask;
         uint8_t *vdu_ptr;
         addr32 -= xios83_screen_start;
         switch (xios83_type) {
         case 0:
            // Type 0: BBC Mode 3, used for DOS Mode 7.
            break;
         case 1:
            // Type 1: BBC Mode 0, used for standard (2-colour) GEM
            // Colour 0 => Black (0)
            // Colour 1 => White (7)
            vdu_ptr = vdu_base + (addr32 << 3);
            for (int i = 0; i < 8; i++) {
               if (value & 128) {
                  *vdu_ptr++ = 7; // white
               } else {
                  *vdu_ptr++ = 0; // black
               }
               value <<= 1;
            }
            break;
         case 2:
            // Type 2: BBC Mode 1, used for 4-colour GEM.
            // Colour 00 => Black (0) 000
            // Colour 01 => Cyan  (6) 110
            // Colour 10 => Red   (1) 001
            // Colour 11 => White (7) 111
            mask = 6;
            if (addr32 >= 0x5000) {
               mask = 1;
               addr32 -= 0x5000;
            }
            vdu_ptr = vdu_base + (addr32 << 3);
            for (int i = 0; i < 8; i++) {
               *vdu_ptr &= ~mask;
               if (value & 128) {
                  *vdu_ptr |= mask;
               }
               vdu_ptr++;
               value <<= 1;
            }
            break;
         case 3:
            // Type 3: A 25-line, 4-colour, 40-column mode. It is used by DOS Screen Modes 0/1 and 4/5.
            break;
         case 4:
            // Type 4: The 25-line, 2-colour, 80-column mode without gaps between the lines, used by DOS Modes 2/3 and 6. (This is the screen type entered by DOS-Plus on system boot.)
            break;
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
