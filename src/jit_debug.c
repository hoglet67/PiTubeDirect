#include <stdio.h>
#include <stdint.h>
#include <inttypes.h>
#include <string.h>

#include "cpu_debug.h"
#include "lib6502.h"
#include "jit_debug.h"

/*****************************************************
 * CPU Debug Interface
 *****************************************************/

int jit_debug_enabled = 0;

volatile int jit_last_PC = 0;

enum register_numbers {
   i_A,
   i_X,
   i_Y,
   i_P,
   i_S,
   i_PC
};

// NULL pointer terminated list of register names.
static const char *const dbg_reg_names[] = {
   "A",
   "X",
   "Y",
   "P",
   "S",
   "PC",
   NULL
};

// NULL pointer terminated list of trap names.
static const char *const dbg_trap_names[] = {
   "BRK",
   NULL
};

// enable/disable debugging on this CPU, returns previous value.
static int dbg_debug_enable(int newvalue) {
   int oldvalue = jit_debug_enabled;
   jit_debug_enabled = newvalue;
   return oldvalue;
}

// CPU's usual memory read function for data.
static uint32_t dbg_memread(uint32_t addr) {
  // return (uint32_t) (uint8_t *)addr[0];
  return 0;
}

// CPU's usual memory write function.
static void dbg_memwrite(uint32_t addr, uint32_t value) {
   // (uint8_t *)addr[0] = (uint8_t) value;
}

static uint32_t dbg_disassemble(uint32_t addr, char *buf, size_t bufsize) {
   char instr[64];
   M6502 mpu;
   mpu.memory=0;
   uint32_t oplen = (uint32_t) M6502_disassemble(&mpu, (uint16_t)addr, instr);
   uint32_t len = (uint32_t)snprintf(buf, bufsize, "%04"PRIx32" ", addr);
   buf += len;
   bufsize -= (uint32_t)len;
   for (unsigned int i = 0; i < 3; i++) {
      if (i < oplen) {
         len = (uint32_t)snprintf(buf, bufsize, "%02"PRIx32" ", dbg_memread(addr + i));
      } else {
         len = (uint32_t)snprintf(buf, bufsize, "   ");
      }
      buf += len;
      bufsize -= len;
   }
   strncpy(buf, instr, bufsize);
   return (uint32_t)(addr + oplen);
}

// Get a register - which is the index into the names above
static uint32_t dbg_reg_get(int which) {
   switch (which) {
   case i_A:
      return 1; //copro_jit_mpu->registers->a;
   case i_X:
      return 2; //copro_jit_mpu->registers->x;
   case i_Y:
      return 3; //copro_jit_mpu->registers->y;
   case i_P:
      return 4; //copro_jit_mpu->registers->p;
   case i_S:
      return 5; //copro_jit_mpu->registers->s;
   case i_PC:
      return 6; //copro_jit_mpu->registers->pc;
   default:
      return 0;
   }
}

// Set a register.
static void  dbg_reg_set(int which, uint32_t value) {
    /*
   switch (which) {
   case i_A:
      copro_jit_mpu->registers->a = (uint8_t)value;
      break;
   case i_X:
      copro_jit_mpu->registers->x = (uint8_t)value;
      break;
   case i_Y:
      copro_jit_mpu->registers->y = (uint8_t)value;
      break;
   case i_P:
      copro_jit_mpu->registers->p = (uint8_t)value;
      break;
   case i_S:
      copro_jit_mpu->registers->s = (uint8_t)value;
      break;
   case i_PC:
      copro_jit_mpu->registers->pc = (uint16_t)value;
      break;
   }*/
}

static const char* flagname = "N V * B D I Z C ";

// Print register value in CPU standard form.
static size_t dbg_reg_print(int which, char *buf, size_t bufsize) {
   if (which == i_P) {
      uint32_t bit;
      char c;
      const char *flagnameptr = flagname;
      uint32_t psr = dbg_reg_get(which);

      if (bufsize < 40) {
         strncpy(buf, "buffer too small!!!", bufsize);
      }

      bit = 0x80;
      for (int i = 0; i < 8; i++) {
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
      return (size_t)snprintf(buf, bufsize, "%04"PRIx32, dbg_reg_get(which));
   } else {
      return (size_t)snprintf(buf, bufsize, "%02"PRIx32, dbg_reg_get(which));
   }
}

// Parse a value into a register.
static void dbg_reg_parse(int which, const char *strval) {
   uint32_t val = 0;
   sscanf(strval, "%"SCNx32, &val);
   dbg_reg_set(which, val);
}

static uint32_t dbg_get_instr_addr() {
   return (uint32_t)jit_last_PC;
}

const cpu_debug_t jit_cpu_debug = {
   .cpu_name       = "jit",
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
