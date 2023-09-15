// Based on https://github.com/andportnoy/riscv-disassembler

// TODO: Opcodes in Mini-rv32ima that need adding to the disassembler
//
// RV32A Atomic extension (opcode 0x2f)
//    LR, SC, AMISWAP, AMOADD, AMOXOR, AMIAND, AMOOR, AMOMIN, AMOMAX, AMOMINU, AMOMAXU
//

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
   uint32_t inst;          /* cppcheck-suppress unusedStructMember */
   struct {
      uint32_t opcode :7;
      uint32_t rd     :5;
      uint32_t funct3 :3;
      uint32_t rs1    :5;
      uint32_t rs2    :5;
      uint32_t funct7 :7;
   };
   struct {
      uint32_t        :7;  /* opcode */
      uint32_t        :5;  /* rd */
      uint32_t        :3;  /* funct3 */
      uint32_t        :5;  /* rs1 */
      uint32_t i11_0  :12;
   } i;
   struct {
      uint32_t        :7;  /* opcode */
      uint32_t i4_0   :5;
      uint32_t        :3;  /* funct3 */
      uint32_t        :5;  /* rs1 */
      uint32_t        :5;  /* rs2 */
      uint32_t i11_5  :7;
   } s;
   struct {
      uint32_t        :7;  /* opcode */
      uint32_t i11    :1;
      uint32_t i4_1   :4;
      uint32_t        :3;  /* funct3 */
      uint32_t        :5;  /* rs1 */
      uint32_t        :5;  /* rs2 */
      uint32_t i10_5  :6;
      uint32_t i12    :1;
   } b;
   struct {
      uint32_t        :7;  /* opcode */
      uint32_t        :5;  /* rd */
      uint32_t i31_12 :20;
   } u;
   struct {
      uint32_t        :7;  /* opcode */
      uint32_t        :5;  /* rd */
      uint32_t i19_12 :8;
      uint32_t i11    :1;
      uint32_t i10_1  :10;
      uint32_t i20    :1;
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
   case R:
      if (e->funct7 == 0x00) {
         switch (e->funct3) {
         case 0: return "add";
         case 1: return "sll";
         case 2: return "slt";
         case 3: return "sltu";
         case 4: return "xor";
         case 5: return "srl";
         case 6: return "or";
         case 7: return "and";
         }
      } else if (e->funct7 == 0x20) {
         switch (e->funct3) {
         case 0: return "sub";
         case 5: return "sra";
         }
      } else if (e->funct7 == 0x01) {
         switch (e->funct3) {
         case 0: return "mul";
         case 1: return "mulh";
         case 2: return "mulsu";
         case 3: return "mulu";
         case 4: return "div";
         case 5: return "divu";
         case 6: return "rem";
         case 7: return "remu";
         }
      } break;
   case I:
      switch(e->opcode) {
      case 0x67:
         if (e->funct3 == 0) {
            return "jalr";
         }
         break;
      case 0x03:
         switch (e->funct3) {
         case 0: return "lb";
         case 1: return "lh";
         case 2: return "lw";
         case 4: return "lbu";
         case 5: return "lhu";
         }
         break;
      case 0x13:
         switch (e->funct3) {
         case 0: return "addi";
         case 1:
            if (e->funct7 == 0x00) {
               return "slli";
            }
            break;
         case 2: return "slti";
         case 3: return "sltiu";
         case 4: return "xori";
         case 5:
            if (e->funct7 == 0x00) {
               return "srli";
            } else if (e->funct7 == 0x20) {
               return "srai";
            }
            break;
         case 6: return "ori";
         case 7: return "andi";
         }
         break;
      case 0x0f:
         switch (e->funct3) {
         case 0: return "fence";
         case 1: return "fence.i";
         }
         break;
      case 0x73:
         switch (e->funct3) {
         case 0:
            switch (e->i.i11_0) {
            case 0x000: return "ecall";
            case 0x001: return "ebreak";
            case 0x102: return "sret";
            case 0x105: return "wfi";
            case 0x302: return "mret";
            }
            break;
         case 1: return e->rd ?           "csrrw"            : "csrw";
         case 2: return e->rd ? (e->rs1 ? "csrrs"  : "csrr") : "csrs";
         case 3: return e->rd ? (e->rs1 ? "csrrc"  : "csrr") : "csrc";
         case 5: return e->rd ?           "csrrwi"           : "csrwi";
         case 6: return e->rd ? (e->rs1 ? "csrrsi" : "csrr") : "csrsi";
         case 7: return e->rd ? (e->rs1 ? "csrrci" : "csrr") : "csrci";
         }
         break;
      }
      break;
   case S:
      switch(e->funct3) {
      case 0: return "sb";
      case 1: return "sh";
      case 2: return "sw";
      }
      break;
   case B:
      switch(e->funct3) {
      case 0: return e->rs2 ? "beq"  : "beqz";
      case 1: return e->rs2 ? "bne"  : "bnez";
      case 4: return e->rs2 ? "blt"  : "bltz";
      case 5: return e->rs2 ? "bge"  : "bgez";
      case 6: return e->rs2 ? "bltu" : "bltuz";
      case 7: return e->rs2 ? "bgeu" : "bgeuz";
      }
      break;
   case U:
      switch(e->opcode) {
      case 0x37: return "lui";
      case 0x17: return "auipc";
      }
      break;
   case J:
      return "jal";
   }

   return "???";
}

