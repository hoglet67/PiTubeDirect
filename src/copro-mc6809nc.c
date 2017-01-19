/*
 * 6809 Co Pro Emulation based on Neal Crook's 6809 emulator
 *
 * (c) 2016 David Banks
 */
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "tube-defs.h"
#include "tube.h"
#include "tube-ula.h"
#include "startup.h"
#include "tuberom_6809.h"

#include "copro-mc6809nc.h"

#include "mc6809nc/mc6809.h"

static int overlay_rom = 0;

static unsigned char *copro_mc6809_ram;

static unsigned char *copro_mc6809_rom = tuberom_6809_jgh_1_0;

static int debug = 0;

void copro_mc6809nc_write(uint16_t addr, uint8_t data) {
   if ((addr & 0xFFF0) == 0xFEE0) {
      overlay_rom = 0;
      tube_parasite_write(addr & 7, data);
   } else {
      copro_mc6809_ram[addr & 0xffff] = data;
   }
   if (debug) {
      printf("Wr %04x=%02x\r\n", addr, data);
   }
}

uint8_t copro_mc6809nc_read(uint16_t addr) {
   uint8_t data;
   if ((addr & 0xFFF0) == 0xFEE0) {
      overlay_rom = 0;
      data = tube_parasite_read(addr & 7);
   } else if (overlay_rom) {
      data = copro_mc6809_rom[addr & 0x7ff];
   } else {
      data = copro_mc6809_ram[addr & 0xffff];
   }
   if (debug) {
      printf("Rd %04x=%02x\r\n", addr, data);
   }
   return data;
}

static void copro_mc6809_poweron_reset() {
   // Initialize memory pointer to zero (the start of the 2MB of memory shared with the 6502)
   copro_mc6809_ram = (unsigned char *) 0;
   // Wipe memory
   memset(copro_mc6809_ram, 0, 0x10000);
}

static void copro_mc6809_reset() {
  // Log ARM performance counters
  tube_log_performance_counters();

  // Re-instate the Tube ROM on reset
  overlay_rom = 1;

  // Reset the processor
  mc6809nc_reset();

  // Wait for rst become inactive before continuing to execute
  tube_wait_for_rst_release();

  // Reset ARM performance counters
  tube_reset_performance_counters();
}

void copro_mc6809nc_emulator()
{

   static unsigned int last_rst = 0;

   // Remember the current copro so we can exit if it changes
   int last_copro = copro;

   copro_mc6809_poweron_reset(); 
   copro_mc6809_reset();
  
   while (1)
   {
      // Execute emulator for one instruction
      mc6809nc_execute(1);

      if (is_mailbox_non_empty()) {
         unsigned int tube_mailbox_copy = read_mailbox();
         unsigned int intr = tube_io_handler(tube_mailbox_copy);
         unsigned int nmi = intr & 2;
         unsigned int rst = intr & 4;
         // Reset the processor on active edge of rst
         if (rst && !last_rst) {
            // Exit if the copro has changed
            if (copro != last_copro) {
               break;
            }
            copro_mc6809_reset();
         }
         // NMI is edge sensitive, so only check after mailbox activity
         if (nmi) {
            mc6809nc_request_irq(1);
         }

         last_rst = rst;
      }
      // IRQ is level sensitive, so check between every instruction
      if (tube_irq & 1) {
         mc6809nc_request_firq(1);
      }
   }
}
