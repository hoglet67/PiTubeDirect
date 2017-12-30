/*
 * OPC5LS Co Pro Emulation
 *
 * (c) 2017 David Banks
 */
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "tube-defs.h"
#include "tube-client.h"
#include "tube-ula.h"
#include "opc5ls/opc5ls.h"
#include "opc5ls/tuberom.h"
#include "copro-opc5ls.h"

#ifdef INCLUDE_DEBUGGER
#include "cpu_debug.h"
#include "opc5ls/opc5ls_debug.h"
#endif

static uint16_t *memory;

void copro_opc5ls_write(uint16_t addr, uint16_t data) {
#ifdef INCLUDE_DEBUGGER
   if (opc5ls_debug_enabled) {
      debug_memwrite(&opc5ls_cpu_debug, addr, data, 2);
   }
#endif
   if ((addr & 0xFFF8) == 0xFEF8) {
      tube_parasite_write(addr & 7, data);
      DBG_PRINT("write: %d = %x\r\n", addr & 7, data);
   } else {
      memory[addr] = data;
   }
}

uint16_t copro_opc5ls_read(uint16_t addr) {
   uint16_t data;
   if ((addr & 0xFFF8) == 0xFEF8) {
      data = tube_parasite_read(addr & 7);
      DBG_PRINT("read: %d = %x\r\n", addr & 7, data);
   } else {
      data = memory[addr];
   }
#ifdef INCLUDE_DEBUGGER
   if (opc5ls_debug_enabled) {
      debug_memread(&opc5ls_cpu_debug, addr, data, 2);
   }
#endif
   return data;
}

static void copro_opc5ls_poweron_reset() {
   // Initialize memory pointer to zero (the start of the 2MB of memory shared with the 6502)
   memory = (uint16_t *) copro_mem_reset(0x20000);

   // Initialize the CPU
   opc5ls_init(memory, 0xfffc, 0xfffe);

   // Copy over client ROM
   memcpy((void *) (memory + 0xF000), (void *)tuberom_opc5ls, sizeof(tuberom_opc5ls));
}

static void copro_opc5ls_reset() {
  // Log ARM performance counters
  tube_log_performance_counters();

  // Reset the processor
  opc5ls_reset();

  // Wait for rst become inactive before continuing to execute
  tube_wait_for_rst_release();

  // Reset ARM performance counters
  tube_reset_performance_counters();
}

void copro_opc5ls_emulator()
{
   unsigned int tube_irq_copy;

   // Remember the current copro so we can exit if it changes
   int last_copro = copro;

   copro_opc5ls_poweron_reset();
   copro_opc5ls_reset();

   while (1) {
      opc5ls_execute();
      DBG_PRINT("tube_irq = %d\r\n", tube_irq);
      tube_irq_copy = tube_irq & ( RESET_BIT + NMI_BIT + IRQ_BIT);
      if (tube_irq_copy) {
         // Reset the processor on active edge of rst
         if ( tube_irq_copy & RESET_BIT ) {
            // Exit if the copro has changed
            if (copro != last_copro) {
               break;
            }
            copro_opc5ls_reset();
         }
         // IRQ is level sensitive so check between every instruction
         if ( tube_irq_copy & IRQ_BIT ) {
            opc5ls_irq();
         }
      }
   }
}
