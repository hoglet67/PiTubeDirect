/*
 * Tube ULA Emulation
 *
 * (c) 2016 David Banks and Ed Spittles
 * 
 * Based on code from B-em v2.2 by Tom Walker
 *
 */

#include <stdio.h>
#include <inttypes.h>
#include "tube-defs.h"
#include "tube.h"
#include "tube-ula.h"
#include "rpi-gpio.h"
#include "rpi-aux.h"
#include "rpi-interrupts.h"
#include "cache.h"
#include "info.h"
#include "performance.h"

// For predictable timing (i.e. stalling to to cache or memory contention)
// we need to find somewhere in I/O space to place the tube registers.
//
// This makes a MASSIVE improvement to the timing, compared to tube_regs
// being placed in L2 cached memory. With this change we have ~+50ns of
// setup margin, without it we had ~-150ns in the worst (rare) case.
//
// These locations must allow 32 bit reads/writes of arbitrary data
//
// See http://elinux.org/BCM2835_registers
//
// We have chosen MS_MBOX_0..MS_MBOX_7, which are 8 consecutive words.
//
// These locations are possibly the 8 words of the ARM-GPU mailbox,
// so we may need to change when we want to use mailboxes.
//
// Another option if we go back to 8-bit values tube_regs is to use
// CPG_Param0..CPG_Param1

#define GPU_TUBE_REG_ADDR 0x7e0000a0
#define ARM_TUBE_REG_ADDR ((GPU_TUBE_REG_ADDR & 0x00FFFFFF) | PERIPHERAL_BASE)

#include "tubevc.h"
#include "startup.h"

int test_pin;
static int led_type=0;

static volatile uint32_t *tube_regs = (uint32_t *) ARM_TUBE_REG_ADDR;
static uint32_t host_addr_bus;

#define HBIT_7 (1 << 25)
#define HBIT_6 (1 << 24)
#define HBIT_5 (1 << 23)
#define HBIT_4 (1 << 22)
#define HBIT_3 (1 << 11)
#define HBIT_2 (1 << 10)
#define HBIT_1 (1 << 9)
#define HBIT_0 (1 << 8)

#define BYTE_TO_WORD(data) ((((data) & 0x0F) << 8) | (((data) & 0xF0) << 18))
#define WORD_TO_BYTE(data) ((((data) >> 8) & 0x0F) | (((data) << 18) & 0xF0))

static char copro_command =0;
static perf_counters_t pct;

uint8_t ph1[24],ph3_1;
uint8_t hp1,hp2,hp3[2],hp4;
uint8_t pstat[4];
uint8_t ph3pos,hp3pos;
uint8_t ph1rdpos,ph1wrpos,ph1len;
volatile int tube_irq;


// Host end of the fifos are the ones read by the tube isr
#define PH1_0 tube_regs[1]
#define PH2   tube_regs[3]
#define PH3_0 tube_regs[5]
#define PH3_1 ph3_1
#define PH4   tube_regs[7]

// Host status registers are the ones read by the tube isr
#define HSTAT1 tube_regs[0]
#define HSTAT2 tube_regs[2]
#define HSTAT3 tube_regs[4]
#define HSTAT4 tube_regs[6]

// parasite status registers are local to this file
#define PSTAT1 pstat[0]
#define PSTAT2 pstat[1]
#define PSTAT3 pstat[2]
#define PSTAT4 pstat[3]

#ifdef DEBUG_TUBE

#define  TUBE_READ_MARKER 0x80000000
#define TUBE_WRITE_MARKER 0x40000000

unsigned int tube_index;
unsigned int tube_buffer[0x10000];

void tube_dump_buffer() {
   int i;
   LOG_INFO("tube_index = %u\r\n", tube_index);
   for (i = 0; i < tube_index; i++) {
      if (tube_buffer[i] & (TUBE_READ_MARKER | TUBE_WRITE_MARKER)) {
         if (tube_buffer[i] & TUBE_READ_MARKER) {
            LOG_INFO("Rd R");
         }
         if (tube_buffer[i] & TUBE_WRITE_MARKER) {
            LOG_INFO("Wr R");
         }
         // Covert address (1,3,5,7) to R1,R2,R3,R4
         LOG_INFO("%u = %02x\r\n", 1 + ((tube_buffer[i] & 0xF00) >> 9), tube_buffer[i] & 0xFF);
      } else {
         LOG_INFO("?? %08x\r\n", tube_buffer[i]);
      }
   }
}

