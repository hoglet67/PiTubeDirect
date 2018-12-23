/*
 * Arm2 Co Pro Emulation
 *
 * (c) 2015 David Banks
 * 
 * based on code by from MAME
 */


#include <stdio.h>
#include <string.h>
#include "tube-defs.h"
#include "tube.h"
#include "tube-ula.h"
#include "mame/arm.h"
#include "tuberom_arm.h"
#include "copro-arm2.h"
#include "startup.h"
#include "tube-client.h"

#ifdef INCLUDE_DEBUGGER
#include "cpu_debug.h"
#include "mame/arm_debug.h"
#endif

#define TUBE_ROM_ARM tuberom_arm_v100

#define RAM_MASK8    ((UINT32) 0x003fffff)
#define ROM_MASK8    ((UINT32) 0x00003fff)
#define RAM_MASK32   ((UINT32) 0x003ffffc)
#define ROM_MASK32   ((UINT32) 0x00003ffc)

// 4MB of RAM starting at 0x00000000
#define ARM_RAM_SIZE (1024 * 1024 * 4)
UINT8 * arm2_ram;

// 16KB of ROM starting at 0x03000000
//UINT8 arm2_rom[0x4000] __attribute__((aligned(0x10000)));

#define R15 arm2_getR15()

UINT8 copro_arm2_read8(int addr) {
   UINT8 result;

   if (addr <= RAM_MASK8) {
#if USE_MEMORY_POINTER       
      result = *(UINT8*) (arm2_ram + addr);
#else
      result = *(UINT8*) (addr);
#endif
   } else {
      int type = (addr >> 24) & 3;
      switch (type) {
      case 0:
#if USE_MEMORY_POINTER       
         result = *(UINT8*) (arm2_ram + (addr & RAM_MASK8));
#else
         result = *(UINT8*) ((addr & RAM_MASK8));
#endif
         break;
      case 1:
         result = tube_parasite_read((addr >> 2) & 7);
         break;
      case 3:
         result = *(UINT8*) (TUBE_ROM_ARM+(addr & ROM_MASK8));
         break;
      default:
         result = 0;
     }
   }
#ifdef INCLUDE_DEBUGGER
   if (arm2_debug_enabled) {
      debug_memread(&arm2_cpu_debug, addr, result, 4);
   }
#endif
   return result;
}

UINT32 copro_arm2_read32(int addr)
{
  UINT32 result;

   if ((addr & ~RAM_MASK32) == 0) {
#if USE_MEMORY_POINTER       
      result = *(UINT32*) (arm2_ram + addr);
#else
      result = *(UINT32*) (addr);
#endif
   } else {
      int type = (addr >> 24) & 3;
      switch (type) {
      case 0:
#if USE_MEMORY_POINTER       
         result = *(UINT32*) (arm2_ram + (addr & RAM_MASK32));
#else
         result = *(UINT32*) ((addr & RAM_MASK32));
#endif
         break;
      case 1:
         result = tube_parasite_read((addr >> 2) & 7);
         break;
      case 3:
         result = *(UINT32*) (TUBE_ROM_ARM+(addr & ROM_MASK32));
         break;
      default:
         result = 0;
      }
   }
   /* Unaligned reads rotate the word, they never combine words */
   if (addr & 3) {
      if (ARM_DEBUG_CORE && (addr & 1)) {
         logerror("%08x: Unaligned byte read %08x\n", R15, addr);
      }
      if ((addr & 3) == 1) {
         result = ((result & 0x000000ff) << 24) | ((result & 0xffffff00) >> 8);
      } else if ((addr & 3) == 2) {
         result =  ((result & 0x0000ffff) << 16) | ((result & 0xffff0000) >> 16);
      } else if ((addr & 3) == 3) {
         result = ((result & 0x00ffffff) << 8) | ((result & 0xff000000) >> 24);
      }
   }
#ifdef INCLUDE_DEBUGGER
   if (arm2_debug_enabled) {
      debug_memread(&arm2_cpu_debug, addr, result, 4);
   }
#endif
   return result;
}

void copro_arm2_write8(int addr, UINT8 data)
{
#ifdef INCLUDE_DEBUGGER
   if (arm2_debug_enabled) {
      debug_memwrite(&arm2_cpu_debug, addr, data, 1);
   }
#endif
   int type = (addr >> 24) & 3;
   switch (type) {
   case 0:
#if USE_MEMORY_POINTER       
      *(UINT8*) (arm2_ram + (addr & RAM_MASK8)) = data;
#else
      *(UINT8*) ( (addr & RAM_MASK8)) = data;
#endif
      break;
   case 1:
      tube_parasite_write((addr >> 2) & 7, data);
      break;
   }
}

void copro_arm2_write32(int addr, UINT32 data) {
#ifdef INCLUDE_DEBUGGER
   if (arm2_debug_enabled) {
      debug_memwrite(&arm2_cpu_debug, addr, data, 4);
   }
#endif
   int type = (addr >> 24) & 3;
   switch (type) {
   case 0:
#if USE_MEMORY_POINTER       
      *(UINT32*) (arm2_ram + (addr & RAM_MASK32)) = data;
#else
      *(UINT32*) ( (addr & RAM_MASK32)) = data;
#endif
      break;
   case 1:
      tube_parasite_write((addr >> 2) & 7, data);
      break;
   }
   /* Unaligned writes are treated as normal writes */
   if (addr & 3) {
      printf("%08x: Unaligned write %08x\n", R15, addr);
}
}

static void copro_arm2_poweron_reset() {
   // Wipe memory
   arm2_ram = copro_mem_reset(ARM_RAM_SIZE);
}

static void copro_arm2_reset() {
   // Log ARM performance counters
   tube_log_performance_counters();
   // Re-instate the reset vector on reset
   memcpy(arm2_ram, TUBE_ROM_ARM, 0x4);
   // Reset the ARM device
   arm2_device_reset();
   // Wait for rst become inactive before continuing to execute
   tube_wait_for_rst_release();
   // Reset ARM performance counters
   tube_reset_performance_counters();
}

void copro_arm2_emulator() {
   unsigned int tube_irq_copy;

   // Remember the current copro so we can exit if it changes
   int last_copro = copro;

   copro_arm2_poweron_reset();
   copro_arm2_reset();
  
   while (1)
   {
      arm2_execute_run(1);
      tube_irq_copy = tube_irq & ( RESET_BIT + NMI_BIT + IRQ_BIT);
      if (tube_irq_copy ) {
         // Reset the processor on active edge of rst
         if ( tube_irq_copy & RESET_BIT ) {
            // Exit if the copro has changed
            if (copro != last_copro) {
               break;
            }
            copro_arm2_reset();
         }
         
         // NMI is edge sensitive, so only check after mailbox activity
         if (tube_irq_copy & NMI_BIT) {
            arm2_execute_set_input(ARM_FIRQ_LINE, 1);
            tube_ack_nmi();
         }
         
         // IRQ is level sensitive, so check between every instruction
         arm2_execute_set_input(ARM_IRQ_LINE, tube_irq_copy & IRQ_BIT);
      }
      // IRQ is level sensitive, so check between every instruction
     // arm2_execute_set_input(ARM_IRQ_LINE, tube_irq & 1);
   }
}
