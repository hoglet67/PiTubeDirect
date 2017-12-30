#include <stdio.h>
#include <string.h>
#include "tube-defs.h"
#include "tube.h"
#include "tube-ula.h"
#include "cpu80186/cpu80186.h"
#include "cpu80186/mem80186.h"
#include "startup.h"
#include "utils.h"

extern uint8_t Client86_v1_01[];

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
}

int copro_80186_tube_read(uint16_t addr) {
  return tube_parasite_read(addr);
}

void copro_80186_tube_write(uint16_t addr, uint8_t data) {
  tube_parasite_write(addr, data);
}

void copro_80186_emulator()
{
   unsigned int tube_irq_copy;

   // Remember the current copro so we can exit if it changes
   int last_copro = copro;

   copro_80186_poweron_reset(); 
   copro_80186_reset();
  
   while (1)
   {
      exec86(1);
      tube_irq_copy = tube_irq & ( RESET_BIT + NMI_BIT + IRQ_BIT) ;
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
            intcall86(2);
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
