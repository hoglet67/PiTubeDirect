#include <stdio.h>
#include "opc7.h"
#include "../tube.h"
#include "../copro-opc7.h"

#ifdef INCLUDE_DEBUGGER
#include "opc7_debug.h"
#include "../cpu_debug.h"
#endif

// Encapsulate the persistent CPU state
opc7_state s;

opc7_state *m_opc7 = &s;

// Point the memory read/write back to the Co Pro to include tube access
#define OPC7_READ_MEM copro_opc7_read_mem
#define OPC7_WRITE_MEM copro_opc7_write_mem
#define OPC7_READ_IO copro_opc7_read_io
#define OPC7_WRITE_IO copro_opc7_write_io

static void int_action(int id) {
   s.pc_int = s.reg[PC];
   s.psr_int = s.psr & ~SWI_MASK; // Always clear the swi flag in the saved copy
   DBG_PRINT("saving %04x %02x\r\n", s.pc_int, s.psr_int);
   s.reg[PC] = s.pc_irq[id];
   s.psr &= ~EI_MASK;
}

void opc7_execute() {

   do {

#ifdef INCLUDE_DEBUGGER
      if (opc7_debug_enabled)
      {
         s.saved_pc = s.reg[PC];
         debug_preexec(&opc7_cpu_debug, s.reg[PC]);
      }
#endif
      DBG_PRINT("%04x ", s.reg[PC]);

      // Fetch the instruction
      register uint32_t instr = *(s.memory + s.reg[PC]++);
      DBG_PRINT("%04x ", instr);

      // Extract the opcode
      register int opcode = (instr >> OPCODE) & 0x1f;

      // Evaluate the predicate
      register int pred = (instr >> PRED) & 7;
      switch (pred) {
      case 0:
         pred = 1; break;
      case 1:
         pred = 0; break;
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

      // Evaluate the operand (one of two formats; needs sign extension)
      register int operand;

      if (opcode >= op_ljsr){
        operand = instr & 0xfffff;
        if (operand & 0x80000){
          operand |=  0xfff00000;
        }
      } else {
        operand = instr & 0xffff;
        if (operand & 0x8000){
          operand |=  0xffff0000;
        }
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

         if (opcode >= op_ljsr){  // long format instruction
           src = 0;
         }

         uint32_t ea_ed = s.reg[src] + operand;

         // Setup carry going into the "ALU"
         uint64_t res = 0; // result needs to be wider than the machine we emulate for easy add, sub, cmp
         unsigned int cin = (s.psr & C_MASK ) ? 1 : 0;
         int cout = cin;

         // When to preserve the flags
         int preserve_flag = 0;

         // Execute the instruction
         switch(opcode) {
         case op_mov:
         case op_lmov:
            s.reg[dst] = ea_ed;
            break;
         case op_movt:
            s.reg[dst] = (ea_ed << 16) | (s.reg[dst] & 0xffff);
            break;
         case op_xor:
            s.reg[dst] ^= ea_ed;
            break;
         case op_and:
            s.reg[dst] &= ea_ed;
            break;
         case op_or:
            s.reg[dst] |= ea_ed;
            break;
         case op_not:
            s.reg[dst] = ~ea_ed;
            break;
         case op_cmp:
            res = ((uint64_t) s.reg[dst]) + ((~ea_ed) & 0xffffffff) + 1;
            dst = 0; // retarget cmp/cmpc to r0
            s.reg[dst] = res & 0xffffffff;
            cout = (res >> 32) & 1;
            break;
         case op_sub:
            res = ((uint64_t) s.reg[dst]) + ((~ea_ed) & 0xffffffff) + 1;
            s.reg[dst] = res & 0xffffffff;
            cout = (res >> 32) & 1;
            break;
         case op_add:
            res = (uint64_t) s.reg[dst] + ea_ed;
            s.reg[dst] = res & 0xffffffff;
            cout = (res >> 32) & 1;
            break;
         case op_bperm: // pick off one of four bytes from source, four times.
            res = 0;
            ea_ed = s.reg[src];
            res |= ((operand & 0xf) == 4) ? 0 : 0xff & (ea_ed >> (8 * (operand & 0xf) ));
            operand >>= 4;
            res |= ((operand & 0xf) == 4) ? 0 : (0xff & (ea_ed >> (8 * (operand & 0xf) ))) << 8;
            operand >>= 4;
            res |= ((operand & 0xf) == 4) ? 0 : (0xff & (ea_ed >> (8 * (operand & 0xf) ))) << 16;
            operand >>= 4;
            res |= ((operand & 0xf) == 4) ? 0 : (0xff & (ea_ed >> (8 * (operand & 0xf) ))) << 24;
            s.reg[dst] = res;
            break;
         case op_ror:
            cout = ea_ed & 1;
            s.reg[dst] = (cin << 31) | (ea_ed >> 1);
            break;
         case op_lsr:
            cout = ea_ed & 1;
            s.reg[dst] = (ea_ed >> 1);
            break;
         case op_jsr:
         case op_ljsr:
            preserve_flag = 1;
            s.reg[dst] = s.reg[PC];
            s.reg[PC] = ea_ed;
            break;
         case op_asr:
            cout = ea_ed & 1;
            s.reg[dst] = (ea_ed & 0x80000000) |  (ea_ed >> 1);
            break;
         case op_rol:
            cout = ea_ed >> 31;
            s.reg[dst] = ea_ed + ea_ed + cin;
            break;

         case op_halt:
            DBG_PRINT("Halt instruction at with halt number 0x%04x\r\n", operand);
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
         case op_out:
            preserve_flag = 1;
            OPC7_WRITE_IO(ea_ed, s.reg[dst]);
            break;
         case op_in:
            s.reg[dst] = OPC7_READ_IO(ea_ed);
            break;

         case op_sto:
         case op_lsto:
            preserve_flag = 1;
            OPC7_WRITE_MEM(ea_ed, s.reg[dst]);
            break;
         case op_ld:
         case op_lld:
            s.reg[dst] = OPC7_READ_MEM(ea_ed);
            break;
         }

         // Update flags
         if ((!preserve_flag) && (dst != PC)) {
            s.psr &= ~(S_MASK | C_MASK | Z_MASK);
            if (s.reg[dst] & 0x80000000) {
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

void opc7_reset() {
   for (int i = 0; i < 16; i++) {
      s.reg[i] = 0;
   }
   s.reg[PC] = s.pc_rst;
   s.psr = 0;
   s.pc_int = 0;
   s.psr_int = 0;
}

void opc7_irq(int id) {
   if (s.psr & EI_MASK) {
      int_action(id);
   }
}

void opc7_init(uint32_t *memory, uint32_t pc_rst, uint32_t pc_irq0, uint32_t pc_irq1) {
   s.memory = memory;
   s.pc_rst = pc_rst;
   s.pc_irq[0] = pc_irq0;
   s.pc_irq[1] = pc_irq1;
}
