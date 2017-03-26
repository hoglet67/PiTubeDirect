/*
 * 6502 Co Processor Emulation
 *
 * (c) 2015 David Banks
 * 
 * based on code by
 * - Reuben Scratton.
 * - Tom Walker
 *
 */

#include <stdio.h>
#include <string.h>
#include "tube-defs.h"
#include "tube.h"
#include "tube-ula.h"
#include "tuberom_6502.h"
#include "lib6502.h"
#include "programs.h"
#include "copro-lib6502.h"
#include "startup.h"

const int tracing=0;

static void copro_lib6502_poweron_reset(M6502 *mpu) {
  // Wipe memory
  memset(mpu->memory, 0, 0x10000);
  // Install test programs (like sphere)
  copy_test_programs(mpu->memory);
}

static void copro_lib6502_reset(M6502 *mpu) {
  // Log ARM performance counters
  tube_log_performance_counters();
  // Re-instate the Tube ROM on reset
  memcpy(mpu->memory + 0xf800, tuberom_6502_orig, 0x800);
  // Reset lib6502
  M6502_reset(mpu);
  // Wait for rst become inactive before continuing to execute
  tube_wait_for_rst_release();
  // Reset ARM performance counters
  tube_reset_performance_counters();
}

static int copro_lib6502_tube_read(M6502 *mpu, uint16_t addr, uint8_t data) {
  return tube_parasite_read(addr);
}

static int copro_lib6502_tube_write(M6502 *mpu, uint16_t addr, uint8_t data)	{
  tube_parasite_write(addr, data);
  return 0;
}

static int last_copro;

static int copro_lib6502_poll(M6502 *mpu) {
   unsigned int tube_irq_copy;
   tube_irq_copy = tube_irq & ( RESET_BIT + NMI_BIT + IRQ_BIT );
   if (tube_irq_copy) {
      // Reset the processor on a rst going inactive
      if ( tube_irq_copy & RESET_BIT ) {
         // Exit if the copro has changed
         if (copro != last_copro) {
            return 1;
         }
         copro_lib6502_reset(mpu);
      }

      // NMI is edge sensitive, so only check after mailbox activity
      if ( tube_irq_copy & NMI_BIT) {
         M6502_nmi(mpu);
         tube_ack_nmi();
      }
      
      // IRQ is level sensitive, so check between every instruction
      if (tube_irq_copy & IRQ_BIT) {
         if (!(mpu->registers->p & 4)) {
            M6502_irq(mpu);
         }
      }
   }
   return 0;
}

void copro_lib6502_emulator() {
  uint16_t addr;

  // Remember the current copro so we can exit if it changes
  last_copro = copro;

  M6502 *mpu= M6502_new(0, 0, 0);

  for (addr= 0xfef8; addr <= 0xfeff; addr++) {
    M6502_setCallback(mpu, read,  addr, copro_lib6502_tube_read);
    M6502_setCallback(mpu, write, addr, copro_lib6502_tube_write);
  }

  copro_lib6502_poweron_reset(mpu);
  copro_lib6502_reset(mpu);

  M6502_run(mpu, copro_lib6502_poll);

  M6502_delete(mpu);
}
