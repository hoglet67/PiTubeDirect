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

extern volatile uint8_t tube_regs[];
 
// This is managed by the ISR to refect the current reset state
volatile int nRST;

volatile uint32_t tube_mailbox;

volatile uint32_t gpfsel_data_idle[3];
volatile uint32_t gpfsel_data_driving[3];
const uint32_t magic[3] = {MAGIC_C0, MAGIC_C1, MAGIC_C2 | MAGIC_C3 };

void assert_fail(uint32_t r0)
{
   printf("Assert fail: %08"PRIX32"\r\n", r0);   
}

void tube_io_handler(uint32_t mail)
{
   int addr;
   int data;
   int rnw;
   int ntube;
   int nrst;

   //printf("%08"PRIX32" %08"PRIX32"\r\n", r0, r1);

   addr = 0;
   if (mail & A0_MASK) {
      addr += 1;
   }
   if (mail & A1_MASK) {
      addr += 2;
   }
   if (mail & A2_MASK) {
      addr += 4;
   }
   data  = ((mail >> D0_BASE) & 0xF) | (((mail >> D4_BASE) & 0xF) << 4);
   rnw   = (mail >> RNW_PIN) & 1;
   ntube = (mail >> NTUBE_PIN) & 1;
   nrst  = (mail >> NRST_PIN) & 1;

   if (ntube == 0) {
      if (rnw == 0) {
         // host wrote to any tube reg (0..7)
         tube_regs[addr] = data;
      } else {
         // host reads from tube data reg (1, 3, 5, 7)
         // data has already been delivered, just need to deal with side effects here
         // TODO
      }
   } else {
      // probably reset has changed
      printf("nrst = %d\r\n", nrst);
   }
   
   printf("A=%d; D=%02X; RNW=%d; NTUBE=%d; nRST=%d\r\n", addr, data, rnw, ntube, nrst);
}

void copro_65tube_init_hardware()
{
   int i;
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
  RPI_SetGpioPinFunction(NRST_PIN, FS_INPUT);
  RPI_SetGpioPinFunction(RNW_PIN, FS_INPUT);

  // Configure GPIO to detect a falling edge of the NTUBE
  RPI_GpioBase->GPFEN0 |= NTUBE_MASK;

  // Make sure there are no pending detections
  RPI_GpioBase->GPEDS0 = NTUBE_MASK;

  // Enable gpio_int[0] which is IRQ 49
  RPI_GetIrqController()->Enable_IRQs_2 = (1 << (49 - 32));

  // Initialise the UART
  RPI_AuxMiniUartInit( 57600, 8 );

  for (i = 0; i < 3; i++) {
     gpfsel_data_idle[i] = (uint32_t) RPI_GpioBase->GPFSEL[i];
     gpfsel_data_driving[i] = gpfsel_data_idle[i] | magic[i];
     printf("%d %010o %010o\r\n", i, (unsigned int) gpfsel_data_idle[i], (unsigned int) gpfsel_data_driving[i]);
  }

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
     for (i = 0; i < 100000000; i++) {

        if (tube_mailbox & ATTN_MASK) {
           tube_io_handler(tube_mailbox);
           tube_mailbox &= ~ATTN_MASK;
        }

     }
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
