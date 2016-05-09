/*B-em v2.2 by Tom Walker
  Tube ULA emulation*/

#include <stdio.h>
#include <inttypes.h>

extern volatile uint8_t tube_regs[];

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

uint8_t copro_65tube_host_read(uint16_t addr)
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
      for (c = 0; c < 23; c++) ph1[c] = ph1[c + 1];
      ph1pos--;
      PSTAT1 |= 0x40;
      if (!ph1pos) HSTAT1 &= ~0x80;
      printf("Host read R1=%02x\r\n", temp);
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

void copro_65tube_host_write(uint16_t addr, uint8_t val)
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
      break;
   }
   tube_updateints();
}

uint8_t copro_65tube_tube_read(uint32_t addr)
{
   uint8_t temp = 0;
   switch (addr & 7)
   {
   case 0: /*Register 1 stat*/
      temp = PSTAT1 | (HSTAT1 & 0xc0);
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

void copro_65tube_tube_write(uint32_t addr, uint8_t val)
{
   switch (addr & 7)
   {
   case 1: /*Register 1*/
      if (ph1pos < 24)
      {
         ph1[ph1pos++] = val;
         HSTAT1 |= 0x80;
         if (ph1pos == 24) PSTAT1 &= ~0x40;
         printf("Parasite wrote R1=%02x\r\n", val);
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


void copro_65tube_tube_reset()
{
   printf("tube reset\r\n");
   ph1pos = hp3pos = 0;
   ph3pos = 1;
   HSTAT1 = HSTAT2 = HSTAT4 = 0x40;
   PSTAT1 = PSTAT2 = PSTAT3 = PSTAT4 = 0x40;
   HSTAT3 = 0xC0;
   tube_updateints();
}