void tube_reset_buffer() {
   int i;
   tube_index = 0;
   for (i = 0; i < 0x10000; i++) {
      tube_buffer[i] = 0;
   }
}
#endif
/*
static void tube_updateints_IRQ()
{   
   // Test for IRQ
   tube_irq = tube_irq & (0xFF - 1);
   if ((HSTAT1 & HBIT_1) && (PSTAT1 & 128)) tube_irq  |= 1;
   if ((HSTAT1 & HBIT_2) && (PSTAT4 & 128)) tube_irq  |= 1;
}

static void tube_updateints_NMI()
{   
   // Test for NMI
   tube_irq = tube_irq &(0xFF - 2);
   if ((HSTAT1 & HBIT_3) && !(HSTAT1 & HBIT_4) && ((hp3pos > 0) || (ph3pos == 0))) tube_irq|=2;
   if ((HSTAT1 & HBIT_3) &&  (HSTAT1 & HBIT_4) && ((hp3pos > 1) || (ph3pos == 0))) tube_irq|=2;
}
*/
void tube_enable_fast6502(void)
{
   int cpsr = _disable_interrupts();
   tube_irq |= FAST6502_BIT;
   if ((cpsr & 0xc0) != 0xc0) {
    _enable_interrupts();
   }
}

void tube_disable_fast6502(void)
{
   int cpsr = _disable_interrupts();
   tube_irq &= ~FAST6502_BIT;
   if ((cpsr & 0xc0) != 0xc0) {
    _enable_interrupts();
   }
}  

void tube_ack_nmi(void)
{
   int cpsr = _disable_interrupts();
   tube_irq &= ~NMI_BIT;
   if ((cpsr & 0xc0) != 0xc0) {
    _enable_interrupts();
   } 
}

void copro_command_excute(unsigned char copro_command,unsigned char val)
{
    switch (copro_command)
    {
      case 0 :      
          if (val == 0)
             copro_speed = 0;
          else
             copro_speed = (arm_speed/(1000000/256) / val);
          LOG_DEBUG("New Copro speed= %u, %u\r\n", val, copro_speed);
          return; 
      case 1 : // *fx 151,226,1 followed by *fx 151,228,val
               // Select memory size
               if (val & 128 )
                  copro_memory_size = (val & 127 ) * 8 * 1024 * 1024;
               else 
                  copro_memory_size = (val & 127 ) * 64 * 1024 ;
               if (copro_memory_size > 16 *1024 * 1024)
                  copro_memory_size = 0;
               LOG_DEBUG("New Copro memory size = %u, %u\r\n", val, copro_memory_size);
               copro = copro | 128 ;  // Set bit 7 to signal full reset of core
               return;
      default :
          break;
    }
          
}      
      
static void tube_reset()
{   
   tube_irq |= TUBE_ENABLE_BIT;
   tube_irq &= ~(RESET_BIT + NMI_BIT + IRQ_BIT);
   hp3pos = 0;
   ph1rdpos = ph1wrpos = ph1len = 0;
   ph3pos = 1;
   PSTAT1 = 0x40;
   PSTAT2 = 0x7F;
   PSTAT3 = PSTAT2;
   PSTAT4 = PSTAT2;
   HSTAT1 = HBIT_6;
   HSTAT2 = HBIT_6 | HBIT_5 | HBIT_4 | HBIT_3 | HBIT_2 | HBIT_1 | HBIT_0;
   HSTAT3 = HSTAT2 | HBIT_7;
   HSTAT4 = HSTAT2;
   // On the Model B the initial write of &8E to FEE0 is missed
   // If the Pi is slower in starting than the Beeb. A work around
   // is to have the tube emulation reset to a state with interrupts
   // enabled.
   HSTAT1 |= HBIT_3 | HBIT_2 | HBIT_1;
   //tube_updateints_IRQ();
   //tube_updateints_NMI();
}

