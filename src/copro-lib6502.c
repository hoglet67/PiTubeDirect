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
#include <inttypes.h>
#include <string.h>
#include "startup.h"
#include "cache.h"
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
  // Re-instate the Tube ROM on reset
  memcpy(mpu->memory + 0xf800, tuberom_6502_orig, 0x800);
  // Reset lib6502
  M6502_reset(mpu);
  // Do a tube reset
  tube_reset();
}

static unsigned int last_nmi = 0;

static int copro_lib6502_tube_read(M6502 *mpu, uint16_t addr, uint8_t data) {
  data = tube_parasite_read(addr);
  // Update NMI state as it may hve been cleared by the read
  if ((tube_irq & 2) == 0) {
     last_nmi = 0;
  }
  return data;
}

static int copro_lib6502_tube_write(M6502 *mpu, uint16_t addr, uint8_t data)	{
  tube_parasite_write(addr, data);
  // Update NMI state as it may hve been cleared by the write
  if ((tube_irq & 2) == 0) {
     last_nmi = 0;
  }
  return 0;
}

static void copro_lib6502_poll(M6502 *mpu) {
  static unsigned int last_rst = 0;
  if (tube_mailbox & ATTN_MASK) {
    unsigned int tube_mailbox_copy = tube_mailbox;
    tube_mailbox &= ~(ATTN_MASK | OVERRUN_MASK);
    unsigned int intr = tube_io_handler(tube_mailbox_copy);
    unsigned int irq = intr & 1;
    unsigned int nmi = intr & 2;
    unsigned int rst = intr & 4;
    // Reset the 6502 on a rst going inactive
    if (rst == 0 && last_rst != 0) {
      copro_lib6502_reset(mpu);
    }
    // IRQ is level sensitive
    if (irq) {
      if (!(mpu->registers->p & 4)) {
         //printf("irq!\r\n");
        M6502_irq(mpu);
      }
    }
    // NMI is edge sensitive
    if (nmi != 0 && last_nmi == 0) {
       //printf("nmi!\r\n");
      M6502_nmi(mpu);
    }
    last_nmi = nmi;
    last_rst = rst;
   }
}

void copro_lib6502_main() {
  uint16_t addr;

  tube_init_hardware();

  printf("Raspberry Pi Direct lib6502 Client\r\n" );

  enable_MMU_and_IDCaches();
  _enable_unaligned_access();

  // Lock the Tube Interrupt handler into cache
#ifndef RPI2 
  lock_isr();
#endif

  printf("Initialise UART console with standard libc\r\n" );

  _enable_interrupts();

  M6502 *mpu= M6502_new(0, 0, 0);

  for (addr= 0xfef8; addr <= 0xfeff; addr++) {
    M6502_setCallback(mpu, read,  addr, copro_lib6502_tube_read);
    M6502_setCallback(mpu, write, addr, copro_lib6502_tube_write);
  }

  copro_lib6502_poweron_reset(mpu);
  copro_lib6502_reset(mpu);

  M6502_run(mpu, copro_lib6502_poll);
  //M6502_run(mpu);
  M6502_delete(mpu);
}
