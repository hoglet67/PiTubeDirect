#ifndef F100_H
#define F100_H

// Use this to enable full 64KWord addressing rather than F100's limited 64KWords
//#define F200
#include <inttypes.h>
#include <stdbool.h>
#include "../tube.h"
#include "../copro-f100.h"

#ifdef INCLUDE_DEBUGGER
#include "f100_debug.h"
#include "../cpu_debug.h"
#endif

// Point the memory read/write back to the Co Pro to include tube access
#define F100_READ_MEM copro_f100_read_mem
#define F100_WRITE_MEM copro_f100_write_mem

#define F100MEMSZ 65536

void f100_init(uint16_t *memory, uint16_t pc_rst, uint16_t pc_irq0, uint16_t pc_irq1);
void f100_reset();
void f100_execute();
void f100_irq(int id);

// Define this to change the PC start address (mimic adsel pin on actual CPU)
#define ADSEL_2K      1
#ifdef ADSEL_2K
#define F100_PC_RST 2048
#else
#define F100_PC_RST 16384
#endif

#define ISHEXCH(c) (c>= '0' && c<='9') || ( toupper(c) <= 'F' && toupper(c)>='A')
#define ATOH(c)   ((c>='0' && c<='9') ? toupper(c)-'0': toupper(c)-'A'+10)
#define HEXPAIRTOD(c,c1) (16*ATOH(c)+ATOH(c1))
#define INTTOPRINT(c) (( (c)>31 && (c)<128) ? (c): '.')
#define COMPUTE_ZERO(r)   cpu.Z = (((r&0x0FFFF) ==0)?1:0)
#define COMPUTE_CARRY(r)  cpu.C = (((r&0x10000) !=0)?1:0)
#define COMPUTE_BORROW(r) cpu.C = (((r&0x10000) !=0)?0:1)
#define COMPUTE_SIGN(r)   cpu.S = (((r&0x08000) !=0)?1:0)
#define COMPUTE_OVERFLOW_ADD(r,a,b) cpu.V = (((a & 0x8000)==(b & 0x8000)) && (r & 0x8000) != (a & 0x8000))
#define COMPUTE_OVERFLOW_SUB(r,a,b) cpu.V = (((a & 0x8000)!=(b & 0x8000)) && (r & 0x8000) != (a & 0x8000))
#define COMPUTE_SZ(r)        COMPUTE_SIGN(r) ; COMPUTE_ZERO(r)
#define COMPUTE_SV(r, a, b)  COMPUTE_SIGN(r) ; COMPUTE_OVERFLOW_ADD(r,a,b)
#define COMPUTE_SV_ADD(r, a, b)  COMPUTE_SIGN(r) ; COMPUTE_OVERFLOW_ADD(r,a,b)
#define COMPUTE_SV_SUB(r, a, b)  COMPUTE_SIGN(r) ; COMPUTE_OVERFLOW_SUB(r,a,b)
#define COMPUTE_SVZ_ADD(r, a, b) COMPUTE_SV_ADD(r,a,b) ; COMPUTE_ZERO(r)
#define COMPUTE_SVZ_SUB(r, a, b) COMPUTE_SV_SUB(r,a,b) ; COMPUTE_ZERO(r)

#define SET_CARRY         cpu.C = 1
#define CLEAR_CARRY       cpu.C = 0
#define CLEAR_OVERFLOW    cpu.V = 0
#define CLEAR_MULTI       cpu.M = 0
#define UNPACK_FLAGS(f)   cpu.I=(f&1);cpu.Z=(f>>1)&1;cpu.V=(f>>2)&1;cpu.S=(f>>3)&1;cpu.C=(f>>4)&1;cpu.M=(f>>5)&1;cpu.F=(f>>6)&1
#define PACK_FLAGS        (((cpu.F<<6)|(cpu.M<<5)|(cpu.C<<4)|(cpu.S<<3)|(cpu.V<<2)|(cpu.Z<<1)|(cpu.I?1:0)) & 0x7F)
#define TRUNC16(m)        ((uint16_t)(m) & 0xFFFF)

#ifdef F200
// For F200 operation don't truncate the 15th bit of addresses
#define TRUNC15(m)        ((m) & 0xFFFF)
#else
#define TRUNC15(m)        ((m) & 0x7FFF)
#endif
#define INC_ADDR(m,n)     ((m) = TRUNC15(m+n))
// Define an INC_PTR operation - we think pointers use full 16 bit arithmetic but
// may need to revisit this once we see an actual IC in operation
#define INC_PTR(m,n)     ((m) = TRUNC16(m+n))
#define FETCH15(m, o, pc) o=TRUNC15(F100_READ_MEM(pc)); INC_ADDR(pc,1)
#define HALT(ir)          (ir.F==0 && ir.T==1)
#define LSP               0

#define OP_F0      0x0
#define OP_SJM     0x1
#define OP_CAL     0x2
#define OP_RTN_RTC 0x3
#define OP_STO     0x4
#define OP_ADS     0x5
#define OP_SBS     0x6
#define OP_ICZ     0x7
#define OP_LDA     0x8
#define OP_ADD     0x9
#define OP_SUB     0xA
#define OP_CMP     0xB
#define OP_AND     0xC
#define OP_NEQ     0xD
#define OP_F14     0xE
#define OP_JMP     0xF


// Not yet used in PiTubeDirect version - will be needed for debugger
// static char *mnemonic[] = {
//   "HALT/BIT/SHIFT/CONDJMP",
//   "SJM",
//   "CAL",
//   "RTN/RTC",
//   "STO",
//   "ADS",
//   "SBS",
//   "ICZ",
//   "LDA",
//   "ADD",
//   "SUB",
//   "CMP",
//   "AND",
//   "NEQ",
//   "F14",
//   "JMP",
// };


typedef struct {
  uint16_t WORD;
  uint8_t  B, F, I, J, P, R, S, T;
  uint16_t N;
} instr_t;


typedef struct {
  uint16_t *mem ;
  uint16_t acc;
  uint16_t or;
  uint16_t pc;
  bool I;
  bool Z;
  bool V;
  bool S;
  bool C;
  bool M;
  bool F;
  instr_t ir;
  // For the debugger
  uint16_t saved_pc;
} cpu_t;

extern cpu_t *m_f100;

#endif
