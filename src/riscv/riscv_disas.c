// Based on https://github.com/andportnoy/riscv-disassembler

// TODO: Opcodes in Mini-rv32ima that need adding to the disassembler
//
// RV32M Multiply extension (opcode 0x33)
//    MUL, MULH, MULHSU, MULHU, DIV, DIVU, REM, REMU
//
// RV32A Atomic extension (opcode 0x2f)
//    LR, SC, AMISWAP, AMOADD, AMOXOR, AMIAND, AMOOR, AMOMIN, AMOMAX, AMOMINU, AMOMAXU
//
// Certain priviliged instructions (opcode 0x73)
//    WFI, MRET, ...

#include <stdio.h>
#include <inttypes.h>

#include "riscv_disas.h"

enum format {R, I, S, B, U, J};

static const char reg_name[32][5] = {
    "zero", "ra",   "sp",   "gp",   "tp",   "t0",   "t1",   "t2",
    "s0",   "s1",   "a0",   "a1",   "a2",   "a3",   "a4",   "a5",
    "a6",   "a7",   "s2",   "s3",   "s4",   "s5",   "s6",   "s7",
    "s8",   "s9",   "s10",  "s11",  "t3",   "t4",   "t5",   "t6",
};

static const char unimplemented[] = "?";

struct {
   uint8_t opcode; /* 7-bit */
   enum format fmt;
} opcodefmt[] = {
   {0x37, U},
   {0x17, U},
   {0x6f, J},
   {0x67, I},
   {0x63, B},
   {0x03, I},
   {0x23, S},
   {0x13, I},
   {0x33, R},
   {0x0f, I},
   {0x73, I},
};

union encoding {
   uint32_t insn;
   struct { /* generic */
      uint32_t opcode :7;
      uint32_t rd     :5;
      uint32_t funct3 :3;
      uint32_t rs1    :5;
      uint32_t rs2    :5;
      uint32_t funct7 :7;
   };
   struct {
      uint32_t opcode :7;
      uint32_t rd     :5;
      uint32_t funct3 :3;
      uint32_t rs1    :5;
      uint32_t rs2    :5;
      uint32_t funct7 :7;
   } r;
   struct {
      uint32_t opcode :7;
      uint32_t rd     :5;
      uint32_t funct3 :3;
      uint32_t rs1    :5;
      int32_t  i11_0  :12; /* sign extension */
   } i;
   struct {
      uint32_t opcode :7;
      uint32_t i4_0   :5;
      uint32_t funct3 :3;
      uint32_t rs1    :5;
      uint32_t rs2    :5;
      int32_t  i11_5  :7; /* sign extension */
   } s;
   struct {
      uint32_t opcode :7;
      uint32_t i11    :1;
      uint32_t i4_1   :4;
      uint32_t funct3 :3;
      uint32_t rs1    :5;
      uint32_t rs2    :5;
      uint32_t i10_5  :6;
      int32_t  i12    :1; /* sign extension */
   } b;
   struct {
      uint32_t opcode :7;
      uint32_t rd     :5;
      uint32_t i31_12 :20;
   } u;
   struct {
      uint32_t opcode :7;
      uint32_t rd     :5;
      uint32_t i19_12 :8;
      uint32_t i11    :1;
      uint32_t i10_1  :10;
      int32_t  i20    :1; /* sign extension */
   } j;
};

int format(uint8_t opcode) {
   for (int i=0, n=sizeof opcodefmt/sizeof opcodefmt[0]; i<n; ++i)
      if (opcode == opcodefmt[i].opcode)
         return opcodefmt[i].fmt;
   return -1;
}

