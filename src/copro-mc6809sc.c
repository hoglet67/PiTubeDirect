/*
 * 6809 Co Pro Emulation based on Sean Conner's 6809 emulator
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

#include "copro-mc6809sc.h"

#include "mc6809sc/mc6809.h"
#include "tube-client.h"


static int overlay_rom = 0;

static unsigned char *copro_mc6809_ram;

static unsigned char *copro_mc6809_rom = tuberom_6809_jgh_1_0;

static int debug = 0;

static mc6809__t cpu;

static void copro_mc6809sc_write(mc6809__t *cpuptr, mc6809addr__t addr, mc6809byte__t data) {
   if ((addr & 0xFFF0) == 0xFEE0) {
      overlay_rom = 0;
      tube_parasite_write(addr & 7, data);
   } else {
      copro_mc6809_ram[addr & 0xffff] = data;
   }
   if (debug) {
      printf("Wr %04x=%02x\r\n", addr, data);
   }
}

static mc6809byte__t copro_mc6809sc_read(mc6809__t *cpuptr, mc6809addr__t addr, bool opcode_fetch) {
   mc6809byte__t data;
   if ((addr & 0xFFF0) == 0xFEE0) {
      overlay_rom = 0;
      data = tube_parasite_read(addr & 7);
   } else if (overlay_rom) {
      data = copro_mc6809_rom[addr & 0x7ff];
   } else {
      data = copro_mc6809_ram[addr & 0xffff];
   }
   if (debug) {
      printf("Rd %04x=%02x\r\n", addr, data);
   }
   return data;
}


static void copro_mc6809sc_fault(mc6809__t *cpuptr, mc6809fault__t fault) {
  
   switch (fault) {
   case MC6809_FAULT_INTERNAL_ERROR:
      printf("MC6809_FAULT_INTERNAL_ERROR\r\n");
      break;
   case  MC6809_FAULT_INSTRUCTION:
      printf(" MC6809_FAULT_INSTRUCTION\r\n");
      break;
   case MC6809_FAULT_ADDRESS_MODE:
      printf("MC6809_FAULT_ADDRESS_MODE\r\n");
      break;
   case MC6809_FAULT_EXG:
      printf("MC6809_FAULT_EXG\r\n");
      break;
   case MC6809_FAULT_TFR:
      printf("MC6809_FAULT_TFR\r\n");
      break;
   case MC6809_FAULT_user:
      printf("MC6809_FAULT_use\r\n");
      break;
   default:
      printf("undefined MC6809 fault, code=%d\r\n", fault);
   }
   printf("pc=%04x\r\n", cpuptr->instpc);
   printf("opcode=%02x\r\n", cpuptr->inst);
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
  mc6809_reset(&cpu);

  // Wait for rst become inactive before continuing to execute
  tube_wait_for_rst_release();

  // Reset ARM performance counters
  tube_reset_performance_counters();
}

void copro_mc6809sc_emulator()
{

   cpu.read = copro_mc6809sc_read;
   cpu.write = copro_mc6809sc_write;
   cpu.fault = copro_mc6809sc_fault;

   unsigned int last_rst = 0;

   // Remember the current copro so we can exit if it changes
   int last_copro = copro;

   copro_mc6809_poweron_reset(); 
   copro_mc6809_reset();
  
   while (1)
   {
      // Execute emulator for one instruction
      if (cpu.sync == 0) {
         mc6809_step(&cpu);
      }

      if (tube_irq &7) {
         cpu.sync = 0;
         unsigned int nmi = tube_irq & 2;
         unsigned int rst = tube_irq & 4;
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
            cpu.irq = true;
         }

         last_rst = rst;
      
         // IRQ is level sensitive, so check between every instruction
         cpu.firq = tube_irq & 1;
      }
   }
}
