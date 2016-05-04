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
#include <inttypes.h>
#include <string.h>
#include "startup.h"
#include "cache.h"
#include "rpi-gpio.h"
#include "rpi-aux.h"
#include "rpi-interrupts.h"
#include "tuberom_6502.h"
#include "copro-65tube.h"
#include "tube-defs.h"

// This is managed by the ISR to refect the current reset state
volatile int nRST;

void assert_fail(uint32_t r0)
{
   printf("Assert fail: %08"PRIX32"\r\n", r0);   
}

void temp_tube_io_handler(uint32_t r0,  uint32_t r1)
{
   int addr;
   int data;
   int phi2;
   int rnw;
   int ntube;
   int nrst;

   printf("%08"PRIX32" %08"PRIX32"\r\n", r0, r1);

   addr = 0;
   if (r1 & A0_MASK) {
      addr += 1;
   }
   if (r1 & A1_MASK) {
      addr += 2;
   }
   if (r1 & A2_MASK) {
      addr += 4;
   }
   data  = ((r1 >> D0_BASE) & 0xF) | (((r1 >> D4_BASE) & 0xF) << 4);
   phi2  = (r1 >> PHI2_PIN) & 1;
   rnw   = (r1 >> RNW_PIN) & 1;
   ntube = (r1 >> NTUBE_PIN) & 1;
   nrst  = (r1 >> NRST_PIN) & 1;
   
   printf("A=%d; D=%02X; PHI2=%d; RNW=%d; NTUBE=%d; nRST=%d\r\n",
          addr, data, phi2, rnw, ntube, nrst);
}

void copro_65tube_init_hardware()
{
  // Write 1 to the LED init nibble in the Function Select GPIO
  // peripheral register to enable LED pin as an output
  RPI_GpioBase->LED_GPFSEL |= LED_GPFBIT;

  // Configure our pins as inputs
  RPI_SetGpioPinFunction(D7_PIN, FS_INPUT);
  RPI_SetGpioPinFunction(D6_PIN, FS_INPUT);
  RPI_SetGpioPinFunction(D5_PIN, FS_INPUT);
  RPI_SetGpioPinFunction(D4_PIN, FS_INPUT);
  RPI_SetGpioPinFunction(D3_PIN, FS_INPUT);
  RPI_SetGpioPinFunction(D2_PIN, FS_INPUT);
  RPI_SetGpioPinFunction(D1_PIN, FS_INPUT);
  RPI_SetGpioPinFunction(D0_PIN, FS_INPUT);

  RPI_SetGpioPinFunction(A2_PIN, FS_INPUT);
  RPI_SetGpioPinFunction(A1_PIN, FS_INPUT);
  RPI_SetGpioPinFunction(A0_PIN, FS_INPUT);
  RPI_SetGpioPinFunction(PHI2_PIN, FS_INPUT);
  RPI_SetGpioPinFunction(NTUBE_PIN, FS_INPUT);
//  RPI_SetGpioPinFunction(NRST_PIN, FS_INPUT);
  RPI_SetGpioPinFunction(NRST_PIN, FS_OUTPUT);
  RPI_SetGpioPinFunction(RNW_PIN, FS_INPUT);

  // Configure GPIO to detect a falling edge of the NTUBE
  RPI_GpioBase->GPFEN0 |= NTUBE_MASK;

  // Make sure there are no pending detections
  RPI_GpioBase->GPEDS0 = NTUBE_MASK;

  // Enable gpio_int[0] which is IRQ 49
  RPI_GetIrqController()->Enable_IRQs_2 = (1 << (49 - 32));

  // Initialise the UART
  RPI_AuxMiniUartInit( 57600, 8 );

}

//static void copro_65tube_reset() {
//  // Wipe memory
//  memset(mpu_memory, 0, 0x10000);
//  // Re-instate the Tube ROM on reset
//  memcpy(mpu_memory + 0xf800, tuberom_6502_orig, 0x800);
//}


void copro_65tube_main() {
   int i;
   // int count;
   int led = 0;
  copro_65tube_init_hardware();

  printf("Raspberry Pi Direct 65Tube Client\r\n" );

  enable_MMU_and_IDCaches();
  _enable_unaligned_access();

  printf("Initialise UART console with standard libc\r\n" );

  _enable_interrupts();

  //count = 0;
  while (1) {
     for (i = 0; i < 100000000; i++);
     if (led) {
        LED_OFF();
     } else {
        LED_ON();
     }
     led = ~led;
     // printf("tick %d\r\n", count++);

    // Wait for reset to go high - nRST is updated by the ISR
    // while (nRST == 0);
    // printf("RST!\r\n");
    // Reinitialize the 6502 memory
    // copro_65tube_reset();
    // Start executing code, this will return when reset goes low
    // exec_65tube(mpu_memory);
    // Dump memory
    // copro_65tube_dump_mem(0x0000, 0x0400);
  }
}
