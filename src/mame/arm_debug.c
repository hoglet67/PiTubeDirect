#include <stdio.h>
#include <stdint.h>
#include <inttypes.h>
#include <string.h>

#include "../cpu_debug.h"

#include "arm.h"
#include "arm_debug.h"
#include "../darm/darm.h"

#define ADDRESS_MASK    ((UINT32) 0x03fffffcu)

static darm_t dis;
static darm_str_t dis_str;

/*****************************************************
 * CPU Debug Interface
 *****************************************************/

int arm2_debug_enabled = 0;

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
   i_SP,
   i_LR,
   i_PC,
   i_R8_fiq,
   i_R9_fiq,
   i_R10_fiq,
   i_R11_fiq,
   i_R12_fiq,
   i_SP_fiq,
   i_LR_fiq,
   i_SP_irq,
   i_LR_irq,
   i_SP_svc,
   i_LR_svc,
   i_PSR
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
   "SP",
   "LR",
   "PC",
   "R8_fiq",
   "R9_fiq",
   "R10_fiq",
   "R11_fiq",
   "R12_fiq",
   "SP_fiq",
   "LR_fiq",
   "SP_irq",
   "LR_irq",
   "SP_svc",
   "LR_svc",
   "PSR",
   NULL
};

// NULL pointer terminated list of trap names.
static const char *dbg_trap_names[] = {
   NULL
};

// enable/disable debugging on this CPU, returns previous value.
static int dbg_debug_enable(int newvalue) {
   int oldvalue = arm2_debug_enabled;
   arm2_debug_enabled = newvalue;
   return oldvalue;
};

// CPU's usual memory read function for data.
static uint32_t dbg_memread(uint32_t addr) {
   return copro_arm2_read8(addr);
};

// CPU's usual memory write function.
static void dbg_memwrite(uint32_t addr, uint32_t value) {
   copro_arm2_write8(addr, value);
};

static uint32_t dbg_disassemble(uint32_t addr, char *buf, size_t bufsize) {
   uint32_t instr = copro_arm2_read32(addr);
   int len = snprintf(buf, bufsize, "%08"PRIx32" %08"PRIx32" ", addr, instr);
   buf += len;
   bufsize -= len;
   int ok = 0;
   if (darm_armv7_disasm(&dis, instr) == 0) {
      dis.addr = addr;
      dis.addr_mask = ADDRESS_MASK;
      if (darm_str2(&dis, &dis_str, 1) == 0) {
         strncpy(buf, dis_str.total, bufsize);
         ok = 1;
      }
   }
   if (!ok) {
      strncpy(buf, "???", bufsize);
   }
   return addr + 4;
};

// Get a register - which is the index into the names above
static uint32_t dbg_reg_get(int which) {
   if (which == i_PC) {
      return m_sArmRegister[i_PC]  & ADDRESS_MASK;
   } else if (which == i_PSR) {
      return ((m_sArmRegister[i_PC] >> 24) & 0xFC) | (m_sArmRegister[i_PC] & 0x03);
   } else {
      return m_sArmRegister[which];
   }
};

// Set a register.
static void  dbg_reg_set(int which, uint32_t value) {
   if (which == i_PC) {
      m_sArmRegister[i_PC] &= ~ADDRESS_MASK;
      m_sArmRegister[i_PC] |= (value & ADDRESS_MASK);
   } else if (which == i_PSR) {
      m_sArmRegister[i_PC] &= ADDRESS_MASK;
      m_sArmRegister[i_PC] |= ((value & 0xFC) << 24) | (value & 0x03);
   } else {
      m_sArmRegister[which] = value;
   }
};

static const char* flagname = "N Z C V I F M1 M0 ";

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
      switch (psr & 3) {
      case 0:
         sprintf(buf, "(USR)");
         break;
      case 1:
         sprintf(buf, "(FIQ)");
         break;
      case 2:
         sprintf(buf, "(IRQ)");
         break;
      case 3:
         sprintf(buf, "(SVC)");
         break;
      }
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
   return m_sArmRegister[i_PC];
}

cpu_debug_t arm2_cpu_debug = {
   .cpu_name       = "ARM2",
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
