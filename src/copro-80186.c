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

static unsigned int last_nmi = 0;

int copro_80186_tube_read(uint16_t addr) {
  uint8_t data = tube_parasite_read(addr);
  //printf("rd %d %02x\r\n", addr, data);
  // Update NMI state as it may hve been cleared by the read
  if ((tube_irq & 2) == 0) {
     last_nmi = 0;
  }
  return data;
}

void copro_80186_tube_write(uint16_t addr, uint8_t data)	{
  tube_parasite_write(addr, data);
  //printf("wr %d %02x\r\n", addr, data);
  // Update NMI state as it may hve been cleared by the write
  if ((tube_irq & 2) == 0) {
     last_nmi = 0;
  }
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
         unsigned int irq = intr & 1;
         unsigned int nmi = intr & 2;
         unsigned int rst = intr & 4;
         // Reset the 6502 on a rst going inactive
         if (rst == 0 && last_rst != 0) {
            copro_80186_reset();
         }
         // IRQ is level sensitive
         if (irq && ifl) {
            //printf("irq!\r\n");
            intcall86(12);
         }
         // NMI is edge sensitive
         if (nmi != 0 && last_nmi == 0) {
            //printf("nmi!\r\n");
            intcall86(2);
         }
         last_nmi = nmi;
         last_rst = rst;
      }
   }
}
