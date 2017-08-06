/*
 * OPC6 Co Pro Emulation
 *
 * (c) 2017 David Banks
 */
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "tube-defs.h"
#include "tube-client.h"
#include "tube-ula.h"
#include "opc6/opc6.h"
#include "opc6/tuberom.h"
#include "copro-opc6.h"

#ifdef INCLUDE_DEBUGGER
#include "cpu_debug.h"
#include "opc6/opc6_debug.h"
#endif

static uint16_t *memory;

void copro_opc6_write_mem(uint16_t addr, uint16_t data) {
#ifdef INCLUDE_DEBUGGER
   if (opc6_debug_enabled) {
      debug_memwrite(&opc6_cpu_debug, addr, data, 2);
   }
#endif
   memory[addr] = data;
}

uint16_t copro_opc6_read_mem(uint16_t addr) {
   uint16_t data = memory[addr];
#ifdef INCLUDE_DEBUGGER
   if (opc6_debug_enabled) {
      debug_memread(&opc6_cpu_debug, addr, data, 2);
   }
#endif
   return data;
}

void copro_opc6_write_io(uint16_t addr, uint16_t data) {
#ifdef INCLUDE_DEBUGGER
   if (opc6_debug_enabled) {
      debug_iowrite(&opc6_cpu_debug, addr, data, 2);
   }
#endif
   if ((addr & 0xFFF8) == 0xFEF8) {
      tube_parasite_write(addr & 7, data);
      DBG_PRINT("write: %d = %x\r\n", addr & 7, data);
   }
}

uint16_t copro_opc6_read_io(uint16_t addr) {
   uint16_t data = 0;
   if ((addr & 0xFFF8) == 0xFEF8) {
      data = tube_parasite_read(addr & 7);
      DBG_PRINT("read: %d = %x\r\n", addr & 7, data);
   }
#ifdef INCLUDE_DEBUGGER
   if (opc6_debug_enabled) {
      debug_ioread(&opc6_cpu_debug, addr, data, 2);
   }
#endif
   return data;
}

static void copro_opc6_poweron_reset() {
   // Initialize memory pointer to zero (the start of the 2MB of memory shared with the 6502)
   memory = (uint16_t *) copro_mem_reset(0x20000);

   // Initialize the CPU
   opc6_init(memory, 0xfffc, 0xfffe, 0x0000);

   // Copy over client ROM
   memcpy((void *) (memory + 0xF800), (void *)tuberom_opc6, sizeof(tuberom_opc6));
}

static void copro_opc6_reset() {
  // Log ARM performance counters
  tube_log_performance_counters();

  // Reset the processor
  opc6_reset();

  // Wait for rst become inactive before continuing to execute
  tube_wait_for_rst_release();

  // Reset ARM performance counters
  tube_reset_performance_counters();
}

void copro_opc6_emulator()
{
   unsigned int tube_irq_copy;

   // Remember the current copro so we can exit if it changes
   int last_copro = copro;

   copro_opc6_poweron_reset();
   copro_opc6_reset();

   while (1) {
      opc6_execute();
      DBG_PRINT("tube_irq = %d\r\n", tube_irq);
      tube_irq_copy = tube_irq & ( RESET_BIT + NMI_BIT + IRQ_BIT);
      if (tube_irq_copy) {
         // Reset the processor on active edge of rst
         if ( tube_irq_copy & RESET_BIT ) {
            // Exit if the copro has changed
            if (copro != last_copro) {
               break;
            }
            copro_opc6_reset();
         }
         // IRQ is level sensitive so check between every instruction
         if ( tube_irq_copy & IRQ_BIT ) {
            opc6_irq(0);
         }
      }
   }
}
