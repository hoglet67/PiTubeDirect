// opc5ls.h

#ifndef OPC5LS_H
#define OPC5LS_H

#include <inttypes.h>

void opc5ls_init(uint16_t *memory, uint16_t pc_rst, uint16_t pc_irq);
void opc5ls_execute();
void opc5ls_reset();
void opc5ls_irq();


// Instruction
#define PRED  13
#define LEN   12
#define OPCODE 8
#define SRC    4
#define DST    0


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
   uint16_t saved_pc;

   // Register file
   uint16_t reg[16];

   // Processor status flags
   uint16_t psr;

   // Interrupt state
   uint16_t pc_int;
   uint16_t psr_int;

#ifdef USE_ISRV
   int16_t isrv;
#endif

   // Main memory
   uint16_t *memory;

   // Value of PC on reset
   uint16_t pc_rst;

   // Value of PC on irq
   uint16_t pc_irq;

} opc5ls_state;

extern opc5ls_state *m_opc5ls;

enum {
   op_mov,
   op_and,
   op_or,
   op_xor,
   op_add,
   op_adc,
   op_sto,
   op_ld,
   op_ror,
   op_not,
   op_sub,
   op_sbc,
   op_cmp,
   op_cmpc,
   op_bswp,
   op_psr_rti
};


#endif
