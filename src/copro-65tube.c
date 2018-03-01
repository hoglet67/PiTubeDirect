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
#include "tube-client.h"
#include "tube-defs.h"
#include "tube.h"
#include "tube-ula.h"
#include "tuberom_6502.h"
#include "programs.h"
#include "copro-65tube.h"
#include "cache.h"

#ifdef HISTOGRAM

unsigned int histogram_memory[0x100];

void copro_65tube_init_histogram() {
  int i;
  for (i = 0; i < 256; i++) {
    histogram_memory[i] = 0;
  }
}

void copro_65tube_dump_histogram() {
  int i;
  for (i = 0; i < 256; i++) {
    printf("%02x %u\r\n", i, histogram_memory[i]);
  }
}

#endif

static unsigned char *copro_65tube_poweron_reset(void) {
   // Wipe memory
   unsigned char * mpu_memory;
   mpu_memory = copro_mem_reset(0xF800); // only need to goto 0xF800 as rom will be put in later 
   // Install test programs (like sphere)
   copy_test_programs(mpu_memory);
   return mpu_memory;
}

static void copro_65tube_reset(unsigned char mpu_memory[]) {
   // Re-instate the Tube ROM on reset
   memcpy(mpu_memory + 0xf800, tuberom_6502_orig, 0x800);
   // Wait for rst become inactive before continuing to execute
   tube_wait_for_rst_release();
}

void copro_65tube_emulator() {
   // Remember the current copro so we can exit if it changes
   int last_copro = copro;
  // unsigned char *addr;
   //__attribute__ ((aligned (64*1024))) unsigned char mpu_memory[64*1024]; // allocate the amount of ram
   unsigned char * mpu_memory; // now the arm vectors have moved we can set the core memory to start at 0
   unsigned int i;
   // When the 65tube co pro on a single core system, switch to the alternative FIQ handler
   // that flag events from the ISR using the ip register

   mpu_memory = copro_65tube_poweron_reset();
   copro_65tube_reset(mpu_memory);
   
     // Make page 64K point to page 0 so that accesses LDA 0xFFFF, X work without needing masking
  map_4k_page(16, 0);

   while (copro == last_copro) {
#ifdef HISTOGRAM
      copro_65tube_init_histogram();
#endif
      tube_reset_performance_counters();
      exec_65tube(mpu_memory, (copro == COPRO_65TUBE_1) ?1:0);

      tube_log_performance_counters();
#ifdef HISTOGRAM
      copro_65tube_dump_histogram();
#endif
      copro_65tube_reset(mpu_memory);
   }
   
   // restore memory mapping

   for ( i= 0 ; i<=16; i++ )
     map_4k_page(i, i);
  
}
