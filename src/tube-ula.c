/*B-em v2.2 by Tom Walker
  Tube ULA emulation*/

#include <stdio.h>
#include <inttypes.h>
#include "tube-defs.h"
#include "tube.h"
#include "rpi-gpio.h"
#include "rpi-aux.h"
#include "rpi-interrupts.h"

#define DEBUG_TRANSFERS

extern volatile uint8_t tube_regs[];

extern volatile uint32_t gpfsel_data_idle[3];
extern volatile uint32_t gpfsel_data_driving[3];
const uint32_t magic[3] = {MAGIC_C0, MAGIC_C1, MAGIC_C2 | MAGIC_C3 };

// Bit 0 is the tube asserting irq
// Bit 1 is the tube asserting nmi
int tube_irq=0;

uint8_t ph1[24],ph2,ph3[2],ph4;
uint8_t hp1,hp2,hp3[2],hp4;
uint8_t pstat[4];
int ph1pos,ph3pos,hp3pos;

// host status registers are the ones seen by the tube isr
#define HSTAT1 tube_regs[0]
#define HSTAT2 tube_regs[2]
#define HSTAT3 tube_regs[4]
#define HSTAT4 tube_regs[6]

// parasite status registers are local to this file
#define PSTAT1 pstat[0]
#define PSTAT2 pstat[1]
#define PSTAT3 pstat[2]
#define PSTAT4 pstat[3]

#ifdef DEBUG_TRANSFERS
uint32_t checksum_h = 0;
uint32_t count_h = 0;
uint32_t checksum_p = 0;
uint32_t count_p = 0;
#endif

void tube_updateints()
{
   
   // interrupt &= ~8;
   // if ((HSTAT1 & 1) && (HSTAT4 & 128)) interrupt |= 8;
   
   tube_irq = 0;
   if ((HSTAT1 & 2) && (PSTAT1 & 128)) tube_irq  |= 1;
   if ((HSTAT1 & 4) && (PSTAT4 & 128)) tube_irq  |= 1;
   if ((HSTAT1 & 8) && !(HSTAT1 & 16) && ((hp3pos > 0) || (ph3pos == 0))) tube_irq|=2;
   if ((HSTAT1 & 8) &&  (HSTAT1 & 16) && ((hp3pos > 1) || (ph3pos == 0))) tube_irq|=2;

   // Update read-ahead of host data
   tube_regs[1] = ph1[0];
   tube_regs[3] = ph2;
   tube_regs[5] = ph3[0];
   tube_regs[7] = ph4;
}

uint8_t tube_host_read(uint16_t addr)
{
   uint8_t temp = 0;
   int c;
   switch (addr & 7)
   {
   case 0: /*Reg 1 Stat*/
      temp = HSTAT1;
      break;
   case 1: /*Register 1*/
      temp = ph1[0];
      if (ph1pos > 0) {
         for (c = 0; c < 23; c++) ph1[c] = ph1[c + 1];
         ph1pos--;
         PSTAT1 |= 0x40;
         if (!ph1pos) HSTAT1 &= ~0x80;
      }
      //printf("Host read R1=%02x\r\n", temp);
      break;
   case 2: /*Register 2 Stat*/
      temp = HSTAT2;
      break;
   case 3: /*Register 2*/
      temp = ph2;
      if (HSTAT2 & 0x80)
      {
         HSTAT2 &= ~0x80;
         PSTAT2 |=  0x40;
      }
      break;
   case 4: /*Register 3 Stat*/
      temp = HSTAT3;
      break;
   case 5: /*Register 3*/
      temp = ph3[0];
      if (ph3pos > 0)
      {
         ph3[0] = ph3[1];
         ph3pos--;
         PSTAT3 |= 0x40;
         if (!ph3pos) HSTAT3 &= ~0x80;
      }
      break;
   case 6: /*Register 4 Stat*/
      temp = HSTAT4;
      break;
   case 7: /*Register 4*/
      temp = ph4;
      if (HSTAT4 & 0x80)
      {
         HSTAT4 &= ~0x80;
         PSTAT4 |=  0x40;
      }
      break;
   }
   tube_updateints();
   return temp;
}

void tube_host_write(uint16_t addr, uint8_t val)
{
   switch (addr & 7)
   {
   case 0: /*Register 1 stat*/
      if (val & 0x80) HSTAT1 |=  (val&0x3F);
      else            HSTAT1 &= ~(val&0x3F);
      break;
   case 1: /*Register 1*/
      hp1 = val;
      PSTAT1 |=  0x80;
      HSTAT1 &= ~0x40;
      break;
   case 3: /*Register 2*/
      hp2 = val;
      PSTAT2 |=  0x80;
      HSTAT2 &= ~0x40;
      break;
   case 5: /*Register 3*/
#ifdef DEBUG_TRANSFERS
      checksum_h *= 13;
      checksum_h += val;
      count_h++;
#endif
      if (HSTAT1 & 16)
      {
         if (hp3pos < 2)
            hp3[hp3pos++] = val;
         if (hp3pos == 2)
         {
            PSTAT3 |=  0x80;
            HSTAT3 &= ~0x40;
         }
      }
      else
      {
         hp3[0] = val;
         hp3pos = 1;
         PSTAT3 |=  0x80;
         HSTAT3 &= ~0x40;
         tube_updateints();
      }
//                printf("Write R3 %i\n",hp3pos);
      break;
   case 7: /*Register 4*/
      hp4 = val;
      PSTAT4 |=  0x80;
      HSTAT4 &= ~0x40;
#ifdef DEBUG_TRANSFERS
      if (val == 4) {
         printf("checksum_h = %08"PRIX32" %08"PRIX32"\r\n", count_h, checksum_h);
         printf("checksum_p = %08"PRIX32" %08"PRIX32"\r\n", count_p, checksum_p);
         checksum_h = 0;
         checksum_p = 0;
         count_h = 0;
         count_p = 0;
      }
#endif
      break;
   }
   tube_updateints();
}

