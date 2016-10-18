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

int tracing=0;

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
  static unsigned int last_rst = 0;
  if (tube_mailbox & ATTN_MASK) {
    unsigned int tube_mailbox_copy = tube_mailbox;
    tube_mailbox &= ~(ATTN_MASK | OVERRUN_MASK);
    unsigned int intr = tube_io_handler(tube_mailbox_copy);
    unsigned int nmi = intr & 2;
    unsigned int rst = intr & 4;
    // Reset the processor on a rst going inactive
    if (rst && !last_rst) {
       // Exit if the copro has changed
       if (copro != last_copro) {
          return 1;
       }
      copro_lib6502_reset(mpu);
    }
    // NMI is edge sensitive, so only check after mailbox activity
    if (nmi) {
      M6502_nmi(mpu);
    }
    last_rst = rst;
   }
  // IRQ is level sensitive, so check between every instruction
  if (tube_irq & 1) {
     if (!(mpu->registers->p & 4)) {
        M6502_irq(mpu);
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
