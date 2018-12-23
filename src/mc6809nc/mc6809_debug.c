#include <stdio.h>
#include <stdint.h>
#include <inttypes.h>
#include <string.h>

#include "../cpu_debug.h"

#include "mc6809.h"
#include "mc6809_dis.h"
#include "mc6809_debug.h"

/*****************************************************
 * CPU Debug Interface
 *****************************************************/

int mc6809nc_debug_enabled = 0;

enum register_numbers {
   i_PC,
   i_CC,
   i_A,
   i_B,
   i_D,
   i_X,
   i_Y,
   i_U,
   i_S,
   i_DP
};

// NULL pointer terminated list of register names.
static const char *dbg_reg_names[] = {
   "PC",
   "CC",
   "A",
   "B",
   "D",
   "X",
   "Y",
   "U",
   "S",
   "DP",
   NULL
};

// NULL pointer terminated list of trap names.
static const char *dbg_trap_names[] = {
   NULL
};

// enable/disable debugging on this CPU, returns previous value.
static int dbg_debug_enable(int newvalue) {
   int oldvalue = mc6809nc_debug_enabled;
   mc6809nc_debug_enabled = newvalue;
   return oldvalue;
};

// CPU's usual memory read function for data.
static uint32_t dbg_memread(uint32_t addr) {
   return read8(addr);
};

// CPU's usual memory write function.
static void dbg_memwrite(uint32_t addr, uint32_t value) {
   write8(addr, value);
};

static uint32_t dbg_disassemble(uint32_t addr, char *buf, size_t bufsize) {
   return mc6809_disassemble(addr, buf, bufsize);
};

// Get a register - which is the index into the names above
static uint32_t dbg_reg_get(int which) {
   switch (which) {
   case i_PC:
      return get_pc();
   case i_CC:
      return get_cc();
   case i_A:
      return get_a();
   case i_B:
      return get_b();
   case i_D:
      return get_d();
   case i_X:
      return get_x();
   case i_Y:
      return get_y();
   case i_U:
      return get_u();
   case i_S:
      return get_s();
   case i_DP:
      return get_dp();
   default:
      return 0;
   }
};

// Set a register.
static void  dbg_reg_set(int which, uint32_t value) {
   switch (which) {
   case i_PC:
      set_pc(value);
      break;
   case i_CC:
      set_cc(value);
      break;
   case i_A:
      set_a(value);
      break;
   case i_B:
      set_b(value);
      break;
   case i_D:
      set_d(value);
      break;
   case i_X:
      set_x(value);
      break;
   case i_Y:
      set_y(value);
      break;
   case i_U:
      set_u(value);
      break;
   case i_S:
      set_s(value);
      break;
   case i_DP:
      set_dp(value);
      break;
   }
};

static const char* flagname = "E H H I N Z V C ";

// Print register value in CPU standard form.
static size_t dbg_reg_print(int which, char *buf, size_t bufsize) {
   if (which == i_CC) {
      int i;
      int bit;
      char c;
      const char *flagnameptr = flagname;
      int psr = dbg_reg_get(i_CC);

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
   } else if (which == i_A || which == i_B || which == i_DP) {
      return snprintf(buf, bufsize, "%02"PRIx32, dbg_reg_get(which));
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
   return get_pc();
}

cpu_debug_t mc6809nc_cpu_debug = {
   .cpu_name       = "MC6809NC",
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