uint8_t tube_parasite_read(uint32_t addr)
{
   uint8_t temp = 0;
   switch (addr & 7)
   {
   case 0: /*Register 1 stat*/
      temp = PSTAT1 | (HSTAT1 & 0x3F);
      break;
   case 1: /*Register 1*/
      temp = hp1;
      if (PSTAT1 & 0x80)
      {
         PSTAT1 &= ~0x80;
         HSTAT1 |=  0x40;
      }
      break;
   case 2: /*Register 2 stat*/
      temp = PSTAT2;
      break;
   case 3: /*Register 2*/
      temp = hp2;
      if (PSTAT2 & 0x80)
      {
         PSTAT2 &= ~0x80;
         HSTAT2 |=  0x40;
      }
      break;
   case 4: /*Register 3 stat*/
      temp = PSTAT3;
      break;
   case 5: /*Register 3*/
      temp = hp3[0];
#ifdef DEBUG_TRANSFERS
      checksum_p *= 13;
      checksum_p += temp;
      count_p ++;
#endif
      if (hp3pos>0)
      {
         hp3[0] = hp3[1];
         hp3pos--;
         if (!hp3pos)
         {
            HSTAT3 |=  0x40;
            PSTAT3 &= ~0x80;
         }
      }
      break;
   case 6: /*Register 4 stat*/
      temp = PSTAT4;
      break;
   case 7: /*Register 4*/
      temp = hp4;
      if (PSTAT4 & 0x80)
      {
         PSTAT4 &= ~0x80;
         HSTAT4 |=  0x40;
      }
      break;
   }
   tube_updateints();
   return temp;
}

void tube_parasite_write(uint32_t addr, uint8_t val)
{
   switch (addr & 7)
   {
   case 1: /*Register 1*/
      if (ph1pos < 24)
      {
         ph1[ph1pos++] = val;
         HSTAT1 |= 0x80;
         if (ph1pos == 24) PSTAT1 &= ~0x40;
         //printf("Parasite wrote R1=%02x\r\n", val);
      }
      break;
   case 3: /*Register 2*/
      ph2 = val;
      HSTAT2 |=  0x80;
      PSTAT2 &= ~0x40;
      break;
   case 5: /*Register 3*/
      if (HSTAT1 & 16)
      {
         if (ph3pos < 2)
            ph3[ph3pos++] = val;
         if (ph3pos == 2)
         {
            HSTAT3 |=  0x80;
            PSTAT3 &= ~0x40;
         }
      }
      else
      {
         ph3[0] = val;
         ph3pos = 1;
         HSTAT3 |=  0x80;
         PSTAT3 &= ~0x40;
      }
      break;
   case 7: /*Register 4*/
      ph4 = val;
      HSTAT4 |=  0x80;
      PSTAT4 &= ~0x40;
      break;
   }
   tube_updateints();
}


void tube_reset()
{
   printf("tube reset\r\n");
   ph1pos = hp3pos = 0;
   ph3pos = 1;
   HSTAT1 = HSTAT2 = HSTAT4 = 0x40;
   PSTAT1 = PSTAT2 = PSTAT3 = PSTAT4 = 0x40;
   HSTAT3 = 0xC0;
   tube_updateints();
}

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

   // Toggle the LED on each tube access
   static int led = 0;
	if (led) {
	  LED_OFF();
	} else {
	  LED_ON();
	}
	led = ~led;

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

   if (mail & OVERRUN_MASK) {
      printf("OVERRUN: A=%d; D=%02X; RNW=%d; NTUBE=%d; nRST=%d\r\n", addr, data, rnw, ntube, nrst); 
   }

   if (nrst == 1 && ntube == 0) {
      if (rnw == 0) {
         tube_host_write(addr, data);
      } else {
         tube_host_read(addr);
      }
   }

#if TEST_MODE
   printf("A=%d; D=%02X; RNW=%d; NTUBE=%d; nRST=%d\r\n", addr, data, rnw, ntube, nrst);
#endif
   
   if (nrst == 0 || (tube_regs[0] & 0x20)) {
      return tube_irq | 4;
   } else {
      return tube_irq;
   }
}

void tube_init_hardware()
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

int tube_is_rst_active() {
   return ((RPI_GpioBase->GPLEV0 & NRST_MASK) == 0);
}

void tube_wait_for_rst_active() {
   while (!tube_is_rst_active());
}

void tube_wait_for_rst_release() {
   while (tube_is_rst_active());
}
