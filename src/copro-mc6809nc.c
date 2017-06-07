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
#include "tube-client.h"

#ifdef INCLUDE_DEBUGGER
#include "cpu_debug.h"
#include "mc6809nc/mc6809_debug.h"
#endif

static int overlay_rom = 0;

static unsigned char *copro_mc6809_ram;

static unsigned char *copro_mc6809_rom = tuberom_6809_jgh_1_0;

void copro_mc6809nc_write(uint16_t addr, uint8_t data) {
#ifdef INCLUDE_DEBUGGER
   if (mc6809nc_debug_enabled)
   {
      debug_memwrite(&mc6809nc_cpu_debug, addr, data, 1);
   }
#endif
   if ((addr & 0xFFF0) == 0xFEE0) {
      overlay_rom = 0;
      tube_parasite_write(addr & 7, data);
   } else {
#ifdef USE_MEMORY_POINTER
      copro_mc6809_ram[addr & 0xffff] = data;
#else 
      *(unsigned char *)(addr & 0xffff) = data;
#endif
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
#if USE_MEMORY_POINTER       
      data = copro_mc6809_ram[addr & 0xffff];
#else
      data = *(unsigned char *)(addr & 0xffff);
#endif
   }
#ifdef INCLUDE_DEBUGGER
   if (mc6809nc_debug_enabled)
   {
      debug_memread(&mc6809nc_cpu_debug, addr, data, 1);
   }
#endif
   return data;
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
  mc6809nc_reset();

  // Wait for rst become inactive before continuing to execute
  tube_wait_for_rst_release();

  // Reset ARM performance counters
  tube_reset_performance_counters();
}

void copro_mc6809nc_emulator()
{
   unsigned int tube_irq_copy;
   
   // Remember the current copro so we can exit if it changes
   int last_copro = copro;

   copro_mc6809_poweron_reset(); 
   copro_mc6809_reset();
  
   while (1)
   {
      // Execute emulator for one instruction
      mc6809nc_execute(1);
      tube_irq_copy = tube_irq & ( RESET_BIT + NMI_BIT + IRQ_BIT);
      if (tube_irq_copy) {
         // Reset the processor on active edge of rst
         if ( tube_irq_copy & RESET_BIT ) {
            // Exit if the copro has changed
            if (copro != last_copro) {
               break;
            }
            copro_mc6809_reset();
         }

         // NMI is edge sensitive, so only check after mailbox activity
         if ( tube_irq_copy & NMI_BIT ) {
            mc6809nc_request_irq(1);
            tube_ack_nmi();
         }

      // IRQ is level sensitive, so check between every instruction
         if ( tube_irq_copy & IRQ_BIT ) {
            mc6809nc_request_firq(1);
         }
      }
   }
}
