#ifndef COPRO_DEFS_H
#define COPRO_DEFS_H

#ifdef INCLUDE_DEBUGGER
#include "cpu_debug.h"
#endif

typedef void (*func_ptr)();

// Certain Co Pro are typed, allowing code to special case them

typedef enum types {
   TYPE_GENERIC,  // Catch-all for everything else
   TYPE_DISABLED, // Disable tube but include in the Co Pro listing
   TYPE_HIDDEN,   // Disable tube and don't include in the Co Pro listing
   TYPE_65TUBE_0,
   TYPE_65TUBE_1,
   TYPE_65TUBE_2,
   TYPE_65TUBE_3,
   TYPE_80X86,
   TYPE_32016,
   TYPE_ARMNATIVE,
   TYPE_TURBO
} copro_type_t;

// Note: We still have cmdline.txt properties like
//
//   copro1_speed
//   copro3_speed
//   copro8_memory_size
//   copro13_memory_size
//
// TODO: we should possibly rethink these

typedef struct {
   const char        *name;
   const func_ptr     emulator;
   const copro_type_t type;
#ifdef INCLUDE_DEBUGGER
   const cpu_debug_t *debugger;
#endif
} copro_def_t;

extern copro_def_t copro_defs[];

extern unsigned int num_copros();

extern unsigned int default_copro();

char *get_copro_name(unsigned int i, unsigned int maxlen);

#endif
