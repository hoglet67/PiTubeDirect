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
#include "tube.h"
#include "tube-ula.h"
 
// This is managed by the ISR to refect the current reset state
volatile int nRST;

extern volatile uint8_t tube_regs[];

extern volatile uint32_t events;

extern volatile uint32_t gpfsel_data_idle[3];
extern volatile uint32_t gpfsel_data_driving[3];
const uint32_t magic[3] = {MAGIC_C0, MAGIC_C1, MAGIC_C2 | MAGIC_C3 };

volatile int memory[1000000];
int a;

// Bit 0 is the tube asserting irq
// Bit 1 is the tube asserting nmi
extern int tube_irq;

// Returns bit 0 set if IRQ is asserted by the tube
// Returns bit 1 set if NMI is asserted by the tube
// Returns bit 2 set if RST is asserted by the host or tube

int tube_io_handler(uint32_t mail)
{
   int addr;
   int data;
   int rnw;
   int ntube;
   int nrst;

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

   if (nrst == 1 && ntube == 0) {
      if (rnw == 0) {
         copro_65tube_host_write(addr, data);
      } else {
         copro_65tube_host_read(addr);
      }
   }

   //printf("A=%d; D=%02X; RNW=%d; NTUBE=%d; nRST=%d\r\n", addr, data, rnw, ntube, nrst);
   
   if (nrst == 0 || (tube_regs[0] & 0x20)) {
      return tube_irq | 4;
   } else {
      return tube_irq;
   }
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

  // Configure GPIO to detect a falling edge of NTUBE and NRST
  RPI_GpioBase->GPFEN0 |= NTUBE_MASK | NRST_MASK;

  // Configure GPIO to detect a rising edge of NRST
  RPI_GpioBase->GPREN0 |= NRST_MASK;

  // Make sure there are no pending detections
  RPI_GpioBase->GPEDS0 = NTUBE_MASK | NRST_MASK;

  // This line enables IRQ interrupts
  // Enable gpio_int[0] which is IRQ 49
  //RPI_GetIrqController()->Enable_IRQs_2 = (1 << (49 - 32));

  // This line enables FIQ interrupts
  // Enable gpio_int[0] which is IRQ 49 as FIQ
  RPI_GetIrqController()->FIQ_control = 0x80 + 49;

  // Initialise the UART
  RPI_AuxMiniUartInit( 57600, 8 );

  for (i = 0; i < 3; i++) {
     gpfsel_data_idle[i] = (uint32_t) RPI_GpioBase->GPFSEL[i];
     gpfsel_data_driving[i] = gpfsel_data_idle[i] | magic[i];
     printf("%d %010o %010o\r\n", i, (unsigned int) gpfsel_data_idle[i], (unsigned int) gpfsel_data_driving[i]);
  }

}

static void copro_65tube_reset() {
  // Wipe memory
  memset(mpu_memory, 0, 0x10000);
  // Re-instate the Tube ROM on reset
  memcpy(mpu_memory + 0xf800, tuberom_6502_orig, 0x800);
  // Do a tube reset
  copro_65tube_tube_reset();
}


void copro_65tube_main() {

  copro_65tube_init_hardware();

  printf("Raspberry Pi Direct 65Tube Client\r\n" );

  enable_MMU_and_IDCaches();
  _enable_unaligned_access();

  // Lock the Tube Interrupt handler into cache
  lock_isr();

  printf("Initialise UART console with standard libc\r\n" );

#if 0 
  _enable_interrupts();
  while (1) {
     if (events & 0x80000000) {
        tube_io_handler(events);
        events &= ~0x80000000;
     }
  }
#endif
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
    if ((RPI_GpioBase->GPLEV0 & NRST_MASK) == 0) {
       //printf("nRST pressed\r\n");
       while ((RPI_GpioBase->GPLEV0 & NRST_MASK) == 0);
       //printf("nRST released\r\n");
    }
  }
}
