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
#include "rpi-interrupts.h"

#ifdef INCLUDE_DEBUGGER
#include "cpu_debug.h"
#endif

#include "copro-defs.h"

#include "copro-armnative.h"

extern int test_pin;

volatile unsigned int copro;
volatile unsigned int copro_speed;
volatile unsigned int copro_memory_size = 0;
unsigned int tube_delay = 0;

int arm_speed;

static copro_def_t *copro_def;

// This magic number come form cache.c where we have relocated the vectors to
// Might be better to just read the vector pointer register instead.
#define SWI_VECTOR ( _software_interrupt_vector_h)
#define FIQ_VECTOR ( _fast_interrupt_vector_h)

unsigned char * copro_mem_reset(int length)
{
     // Wipe memory
     // Memory starts at zero now vectors have moved.
   unsigned char * mpu_memory = 0;
#pragma GCC diagnostic ignored "-Wnonnull"
     // cppcheck-suppress nullPointer
   memset(mpu_memory, 0, length);
#pragma GCC diagnostic pop
   // return pointer to memory
   return mpu_memory;
}

void init_emulator() {
   _disable_interrupts();

   // Make sure that copro number is valid
   if (copro >= num_copros()) {
      LOG_DEBUG("using default co pro\r\n");
      copro = default_copro();
   }

   // Lookup the copro definition struct
   copro_def = &copro_defs[copro];

   LOG_DEBUG("Raspberry Pi Direct %u %s Client\r\n", copro, copro_def->name);


    tube_irq = 0; // Make sure everything is clear
	// Set up FIQ handler

   FIQ_VECTOR = (uint32_t) arm_fiq_handler_flag1;
   _data_memory_barrier();
   (*(volatile uint32_t *)MBOX0_CONFIG) = MBOX0_DATAIRQEN;
    // Direct Mail box to FIQ handler

#if defined(RPI4)
    RPI_GetIrqController()->Enable_Basic_FIQs = RPI_BASIC_ARM_MAILBOX_IRQ ;
#else
    RPI_GetIrqController()->FIQ_control = 0x80 +65;
#endif

    _data_memory_barrier();

#ifndef MINIMAL_BUILD
   if (copro_def->type == TYPE_ARMNATIVE) {
      SWI_VECTOR = (uint32_t) copro_armnative_swi_handler;
      FIQ_VECTOR = (uint32_t) copro_armnative_fiq_handler;
   }
#endif


#ifdef INCLUDE_DEBUGGER
   // reinitialize the debugger as the Co Pro has changed
   debug_init();
#endif

   _enable_interrupts();
}

#ifdef HAS_MULTICORE

#if 0

// DMB: This method is no longer ever called, because we only
// ever run the emulation on the main core.

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
      copro_def->emulator(copro_def->type);

      // Reload the emulator as copro may have changed
      init_emulator();

   } while (1);
}
#endif

static void start_core(int core, func_ptr func) {
   LOG_DEBUG("starting core %d\r\n", core);
   *(unsigned int *)(0x4000008C + 0x10 * core) = (unsigned int) func;
}
#endif

static unsigned int get_copro_number() {
   unsigned int copro = default_copro();
   char *copro_prop = get_cmdline_prop("copro");

   if (copro_prop) {
      copro = atoi(copro_prop);
   }
   if (copro >= num_copros()) {
      copro = default_copro();
   }
   return copro;
}

unsigned int get_copro_mhz(int copro_num) {
   unsigned int copro_mhz = 0; // default
   char *copro_prop = NULL;
   // Note: Co Pro Speed is only implemented in the 65tube Co Processors (copros 0/1/2/3)
   if (copro_defs[copro_num].type == TYPE_65TUBE_1) {
      copro_mhz = 3; // default to 3MHz (65C02)
      copro_prop = get_cmdline_prop("copro1_speed");
   } else if (copro_defs[copro_num].type == TYPE_65TUBE_3) {
      copro_mhz = 4; // default to 4MHz (65C102)
      copro_prop = get_cmdline_prop("copro3_speed");
   }
   if (copro_prop) {
      copro_mhz = atoi(copro_prop);
   }
   if (copro_mhz > 255) {
      copro_mhz = 0;
   }
   return copro_mhz;
}

static void get_copro_speed() {
   copro_speed = get_copro_mhz(copro);
   LOG_DEBUG("emulator speed %u\r\n", copro_speed);
   if (copro_speed !=0) {
      copro_speed = (arm_speed / (1000000 / 256) / copro_speed);
   }
}

static void get_copro_memory_size() {
   char *copro_prop = NULL;
   copro_memory_size = 0; // default
   // Note: Co Pro Memory Size is only implemented in the 80286 and 32016 Coprocessors (copros 8/13)
   if (copro_def->type == TYPE_80X86) {
      copro_prop = get_cmdline_prop("copro8_memory_size");
   } else if (copro_def->type == TYPE_32016) {
      copro_prop = get_cmdline_prop("copro13_memory_size");
   }
   if (copro_prop) {
      copro_memory_size = atoi(copro_prop);
   }
   if (copro_memory_size > 32*1024*1024) {
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
   int last_copro = -1;

     // Initialise the UART to 57600 baud
   RPI_AuxMiniUartInit( 115200, 8 );
   enable_MMU_and_IDCaches();
   _enable_unaligned_access();
   tube_init_hardware();

   arm_speed = get_clock_rate(ARM_CLK_ID);
   get_tube_delay();
   start_vc_ula();

   copro = get_copro_number();

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

     // When changing Co Processors, reset the Co Pro speed and memory back to the
     // configured default for that Co Pro.
     if (copro != last_copro) {
        get_copro_speed();
        get_copro_memory_size();
     }
     last_copro = copro;

     // Run the emulator
     copro_def->emulator(copro_def->type);

     // Clear top bit which is used to signal full reset
     copro &= 127;

     // Reload the emulator as copro may have changed
     init_emulator();

  } while (1);

}
