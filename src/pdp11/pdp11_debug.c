#include <stdio.h>
#include <stdint.h>
#include <inttypes.h>
#include <string.h>

#include "../cpu_debug.h"
#include "../copro-pdp11.h"

#include "pdp11.h"
#include "pdp11_debug.h"

static uint16_t disasm(char *buf, uint16_t a);

/*****************************************************
 * CPU Debug Interface
 *****************************************************/

int pdp11_debug_enabled = 0;

enum register_numbers {
   i_R0,
   i_R1,
   i_R2,
   i_R3,
   i_R4,
   i_R5,
   i_SP,
   i_PC,
   i_PS,
   i_PREVUSER,
   i_CURUSER,
   i_INTQUEUE
};

// NULL pointer terminated list of register names.
static const char *dbg_reg_names[] = {
   "R0",
   "R1",
   "R2",
   "R3",
   "R4",
   "R5",
   "SP",
   "PC",
   "PS",
   "PREVUSER",
   "CURRUSER",
   "INTQUEUE",
   NULL
};



// NULL pointer terminated list of trap names.
static const char *dbg_trap_names[] = {
   NULL
};

// enable/disable debugging on this CPU, returns previous value.
static int dbg_debug_enable(int newvalue) {
   int oldvalue = pdp11_debug_enabled;
   pdp11_debug_enabled = newvalue;
   return oldvalue;
};

// CPU's usual memory read function for data.
static uint32_t dbg_memread(uint32_t addr) {
   return copro_pdp11_read8(addr);
};

// CPU's usual memory write function.
static void dbg_memwrite(uint32_t addr, uint32_t value) {
   copro_pdp11_write8(addr, value);
};

static uint32_t dbg_disassemble(uint32_t addr, char *buf, size_t bufsize) {
   return disasm(buf, addr);
};

// Get a register - which is the index into the names above
static uint32_t dbg_reg_get(int which) {
   if (which == i_PS) {
      return m_pdp11->PS;
   } else if (which == i_CURUSER) {
      return m_pdp11->curuser;
   } else if (which == i_PREVUSER) {
      return m_pdp11->prevuser;
   } else {
      return (uint16_t) m_pdp11->R[which];
   }
};

// Set a register.
static void  dbg_reg_set(int which, uint32_t value) {
   if (which == i_PS) {
      m_pdp11->PS = value & 0xFF;
   } else if (which == i_CURUSER) {
      m_pdp11->curuser = value & 1;
   } else if (which == i_PREVUSER) {
      m_pdp11->prevuser = value & 1;
   } else {
      m_pdp11->R[which] = value & 0xFFFF;
   }
};

static const char* flagname = "T N Z V C ";

// Print register value in CPU standard form.
static size_t dbg_reg_print(int which, char *buf, size_t bufsize) {
   if (which == i_PS) {
      int i;
      int bit;
      char c;
      const char *flagnameptr = flagname;
      uint32_t psr = dbg_reg_get(i_PS);

      if (bufsize < 200) {
         strncpy(buf, "buffer too small!!!", bufsize);
      }

      // Print the octal/hex value
      buf += sprintf(buf, "%06"PRIo32" (%04"PRIx32") ", psr, psr);

      // Print the I-bit I field
      *buf++ = 'I';
      *buf++ = ':';
      sprintf(buf, "%"PRIx32, (psr >> 5) & 0x07);
      buf++;
      *buf++ = ' ';

      // Print the remaining flag bits
      bit = 0x10;
      for (i = 0; i < 5; i++) {
         if (psr & bit) {
            c = '1';
         } else {
            c = '0';
         }
         do {
            *buf++ = *flagnameptr++;
         } while (*flagnameptr != ' ');
         flagnameptr++;
         *buf++ = ':';
         *buf++ = c;
         *buf++ = ' ';
         bit >>= 1;
      }
      *buf++ = '\0';
      return strlen(buf);
   } else if (which == i_CURUSER || which == i_PREVUSER) {
      return snprintf(buf, bufsize, "%c", dbg_reg_get(which) ? 'U' : 'K');
   } else if (which == i_INTQUEUE) {
      for (int i = 0; i < ITABN; i++) {
         sprintf(buf + i * 5, "%02x:%d ", m_pdp11->itab[i].vec,  m_pdp11->itab[i].pri);
      }
      return strlen(buf);
   } else {
      return snprintf(buf, bufsize, "%06"PRIo32" (%04"PRIx32")", dbg_reg_get(which), dbg_reg_get(which));
   }
};

