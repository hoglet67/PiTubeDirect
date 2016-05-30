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
  // Dump tube buffer
  tube_dump_buffer();
  // Re-instate the Tube ROM on reset
  memcpy(mpu->memory + 0xf800, tuberom_6502_orig, 0x800);
  // Reset lib6502
  M6502_reset(mpu);
  // Do a tube reset
  tube_reset();
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

extern unsigned int tube_index;
extern unsigned int tube_buffer[];
extern int tube_triggered;

static void copro_lib6502_poll(M6502 *mpu) {
  static unsigned int last_rst = 0;
  if (tube_mailbox & ATTN_MASK) {
    unsigned int tube_mailbox_copy = tube_mailbox;
    tube_mailbox &= ~(ATTN_MASK | OVERRUN_MASK);
    unsigned int intr = tube_io_handler(tube_mailbox_copy);
    unsigned int nmi = intr & 2;
    unsigned int rst = intr & 4;
    // Reset the processor on a rst going inactive
    if (rst == 0 && last_rst != 0) {
      copro_lib6502_reset(mpu);
    }
    // NMI is edge sensitive, so only check after mailbox activity
    if (nmi) {
      M6502_nmi(mpu);
    }
    last_rst = rst;
   }
  if (tube_triggered != 0) {
     if (tube_triggered > 0) {
        tube_triggered--;
     }
     tube_buffer[tube_index++] = mpu->registers->pc;
     tube_index &= 0xffff;
  }
  // IRQ is level sensitive, so check between every instruction
  if (tube_irq & 1) {
     if (!(mpu->registers->p & 4)) {
        M6502_irq(mpu);
     }
  }
}

void copro_lib6502_emulator() {
  uint16_t addr;

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