// 6502 Host reading the tube registers
//
// This function implements read-ahead, so the next values
// to be read are already pre-loaded into the tube_regs[]
// array ready for the FIQ handler to read without any delay.
// This is why there is no return value.
// 
// Reading of status registers has no side effects, so nothing to
// do here for even registers (all handled in the FIQ handler).

static void tube_host_read(uint16_t addr)
{
   switch (addr & 7)
   {
   case 1: /*Register 1*/
      if (ph1len > 0) {
         PH1_0 = BYTE_TO_WORD(ph1[ph1rdpos]);
           //for (c = 1; c < 23; c++) ph1[c] = ph1[c + 1];
         ph1len--;
         if ( ph1len != 0)
         {
            if (ph1rdpos== 23)
               ph1rdpos =0; 
            else
               ph1rdpos++;
         }
         if (!ph1len) HSTAT1 &= ~HBIT_7;
         PSTAT1 |= 0x40;
      }
      // tube_updateints_IRQ(); // the above can't change the irq status
      break;
   case 3: /*Register 2*/
      if (HSTAT2 & HBIT_7)
      {
         HSTAT2 &= ~HBIT_7;
         PSTAT2 |=  0x40;
      }
      break;
   case 5: /*Register 3*/
      if (ph3pos > 0)
      {
         PH3_0 = BYTE_TO_WORD(PH3_1);
         ph3pos--;
         PSTAT3 |= 0xC0;
         if (!ph3pos) HSTAT3 &= ~HBIT_7;
         if ((HSTAT1 & HBIT_3) && (ph3pos == 0)) tube_irq|=NMI_BIT;
         //tube_updateints_NMI();
      }
      break;
   case 7: /*Register 4*/
      if (HSTAT4 & HBIT_7)
      {
         HSTAT4 &= ~HBIT_7;
         PSTAT4 |=  0x40;
      }
      // tube_updateints_IRQ(); // the above can't change the irq status
      break;
   }
}

