#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include "tube-defs.h"
#include "tube.h"
#include "tube-ula.h"
#include "startup.h"
#include "rpi-aux.h"
#include "cache.h"
#include "performance.h"
#include "info.h"

#include "copro-lib6502.h"
#include "copro-65tube.h"
#include "copro-80186.h"
#include "copro-arm2.h"
#include "copro-32016.h"
#include "copro-null.h"

typedef void (*func_ptr)();


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
   "6809",
   "68000",
   "PDP11",
   "ARM2",
   "32016",
   "Null/SPI",
   "BIST"
};

static const func_ptr emulator_functions[] = {
   copro_65tube_emulator,
   copro_lib6502_emulator,
   copro_65tube_emulator,
   copro_lib6502_emulator,
   copro_null_emulator,
   copro_null_emulator,
   copro_null_emulator,
   copro_null_emulator,
   copro_80186_emulator,
   copro_null_emulator,
   copro_null_emulator,
   copro_null_emulator,
   copro_arm2_emulator,
   copro_32016_emulator,
   copro_null_emulator,
   copro_null_emulator
};

volatile int copro;

static func_ptr emulator;

void init_emulator() {
   _disable_interrupts();

   // Default to the normal FIQ handler
   *((uint32_t *) 0x3C) = (uint32_t) arm_fiq_handler_flag0;
#ifndef USE_MULTICORE   
   // When the 65tube co pro on a single core system, switch to the alternative FIQ handler
   // that flag events from the ISR using the ip register
   if (copro == COPRO_65TUBE_0 || copro == COPRO_65TUBE_1) {
      *((uint32_t *) 0x3C) = (uint32_t) arm_fiq_handler_flag1;
   }
#endif

   // Make sure that copro number is valid
   if (copro < 0 || copro >= NUM_COPROS) {
      printf("using default co pro\r\n");
      copro = DEFAULT_COPRO;
   }

   printf("Raspberry Pi Direct %s Client\r\n", emulator_names[copro]);

   emulator = emulator_functions[copro];
   
   _enable_interrupts();
}

#ifdef HAS_MULTICORE
void run_core() {
   int i;
   // Write first line without using printf
   // In case the VFP unit is not enabled
	RPI_AuxMiniUartWrite('C');
	RPI_AuxMiniUartWrite('O');
	RPI_AuxMiniUartWrite('R');
	RPI_AuxMiniUartWrite('E');
   i = _get_core();
	RPI_AuxMiniUartWrite('0' + i);
	RPI_AuxMiniUartWrite('\r');
	RPI_AuxMiniUartWrite('\n');
   
   enable_MMU_and_IDCaches();
   _enable_unaligned_access();
   
   printf("emulator running on core %d\r\n", i);

   do {
      // Run the emulator
      emulator();

      // Reload the emulator as copro may have changed
      init_emulator();
     
   } while (1);
}

static void start_core(int core, func_ptr func) {
   printf("starting core %d\r\n", core);
   *(unsigned int *)(0x4000008C + 0x10 * core) = (unsigned int) func;
}
#endif


int get_copro_number() {
   int copro = -1;
   char *copro_prop = get_cmdline_prop("copro");
   if (copro_prop) {
      copro = atoi(copro_prop);
   }
   if (copro < 0 || copro >= NUM_COPROS) {
      copro = DEFAULT_COPRO;
   }
   return copro;
}

void kernel_main(unsigned int r0, unsigned int r1, unsigned int atags)
{
#ifdef HAS_MULTICORE
   volatile int i;
#endif

   tube_init_hardware();

   enable_MMU_and_IDCaches();
   _enable_unaligned_access();


   copro = get_copro_number();

   if (copro < 0 || copro >= sizeof(emulator_functions) / sizeof(func_ptr)) {
      printf("Co Pro %d has not been defined yet\r\n", copro);
      printf("Halted....\r\n");
      while (1);
   }

  // Run a short set of CPU and Memory benchmarks
  benchmark();

  init_emulator();

  // Lock the Tube Interrupt handler into cache for BCM2835 based Pis
#if !defined(RPI2) && !defined(RPI3)
   lock_isr();
#endif

#ifdef HAS_MULTICORE

  printf("main running on core %d\r\n", _get_core());
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
  emulator();
#endif

#else

  do {
     // Run the emulator
     emulator();

     // Reload the emulator as copro may have changed
     init_emulator();
     
  } while (1);

#endif
}
