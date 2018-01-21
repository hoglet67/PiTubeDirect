#include <stdio.h>
#include "opc6.h"
#include "../tube.h"
#include "../copro-opc6.h"

#ifdef INCLUDE_DEBUGGER
#include "opc6_debug.h"
#include "../cpu_debug.h"
#endif

// Encapsulate the persistent CPU state
opc6_state s;

opc6_state *m_opc6 = &s;

// Point the memory read/write back to the Co Pro to include tube access
#define OPC6_READ_MEM copro_opc6_read_mem
#define OPC6_WRITE_MEM copro_opc6_write_mem
#define OPC6_READ_IO copro_opc6_read_io
#define OPC6_WRITE_IO copro_opc6_write_io

static void int_action(int id) {
   s.pc_int = s.reg[PC];
   s.psr_int = s.psr & ~SWI_MASK; // Always clear the swi flag in the saved copy
   DBG_PRINT("saving %04x %02x\r\n", s.pc_int, s.psr_int);
   s.reg[PC] = s.pc_irq[id];
   s.psr &= ~EI_MASK;
}

void opc6_execute() {

   do {

#ifdef INCLUDE_DEBUGGER
      if (opc6_debug_enabled)
      {
         s.saved_pc = s.reg[PC];
         debug_preexec(&opc6_cpu_debug, s.reg[PC]);
      }
#endif

      DBG_PRINT("%04x ", s.reg[PC]);

      // Fetch the instruction
      register uint16_t instr = *(s.memory + s.reg[PC]++);
      DBG_PRINT("%04x ", instr);

      // Extract the opcode
      register int opcode = (instr >> OPCODE) & 15;

      // Evaluate the predicate
      register int pred = (instr >> PRED) & 7;
      switch (pred) {
      case 0:
         pred = 1; break;
      case 1:
         pred = 1; opcode += 16; break;
      case 2:
         pred = s.psr & Z_MASK; break;
      case 3:
         pred = !(s.psr & Z_MASK); break;
      case 4:
         pred = s.psr & C_MASK; break;
      case 5:
         pred = !(s.psr & C_MASK); break;
      case 6:
         pred = s.psr & S_MASK; break;
      case 7:
         pred = !(s.psr & S_MASK); break;
      }

      // Evaluate the operand
      register int operand;

      if ((instr >> LEN) & 1) {
         operand = *(s.memory + s.reg[PC]++);
      } else if (opcode == op_push) {
         operand = 0xffff;
      } else if (opcode == op_pop) {
         operand = 0x0001;
      } else {
         operand = 0;
      }

      DBG_PRINT("%04x %02x", operand, s.psr);
      DBG_PRINT("\r\n");

      // Conditionally execute the instruction
      if (pred) {

         // Force register 0 to be zero (it's overwritten by cmp)
         s.reg[0] = 0;

         // Decode the instruction
         int dst = (instr >> DST) & 15;
         int src = (instr >> SRC) & 15;
         int ea_ed = (s.reg[src] + operand) & 0xffff;

         // Setup carry going into the "ALU"
         uint32_t res = 0;
         int cin = (s.psr & C_MASK) ? 1 : 0;
         int cout = cin;

         // When to preserve the flags
         int preserve_flag = 0;

         // Execute the instruction
         switch(opcode) {
         case op_mov:
            s.reg[dst] = ea_ed;
            break;
         case op_and:
            s.reg[dst] &= ea_ed;
            break;
         case op_or:
            s.reg[dst] |= ea_ed;
            break;
         case op_xor:
            s.reg[dst] ^= ea_ed;
            break;
         case op_add:
            res = s.reg[dst] + ea_ed;
            s.reg[dst] = res & 0xffff;
            cout = (res >> 16) & 1;
            break;
         case op_adc:
            res = s.reg[dst] + ea_ed + cin;
            s.reg[dst] = res & 0xffff;
            cout = (res >> 16) & 1;
            break;
         case op_sto:
            preserve_flag = 1;
            OPC6_WRITE_MEM(ea_ed, s.reg[dst]);
            break;
         case op_ld:
            s.reg[dst] = OPC6_READ_MEM(ea_ed);
            break;
         case op_ror:
            cout = ea_ed & 1;
            s.reg[dst] = (cin << 15) | (ea_ed >> 1);
            break;
         case op_jsr:
            preserve_flag = 1;
            s.reg[dst] = s.reg[PC];
            s.reg[PC] = ea_ed;
            break;
         case op_sub:
            res = s.reg[dst] + ((~ea_ed) & 0xffff) + 1;
            s.reg[dst] = res & 0xffff;
            cout = (res >> 16) & 1;
            break;
         case op_sbc:
            res = s.reg[dst] + ((~ea_ed) & 0xffff) + cin;
            s.reg[dst] = res & 0xffff;
            cout = (res >> 16) & 1;
            break;
         case op_inc:
            res = s.reg[dst] + src;
            s.reg[dst] = res & 0xffff;
            cout = (res >> 16) & 1;
            break;
         case op_lsr:
            cout = ea_ed & 1;
            s.reg[dst] = (ea_ed >> 1);
            break;
         case op_dec:
            res = s.reg[dst] + ((~src) & 0xffff) + 1;
            s.reg[dst] = res & 0xffff;
            cout = (res >> 16) & 1;
            break;
         case op_asr:
            cout = ea_ed & 1;
            s.reg[dst] = (ea_ed & 0x8000) |  (ea_ed >> 1);
            break;
         case op_halt:
            DBG_PRINT("Halt instruction at with halt number 0x%04x\r\n", operand);
            break;
         case op_bswp:
            s.reg[dst] = (((ea_ed & 0xFF00) >> 8) | ((ea_ed & 0x00FF) << 8));
            break;
         case op_putpsr:
            s.psr = ea_ed & PSR_MASK;
            preserve_flag = 1;
            if (s.psr & SWI_MASK) {
               int_action(0);
            }
            break;
         case op_getpsr:
            s.reg[dst] = s.psr & PSR_MASK;
            break;
         case op_rti:
            if (dst == PC) {
               DBG_PRINT("restoring %04x %02x\r\n", s.pc_int, s.psr_int);
               s.reg[PC] = s.pc_int;
               s.psr = s.psr_int & ~SWI_MASK;
               preserve_flag = 1;
            } else {
               DBG_PRINT("Illegal RTI (dst must be PC)\r\n");
            }
            break;
         case op_not:
            s.reg[dst] = ~ea_ed;
            break;
         case op_out:
            preserve_flag = 1;
            OPC6_WRITE_IO(ea_ed, s.reg[dst]);
            break;
         case op_in:
            s.reg[dst] = OPC6_READ_IO(ea_ed);
            break;
         case op_push:
            preserve_flag = 1;
            OPC6_WRITE_MEM(ea_ed, s.reg[dst]);
            s.reg[src] = ea_ed;
            break;
         case op_pop:
            s.reg[dst] = OPC6_READ_MEM(s.reg[src]);
            s.reg[src] = ea_ed;
            break;
         case op_cmp:
            res = s.reg[dst] + ((~ea_ed) & 0xffff) + 1;
            dst = 0; // retarget cmp/cmpc to r0
            s.reg[dst] = res & 0xffff;
            cout = (res >> 16) & 1;
            break;
         case op_cmpc:
            res = s.reg[dst] + ((~ea_ed) & 0xffff) + cin;
            dst = 0; // retarget cmp/cmpc to r0
            s.reg[dst] = res & 0xffff;
            cout = (res >> 16) & 1;
            break;
         }

         // Update flags
         if ((!preserve_flag) && (dst != PC)) {
            s.psr &= ~(S_MASK | C_MASK | Z_MASK);
            if (s.reg[dst] & 0x8000) {
               s.psr |= S_MASK;
            }
            if (s.reg[dst] == 0) {
               s.psr |= Z_MASK;
            }
            if (cout) {
               s.psr |= C_MASK;
            }
         }
      }
   } while (tubeContinueRunning());
}

void opc6_reset() {
   for (int i = 0; i < 16; i++) {
      s.reg[i] = 0;
   }
   s.reg[PC] = s.pc_rst;
   s.psr = 0;
   s.pc_int = 0;
   s.psr_int = 0;
}

void opc6_irq(int id) {
   if (s.psr & EI_MASK) {
      int_action(id);
   }
}

void opc6_init(uint16_t *memory, uint16_t pc_rst, uint16_t pc_irq0, uint16_t pc_irq1) {
   s.memory = memory;
   s.pc_rst = pc_rst;
   s.pc_irq[0] = pc_irq0;
   s.pc_irq[1] = pc_irq1;
}
