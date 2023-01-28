#include <stdio.h>
#include <stdint.h>

#include "copro-defs.h"
#include "copro-65tube.h"
#include "tube-client.h"

#ifndef MINIMAL_BUILD

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
#include "copro-f100.h"
#include "copro-65816.h"
#include "copro-pdp11.h"
#include "copro-armnative.h"
#include "copro-65tubejit.h"

#endif

#ifdef INCLUDE_DEBUGGER

#include "lib6502_debug.h"
#include "yaze/simz80.h"
#include "cpu80186/cpu80186_debug.h"
#include "mc6809nc/mc6809_debug.h"
#include "mame/arm_debug.h"
#include "NS32016/32016_debug.h"
#include "opc5ls/opc5ls_debug.h"
#include "opc6/opc6_debug.h"
#include "opc7/opc7_debug.h"
#include "pdp11/pdp11_debug.h"
#include "f100/f100_debug.h"
#include "65816/65816_debug.h"

#define DEBUGGER(n) (n)
#define NO_DEBUGGER (NULL)

#else

#define DEBUGGER(n)
#define NO_DEBUGGER

#endif

#define DEFAULT_COPRO 0

const copro_def_t copro_defs[] = {
   {
      "65C02 (fast)",           // 0
      copro_65tube_emulator,
      TYPE_65TUBE_0,
      NO_DEBUGGER
   },
   {
      "65C02",                  // 1
      copro_65tube_emulator,
      TYPE_65TUBE_1,
      NO_DEBUGGER
   },
   {
      "65C102 (fast)",          // 2
      copro_65tube_emulator,
      TYPE_65TUBE_2,
      NO_DEBUGGER
   },
   {
      "65C102",                 // 3
      copro_65tube_emulator,
      TYPE_65TUBE_3,
      NO_DEBUGGER
   }
#ifndef MINIMAL_BUILD
   ,
   {
      "Z80 (1.21)",             // 4
      copro_z80_emulator,
      TYPE_GENERIC,
      DEBUGGER(&simz80_cpu_debug)
   },
   {
      "Z80 (2.00)",             // 5
      copro_z80_emulator,
      TYPE_GENERIC,
      DEBUGGER(&simz80_cpu_debug)
   },
   {
      "Z80 (2.2c)",             // 6
      copro_z80_emulator,
      TYPE_GENERIC,
      DEBUGGER(&simz80_cpu_debug)
   },
   {
      "Z80 (2.30)",             // 7
      copro_z80_emulator,
      TYPE_GENERIC,
      DEBUGGER(&simz80_cpu_debug)
   },
   {
      "80286",                  // 8
      copro_80186_emulator,
      TYPE_80X86,
      DEBUGGER(&cpu80186_cpu_debug)
   },
   {
      "MC6809",                 // 9
      copro_mc6809nc_emulator,
      TYPE_GENERIC,
      DEBUGGER(&mc6809nc_cpu_debug)
   },
   {
      "Null",                   // 10
      copro_null_emulator,
      TYPE_HIDDEN,
      NO_DEBUGGER
   },
   {
      "PDP-11",                 // 11
      copro_pdp11_emulator,
      TYPE_GENERIC,
      DEBUGGER(&pdp11_cpu_debug)
   },
   {
      "ARM2",                   // 12
      copro_arm2_emulator,
      TYPE_GENERIC,
      DEBUGGER(&arm2_cpu_debug)
   },
   {
      "32016",                  // 13
      copro_32016_emulator,
      TYPE_32016,
      DEBUGGER(&n32016_cpu_debug)
   },
   {
      "Disable",                // 14 - same as Null but we want it in the list
      copro_null_emulator,
      TYPE_DISABLED,
      NO_DEBUGGER
   },
   {
      "ARM Native",             // 15
      copro_armnative_emulator,
      TYPE_ARMNATIVE,
      NO_DEBUGGER
   },
   {
      "LIB65C02 64K",           // 16
      copro_lib6502_emulator,
      TYPE_GENERIC,
      DEBUGGER(&lib6502_cpu_debug)
   },
   {
      "LIB65C02 256K Turbo",    // 17
      copro_lib6502_emulator,
      TYPE_TURBO,
      DEBUGGER(&lib6502_cpu_debug)
   },
   {
      "65C816 (Dossy)",         // 18
      copro_65816_emulator,
      TYPE_GENERIC,
      DEBUGGER(&w65816_cpu_debug)
   },
   {
      "65C816 (ReCo)",          // 19
      copro_65816_emulator,
      TYPE_GENERIC,
      DEBUGGER(&w65816_cpu_debug)
   },
   {
      "OPC5LS",                 // 20
      copro_opc5ls_emulator,
      TYPE_GENERIC,
      DEBUGGER(&opc5ls_cpu_debug)
   },
   {
      "OPC6",                   // 21
      copro_opc6_emulator,
      TYPE_GENERIC,
      DEBUGGER(&opc6_cpu_debug)
   },
   {
      "OPC7",                   // 22
      copro_opc7_emulator,
      TYPE_GENERIC,
      DEBUGGER(&opc7_cpu_debug)
   },
   {
      "Null",                   // 23
      copro_null_emulator,
      TYPE_HIDDEN,
      NO_DEBUGGER
   },
   {
      "65C02 (JIT)",            // 24
      copro_65tubejit_emulator,
      TYPE_65TUBE_0,
      NO_DEBUGGER
   },
   {
      "Null",                   // 25
      copro_null_emulator,
      TYPE_HIDDEN,
      NO_DEBUGGER
   },
   {
      "Null",                   // 26
      copro_null_emulator,
      TYPE_HIDDEN,
      NO_DEBUGGER
   },
   {
      "Null",                   // 27
      copro_null_emulator,
      TYPE_HIDDEN,
      NO_DEBUGGER
   },
   {
      "Ferranti F100-L",        // 28
      copro_f100_emulator,
      TYPE_GENERIC,
      DEBUGGER(&f100_cpu_debug)
   },
   {
      "Null",                   // 29
      copro_null_emulator,
      TYPE_HIDDEN,
      NO_DEBUGGER
   },
   {
      "Null",                   // 30
      copro_null_emulator,
      TYPE_HIDDEN,
      NO_DEBUGGER
   },
   {
      "Null",                   // 31
      copro_null_emulator,
      TYPE_HIDDEN,
      NO_DEBUGGER
   }
#endif
};

unsigned int default_copro() {
   return DEFAULT_COPRO;
}

unsigned int num_copros() {
   return sizeof(copro_defs) / sizeof(copro_def_t);
}

char *get_copro_name(unsigned int i, unsigned int maxlen) {
   __attribute__ ((section (".noinit"))) static char name[256];
   if (i == 1u || i == 3u) {
      sprintf(name, "%s (%uMHz)", copro_defs[i].name, get_copro_mhz(i));
   } else {
      sprintf(name, "%s", copro_defs[i].name);
   }
   // Make sure name doesn't exceed the required length
   name[maxlen] = '\0';
   return name;
}
