#include <stdio.h>
#include <string.h>
#include "tube-defs.h"
#include "tube.h"
#include "tube-ula.h"
#include "cpu80186/cpu80186.h"
#include "cpu80186/mem80186.h"

static void copro_80186_poweron_reset() {
   // Wipe memory
   Cleari80186Ram();
}

static void copro_80186_reset() {
  // Log ARM performance counters
  tube_log_performance_counters();
  // Re-instate the Tube ROM on reset
  RomCopy();
  // Reset cpu186
  reset();
  // Do a tube reset
  tube_reset();
  // Reset ARM performance counters
  tube_reset_performance_counters();
  // Log...
  //printf("reset 80186\r\n");
}

int copro_80186_tube_read(uint16_t addr) {
  return tube_parasite_read(addr);
}

void copro_80186_tube_write(uint16_t addr, uint8_t data)	{
  tube_parasite_write(addr, data);
}

void copro_80186_emulator()
{
   static unsigned int last_rst = 0;

   copro_80186_poweron_reset(); 
   copro_80186_reset();
  
   while (1)
   {
      exec86(1);

      if (tube_mailbox & ATTN_MASK) {
         unsigned int tube_mailbox_copy = tube_mailbox;
         tube_mailbox &= ~(ATTN_MASK | OVERRUN_MASK);
         unsigned int intr = tube_io_handler(tube_mailbox_copy);
         unsigned int nmi = intr & 2;
         unsigned int rst = intr & 4;
         // Reset the processor on a rst going inactive
         if (rst == 0 && last_rst != 0) {
            copro_80186_reset();
         }
         // NMI is edge sensitive, so only check after mailbox activity
         if (nmi) {
            intcall86(2);
         }
         last_rst = rst;
      }
      // IRQ is level sensitive, so check between every instruction
      if (tube_irq & 1) {
         if (ifl) {
            intcall86(12);
         }
      }
   }
}
