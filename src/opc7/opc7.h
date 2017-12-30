// opc7.h

#ifndef OPC7_H
#define OPC7_H

#include <inttypes.h>

void opc7_init(uint32_t *memory, uint32_t pc_rst, uint32_t pc_irq0, uint32_t pc_irq1);
void opc7_execute();
void opc7_reset();
void opc7_irq(int id);

// Instruction format (for most instructions!)
#define PRED    29
#define OPCODE  24
#define SRC     16
#define DST     20
#define OPERAND  0

// PSR flag bits
#define   Z_FLAG 0
#define   C_FLAG 1
#define   S_FLAG 2
#define  EI_FLAG 3
#define SWI_FLAG 4

#define   Z_MASK (1 <<   Z_FLAG)
#define   C_MASK (1 <<   C_FLAG)
#define   S_MASK (1 <<   S_FLAG)
#define  EI_MASK (1 <<  EI_FLAG)
#define SWI_MASK (15 << SWI_FLAG)

#define PSR_MASK 0xFF

// PC is register 15
#define PC 15

typedef struct {
   // Saved PC at the start of on instruction
   uint32_t saved_pc;

   // Register file
   uint32_t reg[16];

   // Processor status flags
   uint32_t psr;

   // Interrupt state
   uint32_t pc_int;
   uint32_t psr_int;

#ifdef USE_ISRV
   int16_t isrv;
#endif

   // Main memory
   uint32_t *memory;

   // Value of PC on reset
   uint32_t pc_rst;

   // Value of PC on irq
   uint32_t pc_irq[2];

} opc7_state;

extern opc7_state *m_opc7;

enum {
   op_mov    = 0,
   op_movt   = 1,
   op_xor    = 2,
   op_and    = 3,
   op_or     = 4,
   op_not    = 5,
   op_cmp    = 6,
   op_sub    = 7,
   op_add    = 8,
   op_bperm  = 9,
   op_ror    = 10,
   op_lsr    = 11,
   op_jsr    = 12,
   op_asr    = 13,
   op_rol    = 14,

   op_halt   = 16,
   op_rti    = 17,
   op_putpsr = 18,
   op_getpsr = 19,

   op_out    = 24,
   op_in     = 25,
   op_sto    = 26,
   op_ld     = 27,
   op_ljsr   = 28,
   op_lmov   = 29,
   op_lsto   = 30,
   op_lld    = 31
};

#endif