static void tube_host_write(uint16_t addr, uint8_t val)
{
   switch (addr & 7)
   {
   case 0: /*Register 1 control/status*/

      if (!(tube_irq & TUBE_ENABLE_BIT))
         return;

      // Evaluate NMI before the control register written 
      int nmi1 = 0;
      if ((HSTAT1 & HBIT_3) && !(HSTAT1 & HBIT_4) && ((hp3pos > 0) || (ph3pos == 0))) nmi1 = 1;
      if ((HSTAT1 & HBIT_3) &&  (HSTAT1 & HBIT_4) && ((hp3pos > 1) || (ph3pos == 0))) nmi1 = 1;
    
      if (val & 0x80) {
         // Implement software tube reset
         if (val & 0x40) {
            tube_reset();
         } else {
            HSTAT1 |= BYTE_TO_WORD(val & 0x3F);
         }
      } else {
         HSTAT1 &= ~BYTE_TO_WORD(val & 0x3F);
      }
      
      if ( HSTAT1 & HBIT_5) {
         tube_irq |= RESET_BIT;
      } else {
         tube_irq &= ~RESET_BIT;
      }

      // Evaluate NMI again after the control register written 
      int nmi2 = 0;
      if ((HSTAT1 & HBIT_3) && !(HSTAT1 & HBIT_4) && ((hp3pos > 0) || (ph3pos == 0))) nmi2 = 1;
      if ((HSTAT1 & HBIT_3) &&  (HSTAT1 & HBIT_4) && ((hp3pos > 1) || (ph3pos == 0))) nmi2 = 1;
      
      // Only propagate significant rising edges
      if (!nmi1 && nmi2) tube_irq |= NMI_BIT;

      // And disable regardless
      if (!nmi2) tube_irq &= ~(NMI_BIT);

      tube_irq &= ~(IRQ_BIT);
      if ((HSTAT1 & HBIT_1) && (PSTAT1 & 128)) tube_irq  |= IRQ_BIT;
      if ((HSTAT1 & HBIT_2) && (PSTAT4 & 128)) tube_irq  |= IRQ_BIT;
      break;
   case 1: /*Register 1*/
      //if (!tube_enabled)
      //      return;
      hp1 = val;
      PSTAT1 |=  0x80;
      HSTAT1 &= ~HBIT_6;
      if (HSTAT1 & HBIT_1) tube_irq  |= IRQ_BIT; //tube_updateints_IRQ();
      break;
   case 2:
      copro_command = val;   
      break;
   case 3: /*Register 2*/
      //if (!tube_enabled)
      //  return;
      hp2 = val;
      PSTAT2 |=  0x80;
      HSTAT2 &= ~HBIT_6;
      break;
   case 4:
      copro_command_excute(copro_command,val);
      break;
   case 5: /*Register 3*/
     // if (!tube_enabled)
     //    return;
#ifdef DEBUG_TRANSFERS
      checksum_h *= 13;
      checksum_h += val;
      count_h++;
#endif
      if (HSTAT1 & HBIT_4)
      {
         if (hp3pos < 2)
            hp3[hp3pos++] = val;
         if (hp3pos == 2)
         {
            PSTAT3 |=  0x80;
            HSTAT3 &= ~HBIT_6;
         }
         if ((HSTAT1 & HBIT_3) && (hp3pos > 1)) tube_irq |= NMI_BIT;
      }
      else
      {
         hp3[0] = val;
         hp3pos = 1;
         PSTAT3 |=  0x80;
         HSTAT3 &= ~HBIT_6;
         if (HSTAT1 & HBIT_3) tube_irq |= NMI_BIT;
      }
      //tube_updateints_NMI();
      break;
   case 6:  
      copro = val;
      LOG_DEBUG("New Copro = %u\r\n", copro);
      return;
   case 7: /*Register 4*/
     // if (!tube_enabled)
     //       return;
      hp4 = val;
      PSTAT4 |=  0x80;
      HSTAT4 &= ~HBIT_6;
#ifdef DEBUG_TRANSFERS
      if (val == 4) {
         LOG_INFO("checksum_h = %08"PRIX32" %08"PRIX32"\r\n", count_h, checksum_h);
         LOG_INFO("checksum_p = %08"PRIX32" %08"PRIX32"\r\n", count_p, checksum_p);
         checksum_h = 0;
         checksum_p = 0;
         count_h = 0;
         count_p = 0;
      }
#endif
       if (HSTAT1 & HBIT_2) tube_irq |= IRQ_BIT; //tube_updateints_IRQ();
      break;
   default:
      LOG_WARN("Illegal host write to %d\r\n", addr);
   }
}

uint8_t tube_parasite_read(uint32_t addr)
{
   int cpsr = _disable_interrupts();
   uint8_t temp ;
   switch (addr & 7)
   {
   case 0: /*Register 1 stat*/
      temp = PSTAT1 | (WORD_TO_BYTE(HSTAT1) & 0x3F);
      break;
   case 1: /*Register 1*/
      temp = hp1;
      if (PSTAT1 & 0x80)
      {
         PSTAT1 &= ~0x80;
         HSTAT1 |=  HBIT_6;
         //tube_updateints_IRQ(); // clear irq if required reg 4 isnt irqing
         if (!(PSTAT4 & 128)) tube_irq &= ~IRQ_BIT;
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
         HSTAT2 |=  HBIT_6;
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
            HSTAT3 |=  HBIT_6;
            PSTAT3 &= ~0x80;
         }
         //tube_updateints_NMI();
         // here we want to only clear NMI if required
         if ( ( !(ph3pos == 0) ) && ( (!(HSTAT1 & HBIT_4) && (!(hp3pos >0))) || (HSTAT1 & HBIT_4) ) ) tube_irq &= ~NMI_BIT;     
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
         HSTAT4 |=  HBIT_6;
         //tube_updateints_IRQ(); // clear irq if  reg 1 isnt irqing
         if (!(PSTAT1 & 128)) tube_irq &= ~IRQ_BIT;
      }
      break;
   }
  