// Parse a value into a register.
static void dbg_reg_parse(int which, const char *strval) {
   uint32_t val = 0;
   sscanf(strval, "%"SCNx32, &val);
   dbg_reg_set(which, val);
};

static uint32_t dbg_get_instr_addr() {
   return m_pdp11->PC;
}

cpu_debug_t pdp11_cpu_debug = {
   .cpu_name       = "PDP11",
   .debug_enable   = dbg_debug_enable,
   .memread        = dbg_memread,
   .memwrite       = dbg_memwrite,
   .disassemble    = dbg_disassemble,
   .reg_names      = dbg_reg_names,
   .reg_get        = dbg_reg_get,
   .reg_set        = dbg_reg_set,
   .reg_print      = dbg_reg_print,
   .reg_parse      = dbg_reg_parse,
   .get_instr_addr = dbg_get_instr_addr,
   .trap_names     = dbg_trap_names,
   .default_base   = 8
};

/*****************************************************
 * Disassembler
 *****************************************************/

typedef struct {
   uint16_t inst;
   uint16_t arg;
   char *msg;
   uint8_t flag;
   bool b;
} D;

enum { DD = 1 << 1, S = 1 << 2, RR = 1 << 3, O = 1 << 4, NN = 1 << 5 , N = 1 << 6};

