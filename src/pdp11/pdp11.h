// pdp11.h

#ifndef PDP11_H
#define PDP11_H

#include <inttypes.h>

void pdp11_reset(uint16_t address);
void pdp11_execute();
void pdp11_interrupt(uint8_t vec, uint8_t pri);
void pdp11_switchmode(int newm);

#define false  0
#define true   1
#define bool int
#define xor    ^

#define ITABN 8

typedef struct {
   uint8_t vec;
   uint8_t pri;
} intr;

typedef struct {
   
   // Architecturally visible state
   int32_t R[8];      // signed integer registers   
   uint16_t PS;       // processor status
   bool curuser;
   bool prevuser;

   // Working state
   uint16_t PC;       // address of current instruction
   uint16_t KSP, USP; // kernel and user stack pointer
   uint16_t LKS;
   uint16_t clkcounter;
   uint16_t halted;   // flag set to indicate halted
   intr itab[ITABN];
   
} pdp11_state;

extern pdp11_state *m_pdp11;

#endif