#ifdef DEBUG_TUBE
   if (addr & 1) {
      tube_buffer[tube_index++] = TUBE_READ_MARKER | ((addr & 7) << 8) | temp;
      tube_index &= 0xffff;
   }
#endif
   if ((cpsr & 0xc0) != 0xc0) {
      _set_interrupts(cpsr);
   }
   return temp;
}

// Special IO write wrapper for the 65Tube Co Pro:
// - the tube registers are accessed at 0xFEF8-0xFEFF
// - the bank select registers are accessed at 0xFEE0-0xFEE7
void tube_parasite_write_banksel(uint32_t addr, uint8_t val)
{
  if ((addr & 0xFFF8) == 0xFEF8) {
    // Tube writes get passed on to original code
    tube_parasite_write(addr, val);
  } else if ((addr & 0xFFF8) == 0xFEE0) {
     // Implement write only bank selection registers for 8x 8K pages
     int logical = (addr & 7) << 1;
     int physical = (val << 1);
     map_4k_page(logical, physical);
     map_4k_page(logical + 1, physical + 1);
     // Page 0 must also be mapped as page 16 (64K)
     if (logical == 0) {
       printf("Remapping page zero!\r\n");
       map_4k_page(16, physical);
     }
  }
}

void tube_parasite_write(uint32_t addr, uint8_t val)
{
   int cpsr = _disable_interrupts();
#ifdef DEBUG_TUBE
   if (addr & 1) {
      tube_buffer[tube_index++] = TUBE_WRITE_MARKER | ((addr & 7) << 8) | val;
      tube_index &= 0xffff;
   }
#endif
   switch (addr & 7)
   {
   case 1: /*Register 1*/
      if (ph1len < 24)
      {
         if (ph1len == 0) {
            PH1_0 = BYTE_TO_WORD(val);
         } else {
            ph1[ph1wrpos] = val;
            if (ph1wrpos== 23)
               ph1wrpos =0; 
            else
               ph1wrpos++;
         }

         ph1len++;
         HSTAT1 |= HBIT_7;
         if (ph1len == 24) PSTAT1 &= ~0x40;
      }
      // tube_updateints_IRQ(); // the above can't change the IRQ flags
      break;
   case 3: /*Register 2*/
      PH2 = BYTE_TO_WORD(val);
      HSTAT2 |=  HBIT_7;
      PSTAT2 &= ~0x40;
      break;
   case 5: /*Register 3*/
      if (HSTAT1 & HBIT_4)
      {
         if (ph3pos < 2) {
            if (ph3pos == 0) {
               PH3_0 = BYTE_TO_WORD(val);
            } else {
               PH3_1 = val;
            }
            ph3pos++;
         }
         if (ph3pos == 2)
         {
            HSTAT3 |=  HBIT_7;
            PSTAT3 &= ~0x40;
         }
         //NMI if other case isn't seting it
         if (!(hp3pos > 1) ) tube_irq &= ~NMI_BIT;
      }
      else
      {
         PH3_0 = BYTE_TO_WORD(val);
         ph3pos = 1;
         HSTAT3 |=  HBIT_7;
         PSTAT3 &= ~0xC0;
         //NMI if other case isn't seting it
         if (!(hp3pos > 0) ) tube_irq &= ~NMI_BIT;
      }
      //tube_updateints_NMI();
      // here we want to only clear NMI if required
      
      break;
   case 7: /*Register 4*/
      PH4 = BYTE_TO_WORD(val);
      HSTAT4 |=  HBIT_7;
      PSTAT4 &= ~0x40;
      // tube_updateints_IRQ(); // the above can't change IRQ flag
      break;
   }
   if ((cpsr & 0xc0) != 0xc0) {
      _set_interrupts(cpsr);
  }
}

// Returns bit 0 set if IRQ is asserted by the tube
// Returns bit 1 set if NMI is asserted by the tube
// Returns bit 2 set if RST is asserted by the host or tube

