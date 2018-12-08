#include <stdio.h>
#include <stdint.h>
#include <inttypes.h>
#include <string.h>

#include "../cpu_debug.h"
#include "../copro-opc7.h"

#include "opc7.h"
#include "opc7_debug.h"

/*****************************************************
 * CPU Debug Interface
 *****************************************************/

int opc7_debug_enabled = 0;

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
   "movt",
   "xor",
   "and",
   "or",
   "not",
   "cmp",
   "sub",
   "add",
   "bperm",
   "ror",
   "lsr",
   "jsr",
   "asr",
   "rol",
   "???",
   "halt",
   "rti",
   "putpsr",
   "getpsr",
   "???",
   "???",
   "???",
   "???",
   "out",
   "in",
   "sto",
   "ld",
   "ljsr",
   "lmov",
   "lsto",
   "lld"
};

static const char *pred_names[] = {
   "",
   "0.",
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
   int oldvalue = opc7_debug_enabled;
   opc7_debug_enabled = newvalue;
   return oldvalue;
};

// CPU's usual memory read function for data.
static uint32_t dbg_memread(uint32_t addr) {
   return copro_opc7_read_mem(addr);
};

// CPU's usual memory write function.
static void dbg_memwrite(uint32_t addr, uint32_t value) {
   copro_opc7_write_mem(addr, value);
};

// CPU's usual io read function for data.
static uint32_t dbg_ioread(uint32_t addr) {
   return copro_opc7_read_io(addr);
};

// CPU's usual io write function.
static void dbg_iowrite(uint32_t addr, uint32_t value) {
   copro_opc7_write_io(addr, value);
};

static uint32_t dbg_disassemble(uint32_t addr, char *buf, size_t bufsize) {
   int len;

   // Read the instruction
   uint32_t instr = copro_opc7_read_mem(addr);

   // Output address/instruction in hex
   len = snprintf(buf, bufsize, "%05"PRIx32" %08"PRIx32" ", addr, instr);
   buf += len;
   bufsize -= len;

   // Move on to next word
   addr += 1;

   // Decode the instruction
   int opcode = (instr >> OPCODE) & 31;
   int pred = (instr >> PRED) & 7;
   int dst = (instr >> DST) & 15;
   int src = (instr >> SRC) & 15;

   // Output operand in hex (or padding)
   uint32_t operand;
   if (opcode < op_ljsr) {
      operand = instr & 0xFFFF;
      if (instr & 0x8000) {
         operand |= 0xFFFF0000;
      }
   } else {
      operand = instr & 0xFFFFF;
      if (instr & 0x80000) {
         operand |= 0xFFF00000;
      }
   }

   // Output instruction
   if (opcode < op_ljsr) {
      snprintf(buf, bufsize, ": %s%s r%d, r%d, %08"PRIx32,
                     pred_names[pred], opcode_names[opcode], dst, src, operand);      
   } else {
      snprintf(buf, bufsize, ": %s%s r%d, %08"PRIx32,
                     pred_names[pred], opcode_names[opcode], dst, operand);            
   }

   return addr;
};

// Get a register - which is the index into the names above
static uint32_t dbg_reg_get(int which) {
   if (which == i_PSR) {
      return m_opc7->psr;
   } else if (which == i_PSR_int) {
      return m_opc7->psr_int;
   } else if (which == i_PC_int) {
      return m_opc7->pc_int;
   } else {
      return m_opc7->reg[which];
   }
};

// Set a register.
static void  dbg_reg_set(int which, uint32_t value) {
   if (which == i_PSR) {
      m_opc7->psr = value;
   } else if (which == i_PSR_int) {
      m_opc7->psr_int = value;
   } else if (which == i_PC_int) {
      m_opc7->pc_int = value;
   } else {
      m_opc7->reg[which] = value;
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
      return snprintf(buf, bufsize, "%08"PRIx32, dbg_reg_get(which));
   }
};

// Parse a value into a register.
static void dbg_reg_parse(int which, const char *strval) {
   uint32_t val = 0;
   sscanf(strval, "%"SCNx32, &val);
   dbg_reg_set(which, val);
};

static uint32_t dbg_get_instr_addr() {
   return m_opc7->saved_pc;
}

cpu_debug_t opc7_cpu_debug = {
   .cpu_name       = "OPC7",
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
   .mem_width      = WIDTH_32BITS,
   .io_width       = WIDTH_8BITS
};
