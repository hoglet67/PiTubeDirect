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
#include "cache.h"
#include "tube.h"
#include "tube-ula.h"
#include "tuberom_6502.h"
#include "copro-65tube.h"
 
#if TEST_MODE
extern volatile uint32_t events;
#endif

volatile int memory[1000000];

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
static void copro_65tube_reset() {
  // Wipe memory
  memset(mpu_memory, 0, 0x10000);
  // Re-instate the Tube ROM on reset
  memcpy(mpu_memory + 0xf800, tuberom_6502_orig, 0x800);
  // Do a tube reset
  tube_reset();
}
#endif

void copro_65tube_main() {

  tube_init_hardware();

  printf("Raspberry Pi Direct 65Tube Client\r\n" );

  enable_MMU_and_IDCaches();
  _enable_unaligned_access();

  // Lock the Tube Interrupt handler into cache
  lock_isr();

  printf("Initialise UART console with standard libc\r\n" );

  _enable_interrupts();

#if TEST_MODE
  // Fake a startup message
  tube_reset_and_write_test_string();
  while (1) {
     if (events & 0x80000000) {
        if (tube_io_handler(events) & 4) {
           // A reset has been detected
           tube_reset_and_write_test_string();
        }
        events &= ~0x80000000;
     }
  }
#else
  // This is the proper 6502 emulation
  while (1) {
    // Reinitialize the 6502 memory
    copro_65tube_reset();
    // log...
    //printf("starting 6502\r\n");
    // Start executing code, this will return when reset goes low
    exec_65tube(mpu_memory);
    // log...
    //printf("stopping 6502\r\n");
    // wait for nRST to be released
    tube_wait_for_rst_release();
  }
#endif
}
