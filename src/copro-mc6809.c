/*
 * 6809 Co Pro Emulation based on XRoar 6809/6309 emulation
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
#include "tube-client.h"

#include "copro-mc6809.h"

#ifdef USE_HD6309
#include "mc6809/hd6309.h"
#else
#include "mc6809/mc6809.h"
#endif

static struct MC6809 *mc6809;

static int overlay_rom = 0;

static unsigned char *copro_mc6809_ram;

static unsigned char *copro_mc6809_rom = tuberom_6809_jgh_1_0;

static int debug = 0;

static void copro_mc6809_mem_cycle(void *sptr, _Bool rnw, uint16_t addr) {
   if (rnw) {
      // read cycle
      if ((addr & 0xFFF0) == 0xFEE0) {
         overlay_rom = 0;
         mc6809->D = tube_parasite_read(addr & 7);
      } else if (overlay_rom) {
         mc6809->D = copro_mc6809_rom[addr & 0x7ff];
      } else {
         mc6809->D = copro_mc6809_ram[addr & 0xffff];
      }
      if (debug) {
         printf("Rd %04x=%02x\r\n", addr, mc6809->D);
      }
   } else {
      // write cycle
      if ((addr & 0xFFF0) == 0xFEE0) {
         overlay_rom = 0;
         tube_parasite_write(addr & 7, mc6809->D);
      } else {
         copro_mc6809_ram[addr & 0xffff] = mc6809->D;
      }
      if (debug) {
         printf("Wr %04x=%02x\r\n", addr, mc6809->D);
      }
   }
}

static void copro_mc6809_poweron_reset() {
   // Initialize memory pointer to zero (the start of the 2MB of memory shared with the 6502)
   copro_mc6809_ram = copro_mem_reset(0x10000);
}

static void copro_mc6809_reset() {
  // Log ARM performance counters
  tube_log_performance_counters();

  // Re-instate the Tube ROM on reset
  overlay_rom = 1;

  // Reset the processor
  mc6809->reset(mc6809);

  // Wait for rst become inactive before continuing to execute
  tube_wait_for_rst_release();

  // Reset ARM performance counters
  tube_reset_performance_counters();
}

void copro_mc6809_emulator()
{

#ifdef USE_HD6309
   mc6809 = hd6309_new();
#else
   mc6809 = mc6809_new();
#endif

	mc6809->mem_cycle = DELEGATE_AS2(void, bool, uint16, copro_mc6809_mem_cycle, (void *)0);

   static unsigned int last_rst = 0;

   // Remember the current copro so we can exit if it changes
   int last_copro = copro;

   copro_mc6809_poweron_reset(); 
   copro_mc6809_reset();
  
   while (1)
   {
      // Execute emulator for one instruction
      mc6809->run(mc6809);

      MC6809_IRQ_SET(mc6809, 0);
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
            MC6809_IRQ_SET(mc6809, 1);
         }

         last_rst = rst;
      }
      // IRQ is level sensitive, so check between every instruction
      MC6809_FIRQ_SET(mc6809, tube_irq & 1);
   }
}
