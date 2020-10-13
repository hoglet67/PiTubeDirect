#ifndef COPRO_DEFS_H
#define COPRO_DEFS_H

#ifdef INCLUDE_DEBUGGER
#include "cpu_debug.h"
#endif

typedef void (*func_ptr)();

// Certain Co Pro are typed, allowing code to special case them

#define TYPE_GENERIC        0   // Catch-all for everything else
#define TYPE_HIDDEN         1   // Don't include in the Co Pro listing
#define TYPE_65TUBE_0       2
#define TYPE_65TUBE_1       3
#define TYPE_65TUBE_2       4
#define TYPE_65TUBE_3       5
#define TYPE_80X86          6
#define TYPE_32016          7
#define TYPE_ARMNATIVE      8
#define TYPE_TURBO          9

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
   const int          type;
#ifdef INCLUDE_DEBUGGER
   const cpu_debug_t *debugger;
#endif
} copro_def_t;

extern copro_def_t copro_defs[];

extern int num_copros();

extern int default_copro();

#endif
