/*
 * 65816 Co Pro Emulation
 *
 * (c) 2016 Simon Ellwood (fordp)
 * (c) 2016 David Banks (hoglet)
 *
 */

#include <stdio.h>
#include <string.h>
#include "tube-defs.h"
#include "tube.h"
#include "tube-ula.h"
#include "65816/65816.h"
#include "65816/tuberom_65816.h"
#include "startup.h"

#define ROM_BASE 0xF800

static void copro_65816_poweron_reset() {
   w65816_init(tuberom_65816);
}

static void copro_65816_reset() {
  // Log ARM performance counters
  tube_log_performance_counters();
  // Reset 65816
  w65816_reset();
  // Wait for rst become inactive before continuing to execute
  tube_wait_for_rst_release();
  // Reset ARM performance counters
  tube_reset_performance_counters();
}

void copro_65816_emulator() {

   // Remember the current copro so we can exit if it changes
   int last_copro = copro;

   copro_65816_poweron_reset();
   copro_65816_reset();

   while (1) {
      // 32 is actually just 4 instructions
      // might need to reduce if we see LATEs
      w65816_exec(8);

      if (tube_irq & RESET_BIT ) {
         // Reset the processor on active edge of rst
         // Exit if the copro has changed
         if (copro != last_copro) {
            break;
         }
         copro_65816_reset();
      }
      // NMI is edge sensitive, so only check after mailbox activity
      // Note: 65816 uses tube_irq directly, so no nmi code here

      // IRQ is level sensitive, so check between every instruction
      // Note: 65816 uses tube_irq directly, so no irq code here
   }
}
