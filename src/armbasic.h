// armbasic.h

#ifndef ARMBASIC_H
#define ARMBASIC_H

// Define BAS135H_ROM to switch to BeebMasters PAGE=&10000
// version. This was needed at one point because the original
// PAGE=&8F00 version would end up with a corrupted BASIC programs on
// Ctrl-BREAK dure to the language transfer. But there were
// compatibility issues. So instead we supressed the language
// transfer.

// #define BAS135H_ROM
extern unsigned int _start ;
#define ARM_BASIC_START (_start-0x00C00000)

// Note, the two version have slightly different start addresses
#ifdef BAS135H_ROM
#define ARM_BASIC_EXEC  (ARM_BASIC_START + 0x10C)
#else
#define ARM_BASIC_EXEC  (ARM_BASIC_START + 0x104)
#endif

// Flag to indicate if ARM Basic was active before reset
extern int armbasic;

// Install ARM Basic into Memory
extern void copy_armbasic();

#endif
