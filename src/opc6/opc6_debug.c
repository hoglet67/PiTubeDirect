#include <stdio.h>
#include <stdint.h>
#include <inttypes.h>
#include <string.h>

#include "../cpu_debug.h"
#include "../copro-opc6.h"

#include "opc6.h"
#include "opc6_debug.h"

/*****************************************************
 * CPU Debug Interface
 *****************************************************/

int opc6_debug_enabled = 0;

enum register_numbers {
   i_R0,
   i_R1,
   i_R2,
   i_R3,
   i_R4,
   i_R5,
   i_R6,
   i_R7,
   i_R8,
   i_R9,
   i_R10,
   i_R11,
   i_R12,
   i_R13,
   i_R14,
   i_PC,
   i_PSR,
   i_PC_int,
   i_PSR_int,
};

// NULL pointer terminated list of register names.
static const char *dbg_reg_names[] = {
   "R0",
   "R1",
   "R2",
   "R3",
   "R4",
   "R5",
   "R6",
   "R7",
   "R8",
   "R9",
   "R10",
   "R11",
   "R12",
   "R13",
   "R14",
   "PC",
   "PSR",
   "PC_int",
   "PSR_int",
   NULL
};


static const char *opcode_names[] = {
   "mov",
   "and",
   "or",
   "xor",
   "add",
   "adc",
   "sto",
   "ld",
   "ror",
   "jsr",
   "sub",
   "sbc",
   "inc",
   "lsr",
   "dec",
   "asr",
   "halt",
   "bswp",
   "putpsr",
   "getpsr",
   "rti",
   "not",
   "out",
   "in",
   "push",
   "pop",
   "cmp",
   "cmpc",
   "???",
   "???",
   "???",
   "???"
};

static const char *pred_names[] = {
   "",
   "",
   "z.",
   "nz.",
   "c.",
   "nc.",
   "mi.",
   "pl.",
};

// NULL pointer terminated list of trap names.
static const char *dbg_trap_names[] = {
   NULL
};

// enable/disable debugging on this CPU, returns previous value.
static int dbg_debug_enable(int newvalue) {
   int oldvalue = opc6_debug_enabled;
   opc6_debug_enabled = newvalue;
   return oldvalue;
};

// CPU's usual memory read function for data.
static uint32_t dbg_memread(uint32_t addr) {
   return copro_opc6_read_mem(addr);
};

// CPU's usual memory write function.
static void dbg_memwrite(uint32_t addr, uint32_t value) {
   copro_opc6_write_mem(addr, value);
};

// CPU's usual io read function for data.
static uint32_t dbg_ioread(uint32_t addr) {
   return copro_opc6_read_io(addr);
};

// CPU's usual io write function.
static void dbg_iowrite(uint32_t addr, uint32_t value) {
   copro_opc6_write_io(addr, value);
};

static uint32_t dbg_disassemble(uint32_t addr, char *buf, size_t bufsize) {
   int len;

   // Read the instruction
   uint16_t instr = copro_opc6_read_mem(addr);

   // Output address/instruction in hex
   len = snprintf(buf, bufsize, "%04x %04x ", (uint16_t) addr, instr);
   buf += len;
   bufsize -= len;

   // Move on to next word
   addr += 1;

   // Decode the instruction
   int opcode = (instr >> OPCODE) & 15;
   int pred = (instr >> PRED) & 7;
   if (pred == 1) {
      opcode += 16;
   }
   int dst = (instr >> DST) & 15;
   int src = (instr >> SRC) & 15;

   // Output operand in hex (or padding)
   int operand_present = ((instr >> LEN) & 1);
   int operand = 0;
   if (operand_present) {
      operand = copro_opc6_read_mem(addr);
      addr += 1;
      len = snprintf(buf, bufsize, "%04x ", operand);
   } else {
      len = snprintf(buf, bufsize, "     ");
   }
   buf += len;
   bufsize -= len;

   // Output instruction
   len = snprintf(buf, bufsize, (opcode == op_inc || opcode == op_dec) ? " : %s%s r%d, %d" : ": %s%s r%d, r%d",
                  pred_names[pred], opcode_names[opcode], dst, src);
   buf += len;
   bufsize -= len;

   // Output optional operand
   if (operand_present) {
      snprintf(buf, bufsize, ", 0x%04x", operand);
   }

   return addr;
};

// Get a register - which is the index into the names above
static uint32_t dbg_reg_get(int which) {
   if (which == i_PSR) {
      return m_opc6->psr;
   } else if (which == i_PSR_int) {
      return m_opc6->psr_int;
   } else if (which == i_PC_int) {
      return m_opc6->pc_int;
   } else {
      return m_opc6->reg[which];
   }
};

// Set a register.
static void  dbg_reg_set(int which, uint32_t value) {
   if (which == i_PSR) {
      m_opc6->psr = value;
   } else if (which == i_PSR_int) {
      m_opc6->psr_int = value;
   } else if (which == i_PC_int) {
      m_opc6->pc_int = value;
   } else {
      m_opc6->reg[which] = value;
   }
};

static const char* flagname = "EI S C Z ";

// Print register value in CPU standard form.
static size_t dbg_reg_print(int which, char *buf, size_t bufsize) {
   if (which == i_PSR) {
      int i;
      int bit;
      char c;
      const char *flagnameptr = flagname;
      int psr = dbg_reg_get(i_PSR);

      if (bufsize < 40) {
         strncpy(buf, "buffer too small!!!", bufsize);
      }

      // Print the 4-bit SWI field
      *buf++ = 'S';
      *buf++ = 'W';
      *buf++ = 'I';
      *buf++ = ':';
      sprintf(buf, "%x", (psr >> 4) & 0x0F);
      buf++;
      *buf++ = ' ';

      // Print the remaining flag bits
      bit = 0x8;
      for (i = 0; i < 4; i++) {
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
   } else {
      return snprintf(buf, bufsize, "%04"PRIx32, dbg_reg_get(which));
   }
};

// Parse a value into a register.
static void dbg_reg_parse(int which, const char *strval) {
   uint32_t val = 0;
   sscanf(strval, "%"SCNx32, &val);
   dbg_reg_set(which, val);
};

static uint32_t dbg_get_instr_addr() {
   return m_opc6->saved_pc;
}

cpu_debug_t opc6_cpu_debug = {
   .cpu_name       = "OPC6",
   .debug_enable   = dbg_debug_enable,
   .memread        = dbg_memread,
   .memwrite       = dbg_memwrite,
   .ioread         = dbg_ioread,
   .iowrite        = dbg_iowrite,
   .disassemble    = dbg_disassemble,
   .reg_names      = dbg_reg_names,
   .reg_get        = dbg_reg_get,
   .reg_set        = dbg_reg_set,
   .reg_print      = dbg_reg_print,
   .reg_parse      = dbg_reg_parse,
   .get_instr_addr = dbg_get_instr_addr,
   .trap_names     = dbg_trap_names,
   .mem_width      = WIDTH_16BITS,
   .io_width       = WIDTH_8BITS
};
