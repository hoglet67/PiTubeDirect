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
   i_CURUSER
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
   "PU",
   "CU",
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
      int psr = dbg_reg_get(i_PS);
      
      if (bufsize < 40) {
         strncpy(buf, "buffer too small!!!", bufsize);
      }
      
      // Print the I-bit I field
      *buf++ = 'I';
      *buf++ = ':';
      sprintf(buf, "%x", (psr >> 5) & 0x07);
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
   .trap_names     = dbg_trap_names
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

enum { DD = 1 << 1, S = 1 << 2, RR = 1 << 3, O = 1 << 4, N = 1 << 5 };

static D disamtable[] = {
   { 0077700, 0005000, "CLR", DD, true },
   { 0077700, 0005100, "COM", DD, true },
   { 0077700, 0005200, "INC", DD, true },
   { 0077700, 0005300, "DEC", DD, true },
   { 0077700, 0005400, "NEG", DD, true },
   { 0077700, 0005700, "TST", DD, true },
   { 0077700, 0006200, "ASR", DD, true },
   { 0077700, 0006300, "ASL", DD, true },
   { 0077700, 0006000, "ROR", DD, true },
   { 0077700, 0006100, "ROL", DD, true },
   { 0177700, 0000300, "SWAB", DD, false },
   { 0077700, 0005500, "ADC", DD, true },
   { 0077700, 0005600, "SBC", DD, true },
   { 0177700, 0006700, "SXT", DD, false },
   { 0070000, 0010000, "MOV", S | DD, true },
   { 0070000, 0020000, "CMP", S | DD, true },
   { 0170000, 0060000, "ADD", S | DD, false },
   { 0170000, 0160000, "SUB", S | DD, false },
   { 0070000, 0030000, "BIT", S | DD, true },
   { 0070000, 0040000, "BIC", S | DD, true },
   { 0070000, 0050000, "BIS", S | DD, true },
   { 0177000, 0070000, "MUL", RR | DD, false },
   { 0177000, 0071000, "DIV", RR | DD, false },
   { 0177000, 0072000, "ASH", RR | DD, false },
   { 0177000, 0073000, "ASHC", RR | DD, false },
   { 0177400, 0000400, "BR", O, false },
   { 0177400, 0001000, "BNE", O, false },
   { 0177400, 0001400, "BEQ", O, false },
   { 0177400, 0100000, "BPL", O, false },
   { 0177400, 0100400, "BMI", O, false },
   { 0177400, 0101000, "BHI", O, false },
   { 0177400, 0101400, "BLOS", O, false },
   { 0177400, 0102000, "BVC", O, false },
   { 0177400, 0102400, "BVS", O, false },
   { 0177400, 0103000, "BCC", O, false },
   { 0177400, 0103400, "BCS", O, false },
   { 0177400, 0002000, "BGE", O, false },
   { 0177400, 0002400, "BLT", O, false },
   { 0177400, 0003000, "BGT", O, false },
   { 0177400, 0003400, "BLE", O, false },
   { 0177700, 0000100, "JMP", DD, false },
   { 0177000, 0004000, "JSR", RR | DD, false },
   { 0177770, 0000200, "RTS", RR, false },
   { 0177777, 0006400, "MARK", 0, false },
   { 0177000, 0077000, "SOB", RR | O, false },
   { 0177777, 0000005, "RESET", 0, false },
   { 0177700, 0006500, "MFPI", DD, false },
   { 0177700, 0006600, "MTPI", DD, false },
   { 0177777, 0000001, "WAIT", 0, false },
   { 0177777, 0000002, "RTI", 0, false },
   { 0177777, 0000006, "RTT", 0, false },
   { 0177400, 0104000, "EMT", N, false },
   { 0177400, 0104400, "TRAP", N, false },
   { 0177777, 0000003, "BPT", 0, false },
   { 0177777, 0000004, "IOT", 0, false },
   { 0, 0, "", 0, false }, };

static uint16_t read16(uint16_t a) {
   return copro_pdp11_read16(a);
}

static int disasmaddr(char **bufp, uint16_t m, uint16_t a) {
   if (m & 7) {
      switch (m) {
      case 027:
         a += 2;
         *bufp += sprintf(*bufp, "#%06o", read16(a));
         return a;
      case 037:
         a += 2;
         *bufp += sprintf(*bufp, "@#%06o", read16(a));
         return a;
      case 067:
         a += 2;
         *bufp += sprintf(*bufp, "*%06o", (a + 2 + (read16(a))) & 0xFFFF);
         return a;
      case 077:
         *bufp += sprintf(*bufp, "**%06o", (a + 2 + (read16(a))) & 0xFFFF);
         return a;
      }
   }
   
   switch (m & 070) {
   case 000:
      *bufp += sprintf(*bufp, "%s", dbg_reg_names[m & 7]);
      break;
   case 010:
      *bufp += sprintf(*bufp, "(%s)", dbg_reg_names[m & 7]);
      break;
   case 020:
      *bufp += sprintf(*bufp, "(%s)+", dbg_reg_names[m & 7]);
      break;
   case 030:
      *bufp += sprintf(*bufp, "*(%s)+", dbg_reg_names[m & 7]);
      break;
   case 040:
      *bufp += sprintf(*bufp, "-(%s)", dbg_reg_names[m & 7]);
      break;
   case 050:
      *bufp += sprintf(*bufp, "*-(%s)", dbg_reg_names[m & 7]);
      break;
   case 060:
      a += 2;
      *bufp += sprintf(*bufp, "%06o(%s)", read16(a), dbg_reg_names[m & 7]);
      break;
   case 070:
      a += 2;
      *bufp += sprintf(*bufp, "*%06o(%s)", read16(a), dbg_reg_names[m & 7]);
      break;
   }   
   return a;
}

static uint16_t disasm(char *buf, uint16_t a) {
   uint16_t ins;
   ins = read16(a);
   buf += sprintf(buf, "%06o %06o : ", a, ins);   
   D l = disamtable[(sizeof(disamtable) / sizeof(disamtable[0])) - 1];
   uint8_t i;
   for (i = 0; disamtable[i].inst; i++) {
      l = disamtable[i];
      if ((ins & l.inst) == l.arg) {
         break;
      }
   }
   if (l.inst == 0) {
      sprintf(buf, "???");
      return a + 2;
   }
   buf += sprintf(buf, l.msg);
   if (l.b && (ins & 0100000)) {
      buf += sprintf(buf, "B");
   }
   uint16_t s = (ins & 07700) >> 6;
   uint16_t d = ins & 077;
   uint8_t o = ins & 0377;
   switch (l.flag) {
   case S | DD:
      buf += sprintf(buf, " ");
      a = disasmaddr(&buf, s, a);
      buf += sprintf(buf, ",");
   case DD:
      buf += sprintf(buf, " ");
      a = disasmaddr(&buf, d, a);
      break;
   case RR | O:
      buf += sprintf(buf, " %s,", dbg_reg_names[(ins & 0700) >> 6]);
      o &= 077;
   case O:
      if (o & 0x80) {
         buf += sprintf(buf, " -%03o", (2 * ((0xFF ^ o) + 1)));
      } else {
         buf += sprintf(buf, " +%03o", (2 * o));
      };
      break;
   case RR | DD:
      buf += sprintf(buf, " %s, ", dbg_reg_names[(ins & 0700) >> 6]);
      a = disasmaddr(&buf, d, a);
   case RR:
      buf += sprintf(buf, " %s", dbg_reg_names[ins & 7]);
   }
   return a + 2;
}

