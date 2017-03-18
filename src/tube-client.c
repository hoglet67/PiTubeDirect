#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include "tube-defs.h"
#include "tube.h"
#include "tube-ula.h"
#include "startup.h"
#include "rpi-aux.h"
#include "cache.h"
#include "performance.h"
#include "info.h"

typedef void (*func_ptr)();

#include "copro-65tube.h"

#ifdef MINIMAL_BUILD

#ifdef DEBUG
static const char * emulator_names[] = {
   "65C02 (65tube)"
};
#endif

static const func_ptr emulator_functions[] = {
   copro_65tube_emulator
};

#else

#include "copro-lib6502.h"
#include "copro-80186.h"
#include "copro-arm2.h"
#include "copro-32016.h"
#include "copro-null.h"
#include "copro-z80.h"
#include "copro-mc6809nc.h"
#include "copro-armnative.h"

#ifdef DEBUG
static const char * emulator_names[] = {
   "65C02 (65tube)",
   "65C02 (65tube)",
   "65C02 (lib6502)",
   "65C02 (lib6502)",
   "Z80",
   "Z80",
   "Z80",
   "Z80",
   "80286",
   "MC6809 (Neal Crook)",
   "Null/SPI",
   "Null/SPI",
   "ARM2",
   "32016",
   "Null/SPI",
   "ARM Native"
};
#endif

static const func_ptr emulator_functions[] = {
   copro_65tube_emulator,
   copro_65tube_emulator,
#if DEBUG   
   copro_lib6502_emulator,
   copro_lib6502_emulator,
#else
   copro_65tube_emulator,
   copro_65tube_emulator,
#endif
   copro_z80_emulator,
   copro_z80_emulator,
   copro_z80_emulator,
   copro_z80_emulator,
   copro_80186_emulator,
   copro_mc6809nc_emulator,
   copro_null_emulator,
   copro_null_emulator,
   copro_arm2_emulator,
   copro_32016_emulator,
   copro_null_emulator,
   copro_armnative_emulator
};

#endif

volatile unsigned int copro;
volatile unsigned int copro_speed;
int arm_speed;

static func_ptr emulator;

// This magic number come form cache.c where we have relocated the vectors to 
// Might be better to just read the vector pointer register instead.
#define SWI_VECTOR (HIGH_VECTORS_BASE + 0x28)
#define FIQ_VECTOR (HIGH_VECTORS_BASE + 0x3C)

unsigned char * copro_mem_reset(int length)
{
     // Wipe memory
     // Memory starts at zero now vectors have moved.
   unsigned char * mpu_memory = 0;  
   memset(mpu_memory, 0, length);
   // return pointer to memory
   return mpu_memory;
}

void init_emulator() {
   _disable_interrupts();

#ifndef MINIMAL_BUILD
   if (copro == COPRO_ARMNATIVE) {
      *((uint32_t *) SWI_VECTOR) = (uint32_t) copro_armnative_swi_handler;
      *((uint32_t *) FIQ_VECTOR) = (uint32_t) copro_armnative_fiq_handler;
   }
#endif

   // Make sure that copro number is valid
   if (copro >= sizeof(emulator_functions) / sizeof(func_ptr)) {
      LOG_DEBUG("using default co pro\r\n");
      copro = DEFAULT_COPRO;
   }

   LOG_DEBUG("Raspberry Pi Direct %u %s Client\r\n", copro,emulator_names[copro]);

   emulator = emulator_functions[copro];
   
   _enable_interrupts();
}

#ifdef HAS_MULTICORE
void run_core() {
   // Write first line without using printf
   // In case the VFP unit is not enabled
#ifdef DEBUG
   int i;
	RPI_AuxMiniUartWrite('C');
	RPI_AuxMiniUartWrite('O');
	RPI_AuxMiniUartWrite('R');
	RPI_AuxMiniUartWrite('E');
   i = _get_core();
	RPI_AuxMiniUartWrite('0' + i);
	RPI_AuxMiniUartWrite('\r');
	RPI_AuxMiniUartWrite('\n');
#endif
   
   enable_MMU_and_IDCaches();
   _enable_unaligned_access();

#ifdef DEBUG   
   LOG_DEBUG("emulator running on core %d\r\n", i);
#endif

   do {
      // Run the emulator
      emulator();

      // Reload the emulator as copro may have changed
      init_emulator();
     
   } while (1);
}

static void start_core(int core, func_ptr func) {
   LOG_DEBUG("starting core %d\r\n", core);
   *(unsigned int *)(0x4000008C + 0x10 * core) = (unsigned int) func;
}
#endif


static unsigned int get_copro_number() {
   unsigned int copro = DEFAULT_COPRO;
   char *copro_prop = get_cmdline_prop("copro");
   
   if (copro_prop) {
      copro = atoi(copro_prop);
   }
   if (copro >= sizeof(emulator_functions) / sizeof(func_ptr)){
      copro = DEFAULT_COPRO;
   }
   return copro;
}

static void get_copro_speed() {
   char *copro_prop = get_cmdline_prop("copro1_speed");
   copro_speed = 3; // default to 3MHz 
   if (copro_prop) {
      copro_speed = atoi(copro_prop);
   }
   if (copro_speed > 255){
      copro_speed = 0;
   }
   LOG_DEBUG("emulator speed %u\r\n", copro_speed);
   if (copro_speed !=0)
      copro_speed = (arm_speed/(1000000/256) / copro_speed);
}


void kernel_main(unsigned int r0, unsigned int r1, unsigned int atags)
{
#ifdef HAS_MULTICORE
   volatile int i;
#endif

   tube_init_hardware();
   arm_speed = get_clock_rate(ARM_CLK_ID);
   copro = get_copro_number();
   get_copro_speed();
   
#ifdef USE_GPU
      LOG_DEBUG("Staring VC ULA\r\n");
      start_vc_ula();
      LOG_DEBUG("Done\r\n");
#endif

   enable_MMU_and_IDCaches();
   _enable_unaligned_access();

#ifdef DEBUG
  // Run a short set of CPU and Memory benchmarks
  benchmark();
#endif

  init_emulator();

  // Lock the Tube Interrupt handler into cache for BCM2835 based Pis
#if !defined(RPI2) && !defined(RPI3) && !defined(USE_GPU)
   lock_isr();
#endif

#ifdef HAS_MULTICORE

  LOG_DEBUG("main running on core %u\r\n", _get_core());
  for (i = 0; i < 10000000; i++);
  start_core(1, _spin_core);
  for (i = 0; i < 10000000; i++);
  start_core(2, _spin_core);
  for (i = 0; i < 10000000; i++);

#ifdef USE_MULTICORE
  start_core(3, _init_core);
  while (1);
#else
  start_core(3, _spin_core);
  for (i = 0; i < 10000000; i++);
#endif

#endif

#ifndef USE_MULTICORE

  do {
     // Run the emulator
     emulator();

     // Reload the emulator as copro may have changed
     init_emulator();
     
  } while (1);

#endif
}
