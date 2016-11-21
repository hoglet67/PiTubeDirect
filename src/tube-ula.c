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
// being placed in L2 cached memory. With this chanhe we have ~+50ns of
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

#ifdef USE_GPU
#include "tubevc.h"
#include "startup.h"
#endif

#ifndef USE_HW_MAILBOX
volatile uint32_t *tube_mailbox;
#endif

#ifdef USE_GPU

extern volatile uint32_t tube_regs_block[8];
static volatile uint32_t *tube_regs;

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

#else

extern volatile uint8_t tube_regs_block[8];
static volatile uint8_t *tube_regs;

#define HBIT_7 (1 << 7)
#define HBIT_6 (1 << 6)
#define HBIT_5 (1 << 5)
#define HBIT_4 (1 << 4)
#define HBIT_3 (1 << 3)
#define HBIT_2 (1 << 2)
#define HBIT_1 (1 << 1)
#define HBIT_0 (1 << 0)

#define BYTE_TO_WORD(data) (data)
#define WORD_TO_BYTE(data) (data)

#endif


extern volatile uint32_t gpfsel_data_idle[3];
extern volatile uint32_t gpfsel_data_driving[3];
const uint32_t magic[3] = {MAGIC_C0, MAGIC_C1, MAGIC_C2 | MAGIC_C3 };

static perf_counters_t pct;

uint8_t ph1[24],ph3_1;
uint8_t hp1,hp2,hp3[2],hp4;
uint8_t pstat[4];
int ph1pos,ph3pos,hp3pos;

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

#ifdef DEBUG_TRANSFERS
uint32_t checksum_h = 0;
uint32_t count_h = 0;
uint32_t checksum_p = 0;
uint32_t count_p = 0;
#endif

// The current Tube IRQ/NMI state is maintained in a global that the emulation code can see
//
// Bit 0 is the tube asserting irq
// Bit 1 is the tube asserting nmi

int tube_irq=0;

static int tube_enabled;

#ifdef DEBUG_TUBE

#define  TUBE_READ_MARKER 0x80000000
#define TUBE_WRITE_MARKER 0x40000000

unsigned int tube_index;
unsigned int tube_buffer[0x10000];

void tube_dump_buffer() {
   int i;
   LOG_INFO("tube_index = %d\r\n", tube_index);
   for (i = 0; i < tube_index; i++) {
      if (tube_buffer[i] & (TUBE_READ_MARKER | TUBE_WRITE_MARKER)) {
         if (tube_buffer[i] & TUBE_READ_MARKER) {
            LOG_INFO("Rd R");
         }
         if (tube_buffer[i] & TUBE_WRITE_MARKER) {
            LOG_INFO("Wr R");
         }
         // Covert address (1,3,5,7) to R1,R2,R3,R4
         LOG_INFO("%d = %02x\r\n", 1 + ((tube_buffer[i] & 0xF00) >> 9), tube_buffer[i] & 0xFF);
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

void tube_updateints()
{   
   tube_irq = 0;
   // Test for IRQ
   if ((HSTAT1 & HBIT_1) && (PSTAT1 & 128)) tube_irq  |= 1;
   if ((HSTAT1 & HBIT_2) && (PSTAT4 & 128)) tube_irq  |= 1;
   // Test for NMI
   if ((HSTAT1 & HBIT_3) && !(HSTAT1 & HBIT_4) && ((hp3pos > 0) || (ph3pos == 0))) tube_irq|=2;
   if ((HSTAT1 & HBIT_3) &&  (HSTAT1 & HBIT_4) && ((hp3pos > 1) || (ph3pos == 0))) tube_irq|=2;
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

void tube_host_read(uint16_t addr)
{
   int c;
   switch (addr & 7)
   {
   case 1: /*Register 1*/
      if (ph1pos > 0) {
         PH1_0 = BYTE_TO_WORD(ph1[1]);
         for (c = 1; c < 23; c++) ph1[c] = ph1[c + 1];
         ph1pos--;
         PSTAT1 |= 0x40;
         if (!ph1pos) HSTAT1 &= ~HBIT_7;
      }
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
         PSTAT3 |= 0x40;
         if (!ph3pos) HSTAT3 &= ~HBIT_7;
      }
      break;
   case 7: /*Register 4*/
      if (HSTAT4 & HBIT_7)
      {
         HSTAT4 &= ~HBIT_7;
         PSTAT4 |=  0x40;
      }
      break;
   }
   tube_updateints();
}

void tube_host_write(uint16_t addr, uint8_t val)
{
   if ((addr & 7) == 6) {
      copro = val;
      return;
   }
   if (!tube_enabled) {
      return;
   }
   switch (addr & 7)
   {
   case 0: /*Register 1 stat*/
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
      break;
   case 1: /*Register 1*/
      hp1 = val;
      PSTAT1 |=  0x80;
      HSTAT1 &= ~HBIT_6;
      break;
   case 3: /*Register 2*/
      hp2 = val;
      PSTAT2 |=  0x80;
      HSTAT2 &= ~HBIT_6;
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
            HSTAT3 &= ~HBIT_6;
         }
      }
      else
      {
         hp3[0] = val;
         hp3pos = 1;
         PSTAT3 |=  0x80;
         HSTAT3 &= ~HBIT_6;
      }
      break;
   case 7: /*Register 4*/
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
      break;
   default:
      LOG_WARN("Illegal host write to %d\r\n", addr);
   }
   tube_updateints();
}

uint8_t tube_parasite_read(uint32_t addr)
{
   uint8_t temp = 0;
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
      }
      break;
   }
   tube_updateints();