int tube_io_handler(uint32_t mail)
{
   int addr;
#ifndef USE_GPU   
   int data;
   int rnw;
   int ntube;
   int nrst;

#ifdef USE_HW_MAILBOX
   // Sequence numbers are currently 4 bits, and are stored in bits 12..15
   int act_seq_num;
   static int exp_seq_num = -1;
   act_seq_num = (mail >> 12) & 15;
#endif
#endif

#ifdef USE_GPU

   if ((mail >> 12) & 1)        // Check for Reset
   {
      tube_irq |= RESET_BIT;
      return tube_irq;      // Set reset Flag
   }
   else
   {
      addr = (mail>>8) & 7;
      if ( ( (mail >>11 ) & 1) == 0) {  // Check read write flag
         tube_host_write(addr, mail & 0xFF);
      } else {
         tube_host_read(addr);
      }
      return tube_irq ;
   }
#else
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

   // Only report OVERRUNs that occur when nRST is high
#ifdef USE_HW_MAILBOX
   if (exp_seq_num < 0) {
      // A resync is being forces
      exp_seq_num = act_seq_num;
   } else {
      // Increment the expected sequence number
      exp_seq_num = (exp_seq_num + 1) & 15;
   }
   if ((exp_seq_num != act_seq_num) && (mail & NRST_MASK)) {
      LOG_WARN("OVERRUN: exp=%X act=%X A=%d; D=%02X; RNW=%d; NTUBE=%d; nRST=%d\r\n", exp_seq_num, act_seq_num, addr, data, rnw, ntube, nrst); 
   }
   if (mail & NRST_MASK) {
      // Not reset: sync to the last received sequence number
      exp_seq_num = act_seq_num;
   } else {
      // Reset: Force a resync as mailbox overlow is very likely here
      exp_seq_num = -1;
   }
#else
   if ((mail & OVERRUN_MASK) && (mail & NRST_MASK)) {
      LOG_WARN("OVERRUN: A=%d; D=%02X; RNW=%d; NTUBE=%d; nRST=%d\r\n", addr, data, rnw, ntube, nrst); 
   }
#endif


   if (mail & GLITCH_MASK) {
      LOG_WARN("GLITCH: A=%d; D=%02X; RNW=%d; NTUBE=%d; nRST=%d\r\n", addr, data, rnw, ntube, nrst); 
   } else if (nrst == 1) {

      if (ntube == 0) {
         if (rnw == 0) {
            tube_host_write(addr, data);
         } else {
            tube_host_read(addr);
         }
      } else {
         LOG_WARN("LATE: A=%d; D=%02X; RNW=%d; NTUBE=%d; nRST=%d\r\n", addr, data, rnw, ntube, nrst); 
      }      
   }

#if TEST_MODE
   LOG_INFO("A=%d; D=%02X; RNW=%d; NTUBE=%d; nRST=%d\r\n", addr, data, rnw, ntube, nrst);
#endif

   if (nrst == 0 || (tube_enabled && (HSTAT1 & HBIT_5))) {
      return tube_irq | 4;
   } else {
      return tube_irq & 3;
   }
#endif
}


