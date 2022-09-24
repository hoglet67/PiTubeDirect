// $Id$

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "tube-defs.h"
#include "tube-client.h"
#include "tube-ula.h"
#include "f100/f100.h"
#include "f100/tuberom.h"
#include "copro-f100.h"

#ifdef INCLUDE_DEBUGGER
#include "cpu_debug.h"
#include "f100/f100_debug.h"
#endif

static uint16_t *memory;

void copro_f100_write_mem(uint16_t addr, uint16_t data) {
#ifdef INCLUDE_DEBUGGER
   if (f100_debug_enabled) {
      debug_memwrite(&f100_cpu_debug, addr, data, 2);
   }
#endif
   if ((addr & 0x7FF8) == 0x7EF8) {
      tube_parasite_write(addr & 7, (uint8_t)data);
      DBG_PRINT("write: %d = %x\r\n", addr & 7, data);
   } else {
     memory[addr] = data;
   }
}

uint16_t copro_f100_read_mem(uint16_t addr) {
   uint16_t data;
   if ((addr & 0x7FF8) == 0x7EF8) {
      data = tube_parasite_read(addr & 7);
      DBG_PRINT("read: %d = %x\r\n", addr & 7, data);
   } else {
      data = memory[addr];
   }
#ifdef INCLUDE_DEBUGGER
   if (f100_debug_enabled) {
      debug_memread(&f100_cpu_debug, addr, data, 2);
   }
#endif
   return data;
}

static void copro_f100_poweron_reset() {
   // Initialize memory pointer to zero (the start of the 2MB of memory shared with the 6502)
   memory = (uint16_t *) copro_mem_reset(0x20000);

   // Initialize the CPU
   f100_init(memory, F100_PC_RST, 0xfffe, 0x0000);

   // Copy over client ROM
   copro_memcpy((void *) (memory + 0x0800), (void *)tuberom_f100, sizeof(tuberom_f100));
   copro_memcpy((void *) (memory + 0x7F00), (void *)tuberom_f100_high, sizeof(tuberom_f100_high));
}

static void copro_f100_reset() {
  // Log ARM performance counters
  tube_log_performance_counters();

  // Reset the processor
  f100_reset();

  // Wait for rst become inactive before continuing to execute
  tube_wait_for_rst_release();

  // Reset ARM performance counters
  tube_reset_performance_counters();
}

void copro_f100_emulator()
{
   // Remember the current copro so we can exit if it changes
   unsigned int last_copro = copro;

   copro_f100_poweron_reset();
   copro_f100_reset();

   while (1) {
      f100_execute();
      DBG_PRINT("tube_irq = %d\r\n", tube_irq);
      int tube_irq_copy = tube_irq & ( RESET_BIT + NMI_BIT + IRQ_BIT);
      if (tube_irq_copy) {
         // Reset the processor on active edge of rst
         if ( tube_irq_copy & RESET_BIT ) {
            // Exit if the copro has changed
            if (copro != last_copro) {
               break;
            }
            copro_f100_reset();
         }
         // IRQ is level sensitive so check between every instruction
         if ( tube_irq_copy & IRQ_BIT ) {
            f100_irq(0);
         }
      }
   }
}