#ifdef DEBUG_TUBE
   if (addr & 1) {
      tube_buffer[tube_index++] = TUBE_READ_MARKER | ((addr & 7) << 8) | temp;
      tube_index &= 0xffff;
   }
#endif
   return temp;
}

void tube_parasite_write(uint32_t addr, uint8_t val)
{
#ifdef DEBUG_TUBE
   if (addr & 1) {
      tube_buffer[tube_index++] = TUBE_WRITE_MARKER | ((addr & 7) << 8) | val;
      tube_index &= 0xffff;
   }
#endif
   switch (addr & 7)
   {
   case 1: /*Register 1*/
      if (ph1pos < 24)
      {
         if (ph1pos == 0) {
            PH1_0 = BYTE_TO_WORD(val);
         } else {
            ph1[ph1pos] = val;
         }
         ph1pos++;
         HSTAT1 |= HBIT_7;
         if (ph1pos == 24) PSTAT1 &= ~0x40;
      }
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
      }
      else
      {
         PH3_0 = BYTE_TO_WORD(val);
         ph3pos = 1;
         HSTAT3 |=  HBIT_7;
         PSTAT3 &= ~0x40;
      }
      break;
   case 7: /*Register 4*/
      PH4 = BYTE_TO_WORD(val);
      HSTAT4 |=  HBIT_7;
      PSTAT4 &= ~0x40;
      break;
   }
   tube_updateints();
}

void tube_reset()
{
   tube_enabled = 1;
   ph1pos = hp3pos = 0;
   ph3pos = 1;
   HSTAT1 = HSTAT2 = HSTAT4 = HBIT_6;
   PSTAT1 = PSTAT2 = PSTAT3 = PSTAT4 = 0x40;
   HSTAT3 = HBIT_7 | HBIT_6;
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

#ifdef USE_HW_MAILBOX
   // Sequence numbers are currently 4 bits, and are stored in bits 12..15
   int act_seq_num;
   static int exp_seq_num = -1;
   act_seq_num = (mail >> 12) & 15;
#endif

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

#ifdef HAS_40PINS
  RPI_SetGpioPinFunction(TEST_PIN, FS_OUTPUT);
  RPI_SetGpioPinFunction(TEST2_PIN, FS_OUTPUT);
  RPI_SetGpioPinFunction(TEST3_PIN, FS_OUTPUT);
#endif

  // Configure GPIO to detect a rising edge of NRST
  // Current code does not reqire this, as emulators poll for NRST to be released
  // RPI_GpioBase->GPREN0 |= NRST_MASK;

#ifndef USE_GPU
  // Configure GPIO to detect a falling edge of NRST
  RPI_GpioBase->GPFEN0 |= NRST_MASK;
  // Make sure there are no pending detections
  RPI_GpioBase->GPEDS0 = NRST_MASK;

  // Configure GPIO to detect a falling edge of NTUBE
  RPI_GpioBase->GPFEN0 |= NTUBE_MASK;
  // Make sure there are no pending detections
  RPI_GpioBase->GPEDS0 = NTUBE_MASK;

  // This line enables IRQ interrupts
  // Enable gpio_int[0] which is IRQ 49
  //RPI_GetIrqController()->Enable_IRQs_2 = (1 << (49 - 32));

  // This line enables FIQ interrupts
  // Enable gpio_int[0] which is IRQ 49 as FIQ
  RPI_GetIrqController()->FIQ_control = 0x80 + 49;
#endif

  // Initialise the UART to 57600 baud
  RPI_AuxMiniUartInit( 115200, 8 );

#ifdef DEBUG
  dump_useful_info();
#endif

  // Precalculate the values that need to be written to the FSEL registers
  // to set the data bus GPIOs as inputs (idle) and output (driving)
  for (i = 0; i < 3; i++) {
     gpfsel_data_idle[i] = (uint32_t) RPI_GpioBase->GPFSEL[i];
     gpfsel_data_driving[i] = gpfsel_data_idle[i] | magic[i];
     LOG_DEBUG("%d %010o %010o\r\n", i, (unsigned int) gpfsel_data_idle[i], (unsigned int) gpfsel_data_driving[i]);
  }

  // Print the GPIO numbers of A0, A1 and A2
  LOG_DEBUG("A0 = GPIO%02d = mask %08x\r\n", A0_PIN, A0_MASK); 
  LOG_DEBUG("A1 = GPIO%02d = mask %08x\r\n", A1_PIN, A1_MASK); 
  LOG_DEBUG("A2 = GPIO%02d = mask %08x\r\n", A2_PIN, A2_MASK); 

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

#ifdef USE_GPU
   tube_regs = (uint32_t *) ARM_TUBE_REG_ADDR;
#else
   tube_regs = &tube_regs_block[0];
#endif

#ifndef USE_HW_MAILBOX
#ifdef USE_GPU
   tube_mailbox = (uint32_t *)(L2_CACHED_MEM_BASE + 0x20);
#else
   tube_mailbox = &tube_mailbox_block;
#endif
#endif
}

