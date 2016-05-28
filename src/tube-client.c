#include <stdio.h>
#include <inttypes.h>
#include "copro-lib6502.h"
#include "copro-65tube.h"
#include "tube-defs.h"
#include "tube.h"
#include "tube-ula.h"
#include "startup.h"
#include "rpi-aux.h"
#include "cache.h"
#include "performance.h"

typedef void (*func_ptr)();

void emulator_not_implemented();

const char * emulator_names[] = {
   "ARM Native",
   "ARM2",
   "Beebdroid6502",
   "Lib6502",
   "65Tube",
   "80186",
   "32016"
};

const func_ptr emulator_functions[] = {
   emulator_not_implemented,
   emulator_not_implemented,
   emulator_not_implemented,
   copro_lib6502_emulator,
   copro_65tube_emulator,
   emulator_not_implemented,
   emulator_not_implemented
};

func_ptr emulator;

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

void start_core(int core, func_ptr func) {
   printf("starting core %d\r\n", core);
   *(unsigned int *)(0x4000008C + 0x10 * core) = (unsigned int) func;
}
#endif

void emulator_not_implemented() {
   printf("Co Pro has not been implemented yet\r\n");
   printf("Halted....\r\n");
   while (1);
}

void kernel_main(unsigned int r0, unsigned int r1, unsigned int atags)
{
#ifdef HAS_MULTICORE
   volatile int i;
#endif
   // TODO, read this from a command line param
   int copro = COPRO;

   tube_init_hardware();

   enable_MMU_and_IDCaches();
   _enable_unaligned_access();

  // Lock the Tube Interrupt handler into cache for BCM2835 based Pis
#if !defined(RPI2) && !defined(RPI3)
   lock_isr();
#endif

   _enable_interrupts();

   if (copro < 0 || copro >= sizeof(emulator_functions) / sizeof(func_ptr)) {
      printf("Co Pro %d has not been defined yet\r\n", copro);
      printf("Halted....\r\n");
      while (1);
   }

  printf("Raspberry Pi Direct %s Client\r\n", emulator_names[copro]);

  // Run a short set of CPU and Memory benchmarks
  benchmark();

  // There is a very wierd bug on the Zero if emulator is initialized earlier
  // It seems that putting a printf() between where emulator is initialized
  // and used causes the 65tube emulator to become flakey on language
  // transfer.
  //
  // This is *not* a code placement issue, as I had a test case where the
  // before and after code was the same overall length. To re-create this
  // test case, take the previous commit, and add one line before the
  // final emulator() which is: emulator = copro_65tube_emulator();
  //
  // For reference, here are the diffs
  //
  // < = last commit
  // > = test case described above
  //
  //359c359
  //<  1f003bc:	e92d4010 	push	{r4, lr}
  //---
  //>  1f003bc:	e92d4038 	push	{r3, r4, r5, lr}
  //365,368c365,368
  //<  1f003d4:	e59f2024 	ldr	r2, [pc, #36]	; 1f00400 <kernel_main+0x44>
  //<  1f003d8:	e59f3024 	ldr	r3, [pc, #36]	; 1f00404 <kernel_main+0x48>
  //<  1f003dc:	e59f4024 	ldr	r4, [pc, #36]	; 1f00408 <kernel_main+0x4c>
  //<  1f003e0:	e5921010 	ldr	r1, [r2, #16]
  //---
  //>  1f003d4:	e59f3024 	ldr	r3, [pc, #36]	; 1f00400 <kernel_main+0x44>
  //>  1f003d8:	e59f4024 	ldr	r4, [pc, #36]	; 1f00404 <kernel_main+0x48>
  //>  1f003dc:	e59f5024 	ldr	r5, [pc, #36]	; 1f00408 <kernel_main+0x4c>
  //>  1f003e0:	e5931010 	ldr	r1, [r3, #16]
  //370c370
  //<  1f003e8:	e5843000 	str	r3, [r4]
  //---
  //>  1f003e8:	e5845000 	str	r5, [r4]
  //373,375c373,375
  //<  1f003f4:	e5943000 	ldr	r3, [r4]
  //<  1f003f8:	e12fff33 	blx	r3
  //<  1f003fc:	e8bd8010 	pop	{r4, pc}
  //---
  //>  1f003f4:	e5845000 	str	r5, [r4]
  //>  1f003f8:	e8bd4038 	pop	{r3, r4, r5, lr}
  //>  1f003fc:	ea000a95 	b	1f02e58 <copro_65tube_emulator>
  //377,378c377,378
  //<  1f00404:	01f02e58 	mvnseq	r2, r8, asr lr
  //<  1f00408:	01f601a0 	mvnseq	r0, r0, lsr #3
  //---
  //>  1f00404:	01f601a0 	mvnseq	r0, r0, lsr #3
  //>  1f00408:	01f02e58 	mvnseq	r2, r8, asr lr
  //
  // I spent a couple of hours trying to understand what's going wrong here
  // and completely failed.

  // Hence, this workaround which will probably come back and bite!

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
