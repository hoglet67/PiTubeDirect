#include <stdio.h>
#include <stdint.h>
#include <inttypes.h>
#include <string.h>

#include "../cpu_debug.h"

#include "cpu80186.h"
#include "mem80186.h"
#include "iop80186.h"
#include "cpu80186_debug.h"

#define MAXOPLEN 6

extern int i386_dasm_one();

/*****************************************************
 * CPU Debug Interface
 *****************************************************/

int cpu80186_debug_enabled = 0;

enum register_numbers {
   i_IP,
   i_FLAGS,
   i_AX,
   i_BX,
   i_CX,
   i_DX,
   i_DI,
   i_SI,
   i_BP,
   i_SP,
   i_CS,
   i_DS,
   i_ES,
   i_SS
};

// NULL pointer terminated list of register names.
static const char *dbg_reg_names[] = {
   "IP",
   "FLAGS",
   "AX",
   "BX",
   "CX",
   "DX",
   "DI",
   "SI",
   "BP",
   "SP",
   "CS",
   "DS",
   "ES",
   "SS",
   NULL
};

// NULL pointer terminated list of trap names.
static const char *dbg_trap_names[] = {
   NULL
};

// enable/disable debugging on this CPU, returns previous value.
static int dbg_debug_enable(int newvalue) {
   int oldvalue = cpu80186_debug_enabled;
   cpu80186_debug_enabled = newvalue;
   return oldvalue;
};

// CPU's usual memory read function for data.
static uint32_t dbg_memread(uint32_t addr) {
   return read86(addr);
};

// CPU's usual memory write function.
static void dbg_memwrite(uint32_t addr, uint32_t value) {
   write86(addr, value);
};

// CPU's usual IO read function for data.
static uint32_t dbg_ioread(uint32_t addr) {
   return portin(addr);
};

// CPU's usual IO write function.
static void dbg_iowrite(uint32_t addr, uint32_t value) {
   portout(addr, value);
};

static uint32_t dbg_disassemble(uint32_t addr, char *buf, size_t bufsize) {
   char instr[100];
   int oplen = i386_dasm_one(instr, addr, 0, 0) & 0xffff;
   int len = snprintf(buf, bufsize, "%06"PRIx32" ", addr);
   buf += len;
   bufsize -= len;
   for (int i = 0; i < MAXOPLEN; i++) {
      if (i < oplen) {
         len = snprintf(buf, bufsize, "%02x ", read86(addr + i));
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
   case i_IP:
      return ip;
   case i_FLAGS:
      return getflags86();
   case i_AX:
      return getreg16(regax);
   case i_BX:
      return getreg16(regbx);
   case i_CX:
      return getreg16(regcx);
   case i_DX:
      return getreg16(regdx);
   case i_DI:
      return getreg16(regdi);
   case i_SI:
      return getreg16(regsi);
   case i_BP:
      return getreg16(regbp);
   case i_SP:
      return getreg16(regsp);
   case i_CS:
      return getsegreg(regcs);
   case i_DS:
      return getsegreg(regds);
   case i_ES:
      return getsegreg(reges);
   case i_SS:
      return getsegreg(regss);
   }
   return 0;
};

// Set a register.
static void  dbg_reg_set(int which, uint32_t value) {
   switch (which) {
   case i_IP:
      ip = value;
      break;
   case i_FLAGS:
      putflags86(value);
      break;
   case i_AX:
      putreg16(regax, value);
      break;
   case i_BX:
      putreg16(regbx, value);
      break;
   case i_CX:
      putreg16(regcx, value);
      break;
   case i_DX:
      putreg16(regdx, value);
      break;
   case i_DI:
      putreg16(regdi, value);
      break;
   case i_SI:
      putreg16(regsi, value);
      break;
   case i_BP:
      putreg16(regbp, value);
      break;
   case i_SP:
      putreg16(regsp, value);
      break;
   case i_CS:
      putsegreg(regcs, value);
      break;
   case i_DS:
      putsegreg(regds, value);
      break;
   case i_ES:
      putsegreg(reges, value);
      break;
   case i_SS:
      putsegreg(regss, value);
      break;
   }
};

static const char* flagname = "O D I T S Z * A * P * C ";

// Print register value in CPU standard form.
static size_t dbg_reg_print(int which, char *buf, size_t bufsize) {
   if (which == i_FLAGS) {
      int i;
      int bit;
      char c;
      const char *flagnameptr = flagname;
      int psr = dbg_reg_get(which);

      if (bufsize < 40) {
         strncpy(buf, "buffer too small!!!", bufsize);
      }

      bit = 0x800;
      for (i = 0; i < 12; i++) {
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
   return getinstraddr86();
}

cpu_debug_t cpu80186_cpu_debug = {
   .cpu_name       = "80x86",
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
   .trap_names     = dbg_trap_names
};