int tube_is_rst_active() {
   // It's necessary to keep servicing the tube_mailbox
   // otherwise a software reset sequence won't get handled properly
   if (is_mailbox_non_empty()) {
      unsigned int tube_mailbox_copy = read_mailbox();
      tube_io_handler(tube_mailbox_copy);
   }
   return ((RPI_GpioBase->GPLEV0 & NRST_MASK) == 0) || (tube_enabled && (HSTAT1 & HBIT_5));
}

void tube_wait_for_rst_active() {
   while (!tube_is_rst_active());
}

// Debounce RST

// On my Model B the characterisc of RST bounce on release is a bust
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
//#ifdef HAS_40PINS
//   RPI_SetGpioValue(TEST2_PIN, 1);
//#endif
   do {
      // Wait for reset to be released
      while (tube_is_rst_active());
      // Make sure RST stays continuously high for a further 100us
      for (i = 0; i < DEBOUNCE_TIME && !tube_is_rst_active(); i++);
      // Loop back if we exit the debouce loop prematurely because RST has gone active again
   } while (i < DEBOUNCE_TIME);
//#ifdef HAS_40PINS
//   RPI_SetGpioValue(TEST2_PIN, 0);
//#endif
   // Reset all the TUBE ULA registers
   tube_reset();
#ifndef USE_HW_MAILBOX
   // Clear any mailbox events that occurred during reset
   // Omit this and you sometimes see a second reset logged on reset release (65tube Co Pro)
   *tube_mailbox = 0;
#endif
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
   LOG_DEBUG("tube reset - copro %d\r\n", copro);
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
   tube_enabled = 0;
   for (i = 0; i < 8; i++) {
      tube_regs[i] = 0xfe;
   }
}

#ifdef USE_GPU
// todo : we need to sort out caches memory map etc here
void start_vc_ula()
{  int func,r0,r1, r2,r3,r4,r5;


   func = (int) &tubevc_asm[0];
   r0   = (int) GPU_TUBE_REG_ADDR;       // address of tube register block in IO space
   r1   = (int) &gpfsel_data_idle;       // gpfsel_data_idle
#ifdef USE_HW_MAILBOX
   r2   = 0;
#else
   r2   = (int) tube_mailbox;            // tube mailbox to be replaced later with VC->ARM mailbox
#endif
   r3   = (A2_PIN << 16) | (A1_PIN << 8) | (A0_PIN); // address bus GPIO mapping
   r4   = 0;
#ifdef HAS_40PINS
   r5   = TEST_MASK;                     // test pin
#else
   r5   = 0;
#endif
   // re-map to bus addresses
   // if the L2 cache is  enabled, the VC MMU maps physical memory to 0x40000000
   // if the L2 cache is disabled, the VC MMU maps physical memory to 0xC0000000
   // https://github.com/raspberrypi/firmware/wiki/Accessing-mailboxes
   if (r0) {
      r0 |= 0x40000000;
   }
   if (r2) {
      r2 |= 0x40000000;
   }
   LOG_DEBUG("VidCore code = %08x\r\n", func);
   LOG_DEBUG("VidCore   r0 = %08x\r\n", r0);
   LOG_DEBUG("VidCore   r1 = %08x\r\n", r1);
   LOG_DEBUG("VidCore   r2 = %08x\r\n", r2);
   LOG_DEBUG("VidCore   r3 = %08x\r\n", r3);
   LOG_DEBUG("VidCore   r4 = %08x\r\n", r4);
   LOG_DEBUG("VidCore   r5 = %08x\r\n", r5);   
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
#endif