const char *op0(int fmt, const union encoding *e) {
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

const char *op1(int fmt, const union encoding *e, uint32_t pc) {
   static char imm[32];
   int i;
   switch (fmt) {
   case R:
   case S:
      return reg_name[e->rs1];
   case I:
      if (e->opcode == 0x73 && e->funct3 >= 5) {
         // special case CSR immediate (rs1 treated as 5-bit unsigned immediate)
         sprintf(imm, "%u", e->rs1);
         return imm;
      } else {
         return reg_name[e->rs1];
      }
   case B:
      if (e->rs2) {
         return reg_name[e->rs2];
      } else {
         return NULL;
      }
   case U:
      sprintf(imm, "0x%" PRIx32, (uint32_t) (e->u.i31_12 << 12));
      return imm;
   case J:
      i = (e->j.i20 << 20) | (e->j.i19_12 << 12) | (e->j.i11 << 11) | (e->j.i10_1  << 1);
      if (i & 0x100000) {
         i -= 0x200000;
      }
      sprintf(imm, "0x%" PRIx32, pc + (uint32_t)i);
      return imm;
   }
   return unimplemented;
}

const char *op2(int fmt, const union encoding *e, uint32_t pc) {
   static char imm[32];
   int i;
   switch (fmt) {
   case R:
      return reg_name[e->rs2];
   case I:
      i = e->i.i11_0;
      if (i & 0x800) {
         i -= 0x1000;
      }
      if (e->opcode == 0x73 && (e->funct3 & 3)) {
         // CSR
         switch (i) {
         case 0x300: return "mstatus";
         case 0x301: return "misa";
         case 0x302: return "medeleg";
         case 0x303: return "mideleg";
         case 0x304: return "mie";
         case 0x305: return "mtvec";
         case 0x306: return "mcounteren";
         case 0x340: return "mscratch";
         case 0x341: return "mepc";
         case 0x342: return "mcause";
         case 0x343: return "mtval";
         case 0x344: return "mip";
         case 0xc00: return "cycle";
         case 0xc01: return "time";
         case 0xc02: return "instret";
         case 0xc80: return "cycleh";
         case 0xc81: return "timeh";
         case 0xc82: return "instreth";
         case 0xf11: return "mvendorid";
         case 0xf12: return "marchid";
         case 0xf13: return "mimpid";
         case 0xf14: return "mhartid";
         }
         sprintf(imm, "0x%03x", i);
      } else {
         sprintf(imm, "%d", i);
      }
      return imm;
   case S:
      i = (e->s.i11_5 << 5) | e->s.i4_0;
      if (i & 0x800) {
         i -= 0x1000;
      }
      sprintf(imm, "%d", i);
      return imm;
   case B:
      i = (e->b.i12 << 12) | (e->b.i11 << 11) | (e->b.i10_5 << 5) | (e->b.i4_1 << 1);
      if (i & 0x1000) {
         i -= 0x2000;
      }
      sprintf(imm, "0x%"PRIx32, pc + (uint32_t)i);
      return imm;
   case U:
   case J:
      return NULL;
   }
   return unimplemented;
}

void riscv_disasm_inst(char *buf, size_t buflen, uint32_t pc, uint32_t inst) {

   union encoding e = { .inst=(inst) };
   int fmt = format(e.opcode);
   const char *n = name(fmt, &e);
   const char *p[3];

   p[0] = op0(fmt, &e);
   p[1] = op1(fmt, &e, pc);
   p[2] = op2(fmt, &e, pc);

   size_t len = (size_t) snprintf(buf, buflen, "%-8s", n);
   buf += len;
   buflen -= len;

   if (e.opcode == 0x03 || e.opcode == 0x23) {
      // special case load/store
      snprintf(buf, buflen, " %s, %s(%s)", p[0], p[2], p[1]);
      return;
   } else if (e.opcode == 0x73) {
      if (e.funct3 == 0) {
         // special case ECALL, EBREAK, SRET, MRET, WFI which don't have any parameters
         return;
      } else if (e.funct3 & 3) {
         // special case CSR
         //
         // func3:
         // 1: csrrw   rd, csr, rs1
         //    csrw        csr, rs1
         // 2: csrrs   rd, csr, rs1
         //    csrs        csr, rs1
         //    csrr    rd, csr
         // 3: csrrc   rd, csr, rs1
         //    csrc        csr, rs1
         //    csrr    rd, csr
         // 5: csrrwi  rd, csr, uimm5
         //    csrwi       csr, uimm5
         // 6: csrrsi  rd, csr, uimm5
         //    csrr    rd, csr
         //    csrsi       csr, uimm5
         // 7: csrrci  rd, csr, uimm5
         //    csrr    rd, csr
         //    csrci       csr, uimm5
         //
         // On entry:
         //   p0: rd
         //   p1: rs1/uimm
         //   p2: csr
         // On exit:
         //   p0: rd
         //   p1: csr
         //   p2: rs1/uimm
         if (e.rd == 0) {
            // csrw,  csrs,  csrc
            // csrwi, csrsi, csrci
            p[0] = NULL;
         } else if ((e.rs1 == 0) && (e.funct3 & 2)) {
            // csrr (2/3/6/7)
            p[1] = NULL;
         }
         // Swap p1/p2 as it's the convention for the csr to be displayed last
         const char *tmp = p[1];
         p[1] = p[2];
         p[2] = tmp;
         // fall through
      }
   }

   // everything else formatter as p0, p1, p2
   // (but we allow for any of these to be omitted)
   int comma = 0;
   for (int i = 0; i < 3; i++) {
      if (p[i]) {
         if (comma) {
            len = (size_t) snprintf(buf, buflen, ", %s", p[i]);
         } else {
            len = (size_t) snprintf(buf, buflen, " %s", p[i]);
         }
         buf += len;
         buflen -= len;
         comma = 1;
      }
   }
}
