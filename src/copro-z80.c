/*
 * Z80 Co Pro Emulation
 *
 * (c) 2016 David Banks
 */
#include <stdio.h>

#include "tube.h"
#include "tube-ula.h"
#include "yaze/simz80.h"
#include "tube-client.h"
#include "tuberom_z80.h"
#include "utils.h"

#ifdef INCLUDE_DEBUGGER
#include "cpu_debug.h"
#endif

static int overlay_rom = 0;

static unsigned char *copro_z80_ram;

static unsigned char *copro_z80_rom;

uint8_t copro_z80_read_mem(unsigned int addr) {
   uint8_t  data;
   if (addr >= 0x8000) {
      overlay_rom = 0;
   }
   if (overlay_rom) {
      data = copro_z80_rom[addr & 0xfff];
   } else {
#ifdef USE_MEMORY_POINTER
      data = copro_z80_ram[addr & 0xffff];
#else
      data = *(unsigned char *)(addr & 0xffff);
#endif
   }
#ifdef INCLUDE_DEBUGGER
   if (simz80_debug_enabled) {
      debug_memread(&simz80_cpu_debug, addr, data, 1);
   }
#endif
   return data;
}

void copro_z80_write_mem(unsigned int addr, unsigned char data) {
#ifdef INCLUDE_DEBUGGER
   if (simz80_debug_enabled) {
      debug_memwrite(&simz80_cpu_debug, addr, data, 1);
   }
#endif
#ifdef USE_MEMORY_POINTER
   copro_z80_ram[addr & 0xffff] = data;
#else
   *(unsigned char *)(addr & 0xffff) = data;
#endif
}

uint8_t copro_z80_read_io(unsigned int addr) {
   uint8_t data =  tube_parasite_read(addr & 7);
#ifdef INCLUDE_DEBUGGER
   if (simz80_debug_enabled) {
      debug_ioread(&simz80_cpu_debug, addr, data, 1);
   }
#endif
   return data;
}

void copro_z80_write_io(unsigned int addr, unsigned char data) {
#ifdef INCLUDE_DEBUGGER
   if (simz80_debug_enabled) {
      debug_iowrite(&simz80_cpu_debug, addr, data, 1);
   }
#endif
   tube_parasite_write(addr & 7, data);
}

static void copro_z80_reset() {
  // Log ARM performance counters
  tube_log_performance_counters();

  // Re-instate the Tube ROM on reset
  overlay_rom = 1;

  // Reset the processor
  simz80_reset();

  // Wait for rst become inactive before continuing to execute
  tube_wait_for_rst_release();

  // Reset ARM performance counters
  tube_reset_performance_counters();
}

void copro_z80_emulator()
{
   // Remember the current copro so we can exit if it changes
   unsigned int last_copro = copro;

   // Setup the client ROM appropriately from the Co Pro number
   switch (copro & 3) {
   case 0: copro_z80_rom = tuberom_z80_1_21; break;
   case 1: copro_z80_rom = tuberom_z80_2_00; break;
   case 2: copro_z80_rom = tuberom_z80_2_2c; break;
   case 3: copro_z80_rom = tuberom_z80_2_30; break;
   }

   // Patch the OSWORD &FF code to change FEE5 to FCE5 (2 changes expected)
   check_elk_mode_and_patch(copro_z80_rom, 0xD30, 0xAA, 2);

   copro_z80_ram = copro_mem_reset(0x10000);
   copro_z80_reset();

   while (1)
   {
      int tube_irq_copy;
      // Execute emulator for one instruction
      simz80_execute(1);
      tube_irq_copy = tube_irq & ( RESET_BIT + NMI_BIT + IRQ_BIT );
      if (tube_irq_copy) {
         // Reset the processor on active edge of rst
         if (tube_irq_copy & RESET_BIT) {
            // Exit if the copro has changed
            if (copro != last_copro) {
               break;
            }
            copro_z80_reset();
         }

         // NMI is edge sensitive,
         if (tube_irq_copy & NMI_BIT) {
            overlay_rom = 1;
            simz80_NMI();
            tube_ack_nmi();
         }

         // IRQ is level sensitive,
         if (tube_irq_copy & IRQ_BIT) {
            // check if the emulator IRQ is enabled
            if (simz80_is_IRQ_enabled()) {
               simz80_IRQ();
            }
         }
      }
   }
}
