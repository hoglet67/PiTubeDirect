// opc6.h

#ifndef OPC6_H
#define OPC6_H

#include <inttypes.h>

void opc6_init(uint16_t *memory, uint16_t pc_rst, uint16_t pc_irq0, uint16_t pc_irq1);
void opc6_execute();
void opc6_reset();
void opc6_irq(int id);

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
   uint16_t pc_irq[2];

} opc6_state;

extern opc6_state *m_opc6;

enum {
   op_mov    = 0,
   op_and    = 1,
   op_or     = 2,
   op_xor    = 3,
   op_add    = 4,
   op_adc    = 5,
   op_sto    = 6,
   op_ld     = 7,
   op_ror    = 8,
   op_jsr    = 9,
   op_sub    = 10,
   op_sbc    = 11,
   op_inc    = 12,
   op_lsr    = 13,
   op_dec    = 14,
   op_asr    = 15,
   op_halt   = 16,
   op_bswp   = 17,
   op_putpsr = 18,
   op_getpsr = 19,
   op_rti    = 20,
   op_not    = 21,
   op_out    = 22,
   op_in     = 23,
   op_push   = 24,
   op_pop    = 25,
   op_cmp    = 26,
   op_cmpc   = 27
};

#endif
