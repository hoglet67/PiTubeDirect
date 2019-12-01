// copro-lib6502.h
#ifndef COPRO_LIB6502_H
#define COPRO_LIB6502_H

#ifdef INCLUDE_DEBUGGER
#include "lib6502.h"
extern M6502 *copro_lib6502_mpu;
extern int copro_lib6502_mem_read(M6502 *mpu, addr_t addr, uint8_t data);
extern int copro_lib6502_mem_write(M6502 *mpu, addr_t addr, uint8_t data);
#endif

extern void copro_lib6502_emulator();

#endif
