/*
 * Z80 Co Pro Emulation
 *
 * (c) 2016 David Banks
 */
#include <stdio.h>
#include <string.h>
#include "tube-defs.h"
#include "tube.h"
#include "tube-ula.h"

static void copro_z80_poweron_reset() {
   // Wipe memory
   // TODO: add something here
}

static void copro_z80_reset() {
  // Log ARM performance counters
  tube_log_performance_counters();

  // Re-instate the Tube ROM on reset
  // TODO: add something here

  // Reset the processor
  // TODO: add something here

  // Do a tube reset
  tube_reset();

  // Reset ARM performance counters
  tube_reset_performance_counters();
}

void copro_z80_emulator()
{
   static unsigned int last_rst = 0;

   // Remember the current copro so we can exit if it changes
   int last_copro = copro;

   copro_z80_poweron_reset(); 
   copro_z80_reset();
  
   while (1)
   {
      // Execute emulator for one instruction
      // TODO: add something here

      if (tube_mailbox & ATTN_MASK) {
         unsigned int tube_mailbox_copy = tube_mailbox;
         tube_mailbox &= ~(ATTN_MASK | OVERRUN_MASK);
         unsigned int intr = tube_io_handler(tube_mailbox_copy);
         unsigned int nmi = intr & 2;
         unsigned int rst = intr & 4;
         // Reset the processor on active edge of rst
         if (rst && !last_rst) {
            // Exit if the copro has changed
            if (copro != last_copro) {
               break;
            }
            copro_z80_reset();
            // Wait for rst become inactive before continuing to execute
            tube_wait_for_rst_release();
         }
         // NMI is edge sensitive, so only check after mailbox activity
         if (nmi) {
            // TODO: add something to call the emulator NMI function here
         }
         last_rst = rst;
      }
      // IRQ is level sensitive, so check between every instruction
      if (tube_irq & 1) {
         // TODO: check if the emulator IRQ is enabled
         //if () {
            // TODO: add something to call the emulator IRQ function here
         //}
      }
   }
}
