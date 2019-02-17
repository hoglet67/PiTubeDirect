/*
 * PDP11 Co Pro Emulation
 *
 * (c) 2018 David Banks
 */
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "tube-defs.h"
#include "tube-client.h"
#include "tube-ula.h"
#include "pdp11/pdp11.h"
#include "pdp11/tuberom.h"
#include "copro-pdp11.h"

#ifdef INCLUDE_DEBUGGER
#include "cpu_debug.h"
#include "pdp11/pdp11_debug.h"
#endif

static uint8_t *memory;

void copro_pdp11_write8(const uint16_t addr, const uint8_t data) {
#ifdef INCLUDE_DEBUGGER
   if (pdp11_debug_enabled) {
      debug_memwrite(&pdp11_cpu_debug, addr, data, 1);
   }
#endif
   if ((addr & 0xFFF0) == 0xFFF0) {
      tube_parasite_write((addr >> 1) & 7, data);
   } else {
      *(memory + addr) = data;
   }
}

uint8_t copro_pdp11_read8(const uint16_t addr) {
   uint8_t data;
   if ((addr & 0xFFF0) == 0xFFF0) {
      data = tube_parasite_read((addr >> 1) & 7);
   } else {
      data = *(memory + addr);
   }
#ifdef INCLUDE_DEBUGGER
   if (pdp11_debug_enabled) {
      debug_memread(&pdp11_cpu_debug, addr, data, 1);
   }
#endif
   return data;
}

void copro_pdp11_write16(const uint16_t addr, const uint16_t data) {
#ifdef INCLUDE_DEBUGGER
   if (pdp11_debug_enabled) {
      debug_memwrite(&pdp11_cpu_debug, addr, data, 2);
   }
#endif
   if ((addr & 0xFFF0) == 0xFFF0) {
      tube_parasite_write((addr >> 1) & 7, data & 255);
   } else {
      *(uint16_t *)(memory + addr) = data;
   }
}

uint16_t copro_pdp11_read16(const uint16_t addr) {
   uint16_t data;
   if ((addr & 0xFFF0) == 0xFFF0) {
      data = tube_parasite_read((addr >> 1) & 7);
   } else {
      data = *(uint16_t *)(memory + addr);
   }
#ifdef INCLUDE_DEBUGGER
   if (pdp11_debug_enabled) {
      debug_memread(&pdp11_cpu_debug, addr, data, 2);
   }
#endif
   return data;
}

static void copro_pdp11_poweron_reset() {
   // Initialize memory pointer to zero (the start of the 2MB of memory shared with the 6502)
   memory = copro_mem_reset(0x10000);
}

static void copro_pdp11_reset() {
   // Log ARM performance counters
   tube_log_performance_counters();

   // Copy over client ROM
   memcpy((void *) (memory + 0xF800), (void *)tuberom_pdp11, sizeof(tuberom_pdp11));

   // Reset the processor
   pdp11_reset(0xf800);

   // Wait for rst become inactive before continuing to execute
   tube_wait_for_rst_release();

   // Reset ARM performance counters
   tube_reset_performance_counters();
}

void copro_pdp11_emulator()
{
   unsigned int tube_irq_copy;

   // Remember the current copro so we can exit if it changes
   int last_copro = copro;

   copro_pdp11_poweron_reset();
   copro_pdp11_reset();

   while (1) {
      pdp11_execute();
      tube_irq_copy = tube_irq & ( RESET_BIT + NMI_BIT + IRQ_BIT) ;
      if (tube_irq_copy) {
         // Reset the processor on active edge of rst
         if (tube_irq_copy & RESET_BIT) {
            // Exit if the copro has changed
            if (copro != last_copro) {
               break;
            }
            copro_pdp11_reset();
         }

         // NMI is edge sensitive, so only check after mailbox activity
         if (tube_irq_copy & NMI_BIT) {
            pdp11_interrupt(0x80, 7);
            tube_ack_nmi();
         }

         // IRQ is level sensitive, so check between every instruction
         if (tube_irq_copy & IRQ_BIT) {
            if (((m_pdp11->PS >> 5) & 7) < 6) {
               pdp11_interrupt(0x84, 6);
            }
         }
      }
   }
}
