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

#define TEST_MODE 0

#include <stdio.h>
#include <inttypes.h>
#include <string.h>
#include "startup.h"
#include "rpi-aux.h"
#include "cache.h"
#include "tube.h"
#include "tube-ula.h"
#include "tuberom_6502.h"
#include "programs.h"
#include "performance.h"
#include "copro-65tube.h"
 
#if TEST_MODE
static void tube_reset_and_write_test_string() {
   int i;
   char testmessage[] = "Ed and Dave's Pi Tube";
   // Reset the tube emulation
   copro_65tube_tube_reset();
   // Write a fake startup message to the Reg 1 (PH1) on reset
   for (i = 0; i <= strlen(testmessage); i++) {
      copro_65tube_tube_write(1, testmessage[i]);
   }
}
#else
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
#endif


void copro_65tube_emulator() {
   perf_counters_t pct;

#if defined(RPI2) || defined(RPI3) 
   pct.num_counters = 6;
   pct.type[0] = PERF_TYPE_L1I_CACHE;
   pct.type[1] = PERF_TYPE_L1I_CACHE_REFILL;
   pct.type[2] = PERF_TYPE_L1D_CACHE;
   pct.type[3] = PERF_TYPE_L1D_CACHE_REFILL;
   pct.type[4] = PERF_TYPE_L2D_CACHE_REFILL;
   pct.type[5] = PERF_TYPE_INST_RETIRED;
   pct.counter[0] = 100;
   pct.counter[1] = 101;
   pct.counter[2] = 102;
   pct.counter[3] = 103;
   pct.counter[4] = 104;
   pct.counter[5] = 105;
#else
   pct.num_counters = 2;
   pct.type[0] = PERF_TYPE_I_CACHE_MISS;
   pct.type[1] = PERF_TYPE_D_CACHE_MISS;
#endif

#if TEST_MODE
  // Fake a startup message
  tube_reset_and_write_test_string();
  while (1) {
     if (tube_mailbox & ATTN_MASK) {
        if (tube_io_handler(tube_mailbox) & 4) {
           // A reset has been detected
           tube_reset_and_write_test_string();
        }
        tube_mailbox &= ~ATTN_MASK;
     }
  }
#else
  benchmark();
  copro_65tube_poweron_reset();
  // This is the proper 6502 emulation
  while (1) {
    // Reinitialize the 6502 memory
    copro_65tube_reset();
    // log...
    //printf("starting 6502\r\n");
    // Start executing code, this will return when reset goes low
    reset_performance_counters(&pct);
    exec_65tube(mpu_memory);
    read_performance_counters(&pct);
    print_performance_counters(&pct);
    // log...
    //printf("stopping 6502\r\n");
    // wait for nRST to be released
    tube_wait_for_rst_release();
  }
#endif
}

#if defined(RPI2) || defined(RPI3)
void run_core() {
   int i;
   // Write first line without using printf
   // In case the VFP unit is not enabled
	RPI_AuxMiniUartWrite('C');
	RPI_AuxMiniUartWrite('O');
	RPI_AuxMiniUartWrite('R');
	RPI_AuxMiniUartWrite('E');
   i = _get_core();
	RPI_AuxMiniUartWrite('0' + i);
	RPI_AuxMiniUartWrite('\r');
	RPI_AuxMiniUartWrite('\n');
   
   enable_MMU_and_IDCaches();
   _enable_unaligned_access();
   
   printf("test running on core %d\r\n", i);
   copro_65tube_emulator();
}

typedef void (*func_ptr)();

void start_core(int core, func_ptr func) {
   printf("starting core %d\r\n", core);
   *(unsigned int *)(0x4000008C + 0x10 * core) = (unsigned int) func;
}
#endif


void copro_65tube_main() {
  volatile int i;
  tube_init_hardware();

  printf("Raspberry Pi Direct 65Tube Client\r\n" );

  enable_MMU_and_IDCaches();
  _enable_unaligned_access();

  // Lock the Tube Interrupt handler into cache
#if !defined(RPI2) && !defined(RPI3)
  lock_isr();
#endif

  printf("Initialise UART console with standard libc\r\n" );

  _enable_interrupts();

#if defined(RPI2) || defined(RPI3)

  printf("main running on core %d\r\n", _get_core());
  for (i = 0; i < 100000000; i++);
  start_core(1, _spin_core);

  for (i = 0; i < 100000000; i++);
  start_core(2, _spin_core);

  for (i = 0; i < 100000000; i++);

#ifdef MULTICORE
  start_core(3, _init_core);
  while (1);
#else
  start_core(3, _spin_core);
  for (i = 0; i < 100000000; i++);
  copro_65tube_emulator();

#endif

#else

  copro_65tube_emulator();

#endif
}
