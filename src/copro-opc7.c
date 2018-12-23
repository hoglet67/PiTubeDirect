/*
 * OPC7 Co Pro Emulation
 *
 * (c) 2017 Ed Spittles after David Banks
 */
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "tube-defs.h"
#include "tube-client.h"
#include "tube-ula.h"
#include "opc7/opc7.h"
#include "opc7/tuberom.h"
#include "copro-opc7.h"

#ifdef INCLUDE_DEBUGGER
#include "cpu_debug.h"
#include "opc7/opc7_debug.h"
#endif

static uint32_t *memory;

void copro_opc7_write_mem(uint32_t addr, uint32_t data) {
   addr &= 0xFFFFF;
#ifdef INCLUDE_DEBUGGER
   if (opc7_debug_enabled) {
      debug_memwrite(&opc7_cpu_debug, addr, data, 4);
   }
#endif
   memory[addr] = data;
}

uint32_t copro_opc7_read_mem(uint32_t addr) {
   addr &= 0xFFFFF;
   uint32_t data = memory[addr];
#ifdef INCLUDE_DEBUGGER
   if (opc7_debug_enabled) {
      debug_memread(&opc7_cpu_debug, addr, data, 4);
   }
#endif
   return data;
}

void copro_opc7_write_io(uint32_t addr, uint32_t data) {
#ifdef INCLUDE_DEBUGGER
   if (opc7_debug_enabled) {
      debug_iowrite(&opc7_cpu_debug, addr, data, 2);
   }
#endif
   if ((addr & 0xFFF8) == 0xFEF8) {
      tube_parasite_write(addr & 7, data);
      DBG_PRINT("write: %u = %x\r\n", addr & 7, data);
   }
}

uint32_t copro_opc7_read_io(uint32_t addr) {
   uint32_t data = 0;
   if ((addr & 0xFFF8) == 0xFEF8) {
      data = tube_parasite_read(addr & 7);
      DBG_PRINT("read: %u = %x\r\n", addr & 7, data);
   }
#ifdef INCLUDE_DEBUGGER
   if (opc7_debug_enabled) {
      debug_ioread(&opc7_cpu_debug, addr, data, 2);
   }
#endif
   return data;
}

static void copro_opc7_poweron_reset() {
   // Initialize memory pointer to zero (the start of the 2MB of memory shared with the 6502)
   memory = (uint32_t *) copro_mem_reset(0x20000);

   // Initialize the CPU
   opc7_init(memory, 0x0000, 0x0002, 0x0000);

   // Copy over client ROM
   memcpy((void *) (memory + 0x0000), (void *)tuberom_opc7, sizeof(tuberom_opc7));
}

static void copro_opc7_reset() {
  // Log ARM performance counters
  tube_log_performance_counters();

  // Reset the processor
  opc7_reset();

  // Wait for rst become inactive before continuing to execute
  tube_wait_for_rst_release();

  // Reset ARM performance counters
  tube_reset_performance_counters();
}

void copro_opc7_emulator()
{
   unsigned int tube_irq_copy;

   // Remember the current copro so we can exit if it changes
   int last_copro = copro;

   copro_opc7_poweron_reset();
   copro_opc7_reset();

   while (1) {
      opc7_execute();
      DBG_PRINT("tube_irq = %d\r\n", tube_irq);
      tube_irq_copy = tube_irq & ( RESET_BIT + NMI_BIT + IRQ_BIT);
      if (tube_irq_copy) {
         // Reset the processor on active edge of rst
         if ( tube_irq_copy & RESET_BIT ) {
            // Exit if the copro has changed
            if (copro != last_copro) {
               break;
            }
            copro_opc7_reset();
         }
         // IRQ is level sensitive so check between every instruction
         if ( tube_irq_copy & IRQ_BIT ) {
            opc7_irq(0);
         }
      }
   }
}