static D disamtable[] = {
   // One-operand instructions
   { 0077700, 0005000, "CLR",       DD,  true },
   { 0077700, 0005100, "COM",       DD,  true },
   { 0077700, 0005200, "INC",       DD,  true },
   { 0077700, 0005300, "DEC",       DD,  true },
   { 0077700, 0005400, "NEG",       DD,  true },
   { 0077700, 0005500, "ADC",       DD,  true },
   { 0077700, 0005600, "SBC",       DD,  true },
   { 0077700, 0005700, "TST",       DD,  true },
   { 0077700, 0006000, "ROR",       DD,  true },
   { 0077700, 0006100, "ROL",       DD,  true },
   { 0077700, 0006200, "ASR",       DD,  true },
   { 0077700, 0006300, "ASL",       DD,  true },
   { 0177700, 0006700, "SXT",       DD, false },
   { 0177700, 0000300, "SWAB",      DD, false },
   // One-and-a-half-operand instructions
   { 0177000, 0070000, "MUL",  RR | DD, false },
   { 0177000, 0071000, "DIV",  RR | DD, false },
   { 0177000, 0072000, "ASH",  RR | DD, false },
   { 0177000, 0073000, "ASHC", RR | DD, false },
   { 0177000, 0074000, "XOR",  RR | DD, false },
   // Two-operand instructions
   { 0070000, 0010000, "MOV",   S | DD,  true },
   { 0070000, 0020000, "CMP",   S | DD,  true },
   { 0070000, 0030000, "BIT",   S | DD,  true },
   { 0070000, 0040000, "BIC",   S | DD,  true },
   { 0070000, 0050000, "BIS",   S | DD,  true },
   { 0170000, 0060000, "ADD",   S | DD, false },
   { 0170000, 0160000, "SUB",   S | DD, false },
   // Branch instructions
   { 0177400, 0000400, "BR ",        O, false },
   { 0177400, 0001000, "BNE",        O, false },
   { 0177400, 0001400, "BEQ",        O, false },
   { 0177400, 0002000, "BGE",        O, false },
   { 0177400, 0002400, "BLT",        O, false },
   { 0177400, 0003000, "BGT",        O, false },
   { 0177400, 0003400, "BLE",        O, false },
   { 0177400, 0100000, "BPL",        O, false },
   { 0177400, 0100400, "BMI",        O, false },
   { 0177400, 0101000, "BHI",        O, false },
   { 0177400, 0101400, "BLOS",       O, false },
   { 0177400, 0102000, "BVC",        O, false },
   { 0177400, 0102400, "BVS",        O, false },
   { 0177400, 0103000, "BCC",        O, false },
   { 0177400, 0103400, "BCS",        O, false },
   // Other instructions
   { 0177700, 0000100, "JMP",       DD, false },
   { 0177000, 0077000, "SOB",  RR |  O, false },
   { 0177000, 0004000, "JSR",  RR | DD, false },
   { 0177770, 0000200, "RTS",  RR     , false },
   { 0177777, 0000002, "RTI",        0, false },
   { 0177400, 0104400, "TRAP",      NN, false },
   { 0177400, 0104000, "EMT",       NN, false },
   { 0177777, 0000003, "BPT",        0, false },
   { 0177777, 0000004, "IOT",        0, false },
   { 0177777, 0000006, "RTT",        0, false },
   // Zero-operand instructions
   { 0177777, 0000000, "HALT",       0, false },
   { 0177777, 0000001, "WAIT",       0, false },
   { 0177777, 0000005, "RESET",      0, false },
   // Processor Status Word
   { 0177770, 0000230, "SPL",        N, false },
   { 0177777, 0000240, "NOP",        0, false },
   { 0177777, 0000241, "CLC",        0, false },
   { 0177777, 0000242, "CLV",        0, false },
   { 0177777, 0000243, "CLVC",       0, false },
   { 0177777, 0000244, "CLZ",        0, false },
   { 0177777, 0000245, "CLZC",       0, false },
   { 0177777, 0000246, "CLZV",       0, false },
   { 0177777, 0000247, "CLZVC",      0, false },
   { 0177777, 0000250, "CLN",        0, false },
   { 0177777, 0000251, "CLNC",       0, false },
   { 0177777, 0000252, "CLNV",       0, false },
   { 0177777, 0000253, "CLNVC",      0, false },
   { 0177777, 0000254, "CLNZ",       0, false },
   { 0177777, 0000255, "CLNZC",      0, false },
   { 0177777, 0000256, "CLNZV",      0, false },
   { 0177777, 0000257, "CCC",        0, false },
   { 0177777, 0000260, "NOP",        0, false },
   { 0177777, 0000261, "SEC",        0, false },
   { 0177777, 0000262, "SEV",        0, false },
   { 0177777, 0000263, "SEVC",       0, false },
   { 0177777, 0000264, "SEZ",        0, false },
   { 0177777, 0000265, "SEZC",       0, false },
   { 0177777, 0000266, "SEZV",       0, false },
   { 0177777, 0000267, "SEZVC",      0, false },
   { 0177777, 0000270, "SEN",        0, false },
   { 0177777, 0000271, "SENC",       0, false },
   { 0177777, 0000272, "SENV",       0, false },
   { 0177777, 0000273, "SENVC",      0, false },
   { 0177777, 0000274, "SENZ",       0, false },
   { 0177777, 0000275, "SENZC",      0, false },
   { 0177777, 0000276, "SENZV",      0, false },
   { 0177777, 0000277, "SCC",        0, false },
   // Other
   { 0177700, 0006400, "MARK",      NN, false },
   { 0177700, 0006500, "MFPI",      DD, false },
   { 0177700, 0006600, "MTPI",      DD, false },
   { 0177700, 0106400, "MTPS",      DD, false },
   { 0177700, 0106500, "MFPD",      DD, false },
   { 0177700, 0106600, "MTPD",      DD, false },
   { 0177700, 0106700, "MFPS",      DD, false },
   // Terminator
   { 0, 0, "", 0, false }, };

static uint16_t read16(uint16_t a) {
   return copro_pdp11_read16(a);
}

// Returns the number of additional operand bytes for the addressing mode
static int disaslen(uint16_t m) {
   if (((m & 027) == 027) || ((m & 060) == 060)) {
      // ((m & 027) == 027) catches 027 037 067 077
      // ((m & 060) == 060) catches 06x 07x
      return 2;
   } else {
      return 0;
   }
}

