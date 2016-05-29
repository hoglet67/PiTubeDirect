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

typedef void (*func_ptr)();

static void emulator_not_implemented();

static const char * emulator_names[] = {
   "Undefined",
   "ARM Native",
   "ARM2",
   "Beebdroid6502",
   "Lib6502",
   "65Tube",
   "80186",
   "32016"
};

static const func_ptr emulator_functions[] = {
   emulator_not_implemented,
   emulator_not_implemented,
   emulator_not_implemented,
   emulator_not_implemented,
   copro_lib6502_emulator,
   copro_65tube_emulator,
   copro_80186_emulator,
   emulator_not_implemented
};

static func_ptr emulator;

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
   
   printf("test running on core %d\r\n", i);
   emulator();
}

static void start_core(int core, func_ptr func) {
   printf("starting core %d\r\n", core);
   *(unsigned int *)(0x4000008C + 0x10 * core) = (unsigned int) func;
}
#endif

static void emulator_not_implemented() {
   printf("Co Pro has not been implemented yet\r\n");
   printf("Halted....\r\n");
   while (1);
}

int get_copro_number() {
   int copro = 0;
   char *copro_prop = get_cmdline_prop("copro");
   if (copro_prop) {
      copro = atoi(copro_prop);
   }
   if (!copro) {
      copro = COPRO;
   }
   return copro;
}

void kernel_main(unsigned int r0, unsigned int r1, unsigned int atags)
{
#ifdef HAS_MULTICORE
   volatile int i;
#endif
   int copro;

   tube_init_hardware();

   enable_MMU_and_IDCaches();
   _enable_unaligned_access();

  // Lock the Tube Interrupt handler into cache for BCM2835 based Pis
#if !defined(RPI2) && !defined(RPI3)
   lock_isr();
#endif

   _enable_interrupts();

   // TODO: need to fix tube.S using COPRO

   copro = get_copro_number();

   if (copro < 0 || copro >= sizeof(emulator_functions) / sizeof(func_ptr)) {
      printf("Co Pro %d has not been defined yet\r\n", copro);
      printf("Halted....\r\n");
      while (1);
   }

  printf("Raspberry Pi Direct %s Client\r\n", emulator_names[copro]);

  // Run a short set of CPU and Memory benchmarks
  benchmark();

  emulator = emulator_functions[copro];

#ifdef HAS_MULTICORE

  printf("main running on core %d\r\n", _get_core());
  for (i = 0; i < 100000000; i++);
  start_core(1, _spin_core);
  for (i = 0; i < 100000000; i++);
  start_core(2, _spin_core);
  for (i = 0; i < 100000000; i++);

#ifdef USE_MULTICORE
  start_core(3, _init_core);
  while (1);
#else
  start_core(3, _spin_core);
  for (i = 0; i < 100000000; i++);
  emulator();
#endif

#else

  emulator();

#endif
}
