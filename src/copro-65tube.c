/*
 * 6502 Co Processor Emulation
 *
 * (c) 2016 David Banks and Ed Spittles
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
#include "programs.h"
#include "copro-65tube.h"
 
static void copro_65tube_poweron_reset() {
   // Wipe memory
   memset(mpu_memory, 0, 0x10000);
   // Install test programs (like sphere)
   copy_test_programs(mpu_memory);
}
static void copro_65tube_reset() {
   // Re-instate the Tube ROM on reset
   memcpy(mpu_memory + 0xf800, tuberom_6502_orig, 0x800);
   // Do a tube reset
   tube_reset();
}

void copro_65tube_emulator() {
   // Remember the current copro so we can exit if it changes
   int last_copro = copro;

   copro_65tube_poweron_reset();
   copro_65tube_reset();

   while (copro == last_copro) {
      tube_reset_performance_counters();
      exec_65tube(mpu_memory);
      tube_log_performance_counters();
      copro_65tube_reset();
      tube_wait_for_rst_release();
   }
}