static int disasmaddr(char *buf, uint16_t m, uint16_t a) {
   if ((m & 7) == 7) {
      switch (m) {
      case 027:
         return sprintf(buf, "#%06o", read16(a));
      case 037:
         return sprintf(buf, "@#%06o", read16(a));
      case 067:
         return sprintf(buf, "%06o", (a + 2 + (read16(a))) & 0xFFFF);
      case 077:
         return sprintf(buf, "@%06o", (a + 2 + (read16(a))) & 0xFFFF);
      }
   }
   switch (m & 070) {
   case 000:
      return sprintf(buf, "%s", dbg_reg_names[m & 7]);
   case 010:
      return sprintf(buf, "(%s)", dbg_reg_names[m & 7]);
   case 020:
      return sprintf(buf, "(%s)+", dbg_reg_names[m & 7]);
   case 030:
      return sprintf(buf, "@(%s)+", dbg_reg_names[m & 7]);
   case 040:
      return sprintf(buf, "-(%s)", dbg_reg_names[m & 7]);
   case 050:
      return sprintf(buf, "@-(%s)", dbg_reg_names[m & 7]);
   case 060:
      return sprintf(buf, "%06o(%s)", read16(a), dbg_reg_names[m & 7]);
   case 070:
      return sprintf(buf, "@%06o(%s)", read16(a), dbg_reg_names[m & 7]);
   }
   return 0;
}

static uint16_t disasm(char *buf, uint16_t a) {
   uint16_t ins;

   // Fetch the instruction
   ins = read16(a);

   // Lookup the instruction in table
   D l = disamtable[(sizeof(disamtable) / sizeof(disamtable[0])) - 1];
   uint8_t i;
   for (i = 0; disamtable[i].inst; i++) {
      l = disamtable[i];
      if ((ins & l.inst) == l.arg) {
         break;
      }
   }

   // Decode the instruction
   uint16_t s = (ins & 07700) >> 6;
   uint16_t d = ins & 077;
   uint8_t  o = ins & 0377;

   // Work out the lenth
   int len = 2;
   if (l.flag & S) {
      len += disaslen(s);
   }
   if (l.flag & DD) {
      len += disaslen(d);
   }

   // Ouput the address
   buf += sprintf(buf, "%06o : ", a);

   // Output the instruction, max len is 3 words
   for (int i = 0; i < 6; i += 2) {
      if (i < len) {
         buf += sprintf(buf, "%06o ", read16(a + i));
      } else {
         buf += sprintf(buf, "       ");
      }
   }
   buf += sprintf(buf, ": ");

   // Consume the instruction
   a += 2;

   // Output the menmonic
   if (disamtable[i].inst == 0) {
      sprintf(buf, "???");
      return a;
   }
   buf += sprintf(buf, "%s", l.msg);

   // Output the byte mode qualifier
   if (l.b && (ins & 0100000)) {
      buf += sprintf(buf, "B");
   }

   // Output the operands
   switch (l.flag) {
   case S | DD:
      buf += sprintf(buf, " ");
      buf += disasmaddr(buf, s, a);
      buf += sprintf(buf, ",");
      // Consume the operand (if there us one)
      a   += disaslen(s);
      // Fall thrrough to DD
   case DD:
      buf += sprintf(buf, " ");
      buf += disasmaddr(buf, d, a);
      // Consume the operand (if there us one)
      a   += disaslen(d);
      break;
   case RR | O:
      // this is SOB
      buf += sprintf(buf, " %s,", dbg_reg_names[(ins & 0700) >> 6]);
      o &= 077;
      buf += sprintf(buf, " %06o", (a - (2 * o)) & 0xffff);
      break;
   case O:
      if (o & 0x80) {
         buf += sprintf(buf, " %06o", (a - (2 * ((0xFF ^ o) + 1))) & 0xffff);
      } else {
         buf += sprintf(buf, " %06o", (a + (2 * o)) & 0xffff);
      };
      break;
   case RR | DD:
      buf += sprintf(buf, " %s, ", dbg_reg_names[(ins & 0700) >> 6]);
      buf += disasmaddr(buf, d, a);
      // Consume the operand (if there us one)
      a   += disaslen(d);
      break;
   case RR:
      sprintf(buf, " %s", dbg_reg_names[ins & 7]);
      break;
   case NN:
      sprintf(buf, " %d", ins & 0xFF);
      break;
   case N:
      sprintf(buf, " %d", ins & 0xF);
      break;
   }
   return a;
}
