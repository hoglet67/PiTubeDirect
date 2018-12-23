#include <stdio.h>
#include <stdint.h>
#include <inttypes.h>
#include <string.h>

#include "cpu_debug.h"

#include "copro-lib6502.h"
#include "lib6502.h"
#include "lib6502_debug.h"

/*****************************************************
 * CPU Debug Interface
 *****************************************************/

int lib6502_debug_enabled = 0;

volatile int lib6502_last_PC = 0;

enum register_numbers {
   i_A,
   i_X,
   i_Y,
   i_P,
   i_S,
   i_PC
};

// NULL pointer terminated list of register names.
static const char *dbg_reg_names[] = {
   "A",
   "X",
   "Y",
   "P",
   "S",
   "PC",
   NULL
};

// NULL pointer terminated list of trap names.
static const char *dbg_trap_names[] = {
   "BRK",
   NULL
};

// enable/disable debugging on this CPU, returns previous value.
static int dbg_debug_enable(int newvalue) {
   int oldvalue = lib6502_debug_enabled;
   lib6502_debug_enabled = newvalue;
   return oldvalue;
};

// CPU's usual memory read function for data.
static uint32_t dbg_memread(uint32_t addr) {
   return copro_lib6502_mem_read(copro_lib6502_mpu, addr, 0);
};

// CPU's usual memory write function.
static void dbg_memwrite(uint32_t addr, uint32_t value) {
   copro_lib6502_mem_write(copro_lib6502_mpu, addr, value);
};

static uint32_t dbg_disassemble(uint32_t addr, char *buf, size_t bufsize) {
   char instr[64];
   int oplen = M6502_disassemble(copro_lib6502_mpu, addr, instr);
   int len = snprintf(buf, bufsize, "%04"PRIx32" ", addr);
   buf += len;
   bufsize -= len;
   for (int i = 0; i < 3; i++) {
      if (i < oplen) {
         len = snprintf(buf, bufsize, "%02"PRIx32" ", dbg_memread(addr + i));
      } else {
         len = snprintf(buf, bufsize, "   ");
      }
      buf += len;
      bufsize -= len;
   }
   strncpy(buf, instr, bufsize);
   return addr + oplen;
};

// Get a register - which is the index into the names above
static uint32_t dbg_reg_get(int which) {
   switch (which) {
   case i_A:
      return copro_lib6502_mpu->registers->a;
   case i_X:
      return copro_lib6502_mpu->registers->x;
   case i_Y:
      return copro_lib6502_mpu->registers->y;
   case i_P:
      return copro_lib6502_mpu->registers->p;
   case i_S:
      return copro_lib6502_mpu->registers->s;
   case i_PC:
      return copro_lib6502_mpu->registers->pc;
   default:
      return 0;
   }
};

// Set a register.
static void  dbg_reg_set(int which, uint32_t value) {
   switch (which) {
   case i_A:
      copro_lib6502_mpu->registers->a = value;
      break;
   case i_X:
      copro_lib6502_mpu->registers->x = value;
      break;
   case i_Y:
      copro_lib6502_mpu->registers->y = value;
      break;
   case i_P:
      copro_lib6502_mpu->registers->p = value;
      break;
   case i_S:
      copro_lib6502_mpu->registers->s = value;
      break;
   case i_PC:
      copro_lib6502_mpu->registers->pc = value;
      break;
   }
};

static const char* flagname = "N V * B D I Z C ";

// Print register value in CPU standard form.
static size_t dbg_reg_print(int which, char *buf, size_t bufsize) {
   if (which == i_P) {
      int i;
      int bit;
      char c;
      const char *flagnameptr = flagname;
      int psr = dbg_reg_get(which);

      if (bufsize < 40) {
         strncpy(buf, "buffer too small!!!", bufsize);
      }

      bit = 0x80;
      for (i = 0; i < 8; i++) {
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
      return strlen(buf);
   } else if (which == i_PC) {
      return snprintf(buf, bufsize, "%04"PRIx32, dbg_reg_get(which));
   } else {
      return snprintf(buf, bufsize, "%02"PRIx32, dbg_reg_get(which));
   }
};

// Parse a value into a register.
static void dbg_reg_parse(int which, const char *strval) {
   uint32_t val = 0;
   sscanf(strval, "%"SCNx32, &val);
   dbg_reg_set(which, val);
};

static uint32_t dbg_get_instr_addr() {
   return lib6502_last_PC;
}

cpu_debug_t lib6502_cpu_debug = {
   .cpu_name       = "lib6502",
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
