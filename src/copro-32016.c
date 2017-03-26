/*
 * 32016 Co Pro Emulation
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
#include "NS32016/32016.h"
#include "NS32016/mem32016.h"
#include "startup.h"

#define PANDORA_BASE 0xF00000

int tubecycles = 0;

static void copro_32016_poweron_reset() {
   n32016_init();
}

static void copro_32016_reset() {
  // Log ARM performance counters
  tube_log_performance_counters();
  // Reset 32016
  n32016_reset_addr(PANDORA_BASE); // Start directly in the ROM
  // Wait for rst become inactive before continuing to execute
  tube_wait_for_rst_release();
  // Reset ARM performance counters
  tube_reset_performance_counters();
}

void copro_32016_emulator() {

   // Remember the current copro so we can exit if it changes
   int last_copro = copro;

   copro_32016_poweron_reset();
   copro_32016_reset();
  
   while (1) {
      // 32 is actually just 4 instructions
      // might need to reduce if we see LATEs
      tubecycles = 8;
      n32016_exec();
 
      if (tube_irq & RESET_BIT ) {
         // Reset the processor on active edge of rst
         // Exit if the copro has changed
         if (copro != last_copro) {
            break;
         }
         copro_32016_reset();
      }
      // NMI is edge sensitive, so only check after mailbox activity
      // Note: 32016 uses tube_irq directly, so no nmi code here   

      // IRQ is level sensitive, so check between every instruction
      // Note: 32016 uses tube_irq directly, so no irq code here
   }
}