void tube_init_hardware()
{

  // early 26pin pins have a slightly different pin out
  
  switch (get_revision())
  {
     case 2 :
     case 3 :   
          // Write 1 to the LED init nibble in the Function Select GPIO
          // peripheral register to enable LED pin as an output
          RPI_GpioBase-> GPFSEL[1] |= 1<<18;
          host_addr_bus = (A2_PIN_26PIN << 16) | (A1_PIN_26PIN << 8) | (A0_PIN_26PIN); // address bus GPIO mapping
          RPI_SetGpioPinFunction(A2_PIN_26PIN, FS_INPUT);
          RPI_SetGpioPinFunction(A1_PIN_26PIN, FS_INPUT);
          RPI_SetGpioPinFunction(A0_PIN_26PIN, FS_INPUT);
          RPI_SetGpioPinFunction(TEST_PIN_26PIN, FS_OUTPUT);
          test_pin = TEST_PIN_26PIN;
        break;
     
         
     default :

          host_addr_bus = (A2_PIN_40PIN << 16) | (A1_PIN_40PIN << 8) | (A0_PIN_40PIN); // address bus GPIO mapping
          RPI_SetGpioPinFunction(A2_PIN_40PIN, FS_INPUT);
          RPI_SetGpioPinFunction(A1_PIN_40PIN, FS_INPUT);
          RPI_SetGpioPinFunction(A0_PIN_40PIN, FS_INPUT); 
          RPI_SetGpioPinFunction(TEST_PIN_40PIN, FS_OUTPUT);
          RPI_SetGpioPinFunction(TEST2_PIN, FS_OUTPUT);
          RPI_SetGpioPinFunction(TEST3_PIN, FS_OUTPUT);
          test_pin = TEST_PIN_40PIN;         
       break;   
  }
  
  switch (get_revision())
  {
     case 2 :
     case 3 :   led_type = 0;
         break;
     case 0xa02082: // Rpi3
     case 0xa22082:
     case 0xa32082:
         led_type = 2;
         break;
     case 0xa020d3 : // rpi3b+
         led_type = 3;
         RPI_GpioBase-> GPFSEL[2] |= 1<<27;
         break;
     default :
               // Write 1 to the LED init nibble in the Function Select GPIO
          // peripheral register to enable LED pin as an output  
          RPI_GpioBase-> GPFSEL[4] |= 1<<21;
          led_type = 1;
         break;
  }        
  
  // Configure our pins as inputs
  RPI_SetGpioPinFunction(D7_PIN, FS_INPUT);
  RPI_SetGpioPinFunction(D6_PIN, FS_INPUT);
  RPI_SetGpioPinFunction(D5_PIN, FS_INPUT);
  RPI_SetGpioPinFunction(D4_PIN, FS_INPUT);
  RPI_SetGpioPinFunction(D3_PIN, FS_INPUT);
  RPI_SetGpioPinFunction(D2_PIN, FS_INPUT);
  RPI_SetGpioPinFunction(D1_PIN, FS_INPUT);
  RPI_SetGpioPinFunction(D0_PIN, FS_INPUT);

  RPI_SetGpioPinFunction(PHI2_PIN, FS_INPUT);
  RPI_SetGpioPinFunction(NTUBE_PIN, FS_INPUT);
  RPI_SetGpioPinFunction(NRST_PIN, FS_INPUT);
  RPI_SetGpioPinFunction(RNW_PIN, FS_INPUT);

  // Initialise the info system with cached values (as we break the GPU property interface)
  init_info();

#ifdef DEBUG
  dump_useful_info();
#endif
  
  // Initialize performance counters
#if defined(RPI2) || defined(RPI3) 
   pct.num_counters = 6;
   pct.type[0] = PERF_TYPE_L1I_CACHE;
   pct.type[1] = PERF_TYPE_L1I_CACHE_REFILL;
   pct.type[2] = PERF_TYPE_L1D_CACHE;
   pct.type[3] = PERF_TYPE_L1D_CACHE_REFILL;
   pct.type[4] = PERF_TYPE_L2D_CACHE_REFILL;
   pct.type[5] = PERF_TYPE_INST_RETIRED;
   pct.counter[0] = 0;
   pct.counter[1] = 0;
   pct.counter[2] = 0;
   pct.counter[3] = 0;
   pct.counter[4] = 0;
   pct.counter[5] = 0;
#else
   pct.num_counters = 2;
   pct.type[0] = PERF_TYPE_I_CACHE_MISS;
   pct.type[1] = PERF_TYPE_D_CACHE_MISS;
   pct.counter[0] = 0;
   pct.counter[1] = 0;
#endif

   hp1 = hp2 = hp4 = hp3[0]= hp3[1]=0;

}

int tube_is_rst_active() {
   return ((RPI_GpioBase->GPLEV0 & NRST_MASK) == 0) ;
}
#if 0
static void tube_wait_for_rst_active() {
   while (!tube_is_rst_active());
}
#endif
// Debounce RST