const char *name(int fmt, union encoding *e) {
   switch (fmt) {
   case R: switch (e->funct3) {
      case 0: return e->funct7? "sub": "add";
      case 1: return "sll";
      case 2: return "slt";
      case 3: return "sltu";
      case 4: return "xor";
      case 5: return e->funct7? "sra": "srl";
      case 6: return "or";
      case 7: return "and";
      } break;
   case I: switch(e->opcode) {
      case 0x67: return "jalr";
      case 0x03: switch (e->funct3) {
         case 0: return "lb";
         case 1: return "lh";
         case 2: return "lw";
         case 4: return "lbu";
         case 5: return "lhu";
         } break;
      case 0x13: switch (e->funct3) {
         case 0: return "addi";
         case 1: return "slli";
         case 2: return "slti";
         case 3: return "sltiu";
         case 4: return "xori";
         case 5: return e->funct7? "srai": "srli";
         case 6: return "ori";
         case 7: return "andi";
         } break;
      case 0x0f: switch (e->funct3) {
         case 0: return "fence";
         case 1: return "fence.i";
         } break;
      case 0x73: switch (e->funct3) {
         case 0: switch (e->i.i11_0) {
            case 0x000: return "ecall";
            case 0x001: return "ebreak";
            case 0x302: return "mret";
            default: return "???";
            }
         case 1: return "csrrw";
         case 2: return "csrrs";
         case 3: return "csrrc";
         case 5: return "csrrwi";
         case 6: return "csrrsi";
         case 7: return "csrrci";
         } break;
      } break;
   case S: switch(e->funct3) {
      case 0: return "sb";
      case 1: return "sh";
      case 2: return "sw";
      } break;
   case B: switch(e->funct3) {
      case 0: return e->rs2 ? "beq"  : "beqz";
      case 1: return e->rs2 ? "bne"  : "bnez";
      case 4: return e->rs2 ? "blt"  : "bltz";
      case 5: return e->rs2 ? "bge"  : "bgez";
      case 6: return e->rs2 ? "bltu" : "bltuz";
      case 7: return e->rs2 ? "bgeu" : "bgeuz";
      } break;
   case U: switch(e->opcode) {
      case 0x37: return "lui";
      case 0x17: return "auipc";
      } break;
   case J: return "jal";
   }

   return "???";
}

const char *op0(int fmt, union encoding *e) {
   switch (fmt) {
   case R:
   case I:
      return reg_name[e->rd];
   case S:
      return reg_name[e->rs2];
   case B:
      return reg_name[e->rs1];
   case U:
   case J:
      return reg_name[e->rd];
   default:
      return unimplemented;
   }
}

const char *op1(int fmt, union encoding *e, uint32_t pc) {
   static char imm[32];
   switch (fmt) {
   case R:
   case I:
   case S:
      return reg_name[e->rs1];
   case B:
      if (e->rs2) {
         return reg_name[e->rs2];
      } else {
         return NULL;
      }
   case U:
      sprintf(imm, "0x%x", e->u.i31_12 << 12);
      return imm;
   case J:
      sprintf(imm, "%08" PRIx32,
              (uint32_t)((e->j.i20 << 20) | (e->j.i19_12 << 12) | (e->j.i11 << 11) | (e->j.i10_1  << 1)) + pc
              );
      return imm;
   }
   return unimplemented;
}

const char *op2(int fmt, union encoding *e, uint32_t pc) {
   static char imm[32];
   switch (fmt) {
   case R:
      return reg_name[e->rs2];
   case I:
      sprintf(imm, "%d", e->i.i11_0);
      return imm;
   case S:
      sprintf(imm, "%d", (e->s.i11_5<<5) | e->s.i4_0);
      return imm;
   case B:
      sprintf(imm, "%08"PRIx32,
              (uint32_t)((e->b.i12 << 12) | (e->b.i11 << 11) | (e->b.i10_5 << 5) | (e->b.i4_1 << 1)) + pc
              );
      return imm;
   case U:
   case J:
      return NULL;
   }
   return unimplemented;
}

void riscv_disasm_inst(char *buf, size_t buflen, uint32_t pc, uint32_t inst) {

   union encoding e = {inst};
   int fmt = format(e.opcode);
   const char *n = name(fmt, &e);
   const char *p0 = op0(fmt, &e);
   const char *p1 = op1(fmt, &e, pc);
   const char *p2 = op2(fmt, &e, pc);

   size_t len = (size_t) snprintf(buf, buflen, "%-8s %s", n, p0);
   buf += len;
   buflen -= len;

   if (e.opcode == 0x03 || e.opcode == 0x23) {
      // special case load/store
      len = (size_t) snprintf(buf, buflen, ", %s(%s)", p2, p1);
      buf += len;
      buflen -= len;
   } else {
      if (p1) {
         len = (size_t) snprintf(buf, buflen, ", %s", p1);
         buf += len;
         buflen -= len;
      }
      if (p2) {
         len = (size_t) snprintf(buf, buflen, ", %s", p2);
         buf += len;
         buflen -= len;
      }
   }
}