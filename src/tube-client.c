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
#include "rpi-gpio.h"

#ifdef INCLUDE_DEBUGGER
#include "cpu_debug.h"
#endif

typedef void (*func_ptr)();

extern int test_pin;

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
#include "copro-opc5ls.h"
#include "copro-opc6.h"
#include "copro-opc7.h"
#include "copro-pdp11.h"
#include "copro-armnative.h"

#ifdef DEBUG
static const char * emulator_names[] = {
   "65C02 (65tube)",
   "65C02 (65tube)",
   "65C02 (lib6502)",
   "65C02 (lib6502)",
   "Z80",
   "OPC5LS",
   "OPC6",
   "OPC7",
   "80286",
   "MC6809",
   "Null",
   "PDP-11",
   "ARM2",
   "32016",
   "Null",
   "ARM Native"
};
#endif

static const func_ptr emulator_functions[] = {
   copro_65tube_emulator,
   copro_65tube_emulator,
#if 1   
   copro_lib6502_emulator,
   copro_lib6502_emulator,
#else
   copro_65tube_emulator,
   copro_65tube_emulator,
#endif
   copro_z80_emulator,
   copro_opc5ls_emulator,
   copro_opc6_emulator,
   copro_opc7_emulator,
   copro_80186_emulator,
   copro_mc6809nc_emulator,
   copro_null_emulator,
   copro_pdp11_emulator,
   copro_arm2_emulator,
   copro_32016_emulator,
   copro_null_emulator,
   copro_armnative_emulator
};

#endif

volatile unsigned int copro;
volatile unsigned int copro_speed;
volatile unsigned int copro_memory_size = 0;
unsigned int tube_delay = 0;

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
#pragma GCC diagnostic ignored "-Wnonnull"   
   memset(mpu_memory, 0, length);
#pragma GCC diagnostic pop   
   // return pointer to memory
   return mpu_memory;
}

void init_emulator() {
   _disable_interrupts();
   
      // Set up FIQ handler 
   
   tube_irq = 0; // Make sure everything is clear
   *((uint32_t *) FIQ_VECTOR) = (uint32_t) arm_fiq_handler_flag1;
   
   // Direct Mail box to FIQ handler
   
   (*(volatile uint32_t *)MBOX0_CONFIG) = MBOX0_DATAIRQEN;
   (*(volatile uint32_t *)FIQCTRL) = 0x80 +65;       
   

#ifndef MINIMAL_BUILD
   if (copro == COPRO_ARMNATIVE) {
      *((uint32_t *) SWI_VECTOR) = (uint32_t) copro_armnative_swi_handler;
      *((uint32_t *) FIQ_VECTOR) = (uint32_t) copro_armnative_fiq_handler;
   }
#endif

   copro &= 127 ; // Clear top bit which is used to signal full reset 
   // Make sure that copro number is valid
   if (copro >= sizeof(emulator_functions) / sizeof(func_ptr)) {
      LOG_DEBUG("using default co pro\r\n");
      copro = DEFAULT_COPRO;
   }

   LOG_DEBUG("Raspberry Pi Direct %u %s Client\r\n", copro,emulator_names[copro]);

   emulator = emulator_functions[copro];

#ifdef INCLUDE_DEBUGGER
   // reinitialize the debugger as the Co Pro has changed
   debug_init();
#endif
   
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

static void get_copro_memory_size() {
   char *copro_prop = get_cmdline_prop("copro13_memory_size");
   copro_memory_size = 0; // default 
   if (copro_prop) {
      copro_memory_size = atoi(copro_prop);
   }
   if (copro_memory_size > 32*1024 *1024){
      copro_memory_size = 0;
   }
   LOG_DEBUG("Copro Memory size %u\r\n", copro_memory_size);
}

static void get_tube_delay() {
   char *copro_prop = get_cmdline_prop("tube_delay");
   tube_delay = 0; // default 
   if (copro_prop) {
      tube_delay = atoi(copro_prop);
   }
   if (tube_delay > 40){
      tube_delay = 40;
   }
   LOG_DEBUG("Tube ULA sample delay  %u\r\n", tube_delay);
}

void kernel_main(unsigned int r0, unsigned int r1, unsigned int atags)
{
     // Initialise the UART to 57600 baud
   RPI_AuxMiniUartInit( 115200, 8 );
   enable_MMU_and_IDCaches();
   _enable_unaligned_access();
   tube_init_hardware();

   arm_speed = get_clock_rate(ARM_CLK_ID);
   get_tube_delay();
   start_vc_ula();

   copro = get_copro_number();
   get_copro_speed();
   get_copro_memory_size();
   
   
#ifdef BENCHMARK
  // Run a short set of CPU and Memory benchmarks
  benchmark();
#endif

#ifdef HAS_MULTICORE
  LOG_DEBUG("main running on core %u\r\n", _get_core());
  start_core(1, _spin_core);
  start_core(2, _spin_core);
  start_core(3, _spin_core);
#endif
  init_emulator();

  RPI_GpioBase->GPSET0 = (1 << test_pin);

  do {
     // Run the emulator
     emulator();

     // Reload the emulator as copro may have changed
     init_emulator();
     
  } while (1);
  
}