// On my Model B the characterisc of RST bounce on release is a burst
// of short (2us) high pulses approx 2ms before a clean rising RST edge

// On my Master 128 there is no RST bounce, and RST is clean

// This debounce code waits for RST to go high and stay high for a period
// set by DEBOUNCE_TIME (a of 10000 on a Pi 3 was measured at 690us)

// On the Model B
// - the first tube accesses are ~15ms after RST is released

// On the Master
// - the first tube accesses are ~13ms after RST is released

#define DEBOUNCE_TIME 10000

void tube_wait_for_rst_release() {
   volatile int i;
   do {
      // Wait for reset to be released
      while (tube_is_rst_active());
      // Make sure RST stays continuously high for a further 100us
      for (i = 0; i < DEBOUNCE_TIME && !tube_is_rst_active(); i++);
      // Loop back if we exit the debouce loop prematurely because RST has gone active again
   } while (i < DEBOUNCE_TIME);
   // Reset all the TUBE ULA registers
   tube_reset();
}

void tube_reset_performance_counters() {
   reset_performance_counters(&pct);
}

void tube_log_performance_counters() {
#ifdef DEBUG_TUBE
   // Dump tube buffer
   tube_dump_buffer();
   // Reset the tube buffer
   tube_reset_buffer();
#endif
#ifdef DEBUG
   read_performance_counters(&pct);
   print_performance_counters(&pct);
#endif
   LOG_DEBUG("tube reset - copro %u\r\n", copro);
#ifdef DEBUG_TRANSFERS
   LOG_INFO("checksum_h = %08"PRIX32" %08"PRIX32"\r\n", count_h, checksum_h);
   LOG_INFO("checksum_p = %08"PRIX32" %08"PRIX32"\r\n", count_p, checksum_p);
   checksum_h = 0;
   checksum_p = 0;
   count_h = 0;
   count_p = 0;
#endif
}

void disable_tube() {
   int i;
   tube_irq &= ~TUBE_ENABLE_BIT;
   for (i = 0; i < 8; i++) {
      tube_regs[i] = 0xfe;
   }
}

void start_vc_ula()
{
   int func,r0,r1, r2,r3,r4,r5;
   extern int tube_delay;
   func = (int) &tubevc_asm[0];
   r0   = (int) GPU_TUBE_REG_ADDR;       // address of tube register block in IO space
   r1   = led_type; 
   r2   = tube_delay;

   r3   = host_addr_bus; 
   r4   = 0;
   r5   = 1<<test_pin;                     // test pin

#ifdef DEBUG_GPU
   LOG_DEBUG("Staring VC ULA\r\n");
   LOG_DEBUG("VidCore code = %08x\r\n", func);
   LOG_DEBUG("VidCore   r0 = %08x\r\n", r0);
   LOG_DEBUG("VidCore   r1 = %08x\r\n", r1);
   LOG_DEBUG("VidCore   r2 = %08x\r\n", r2);
   LOG_DEBUG("VidCore   r3 = %08x\r\n", r3);
   LOG_DEBUG("VidCore   r4 = %08x\r\n", r4);
   LOG_DEBUG("VidCore   r5 = %08x\r\n", r5);
#endif   
   RPI_PropertyInit();
   RPI_PropertyAddTag(TAG_EXECUTE_CODE,func,r0,r1,r2,r3,r4,r5);
   RPI_PropertyProcessNoCheck();
// for (r0 = 0x7E002000; r0 < 0x7E003000; r0+= 4) {
//    rpi_mailbox_property_t *buf;
//    RPI_PropertyInit();
//    RPI_PropertyAddTag(TAG_EXECUTE_CODE,func,r0,r1,r2,r3,r4,r5);
//    RPI_PropertyProcess();
//    buf = RPI_PropertyGet(TAG_EXECUTE_CODE);
//    if (buf) {
//       LOG_DEBUG("%08x %08x\r\n", r0, buf->data.buffer_32[0]);
//    } else {
//       LOG_DEBUG("%08x ?\r\n", r0);
//    }
// }

}
