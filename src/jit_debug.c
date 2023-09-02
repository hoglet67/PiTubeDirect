#include <stdio.h>
#include <stdint.h>
#include <inttypes.h>
#include <string.h>

#include "cpu_debug.h"
#include "lib6502.h"
#include "jit_debug.h"
#include "darm/darm.h"
#include "copro-65tubejit.h"
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
  return (uint32_t) (((uint8_t *)addr)[0]);
}

// CPU's usual memory write function.
static void dbg_memwrite(uint32_t addr, uint32_t value) {
   (((uint8_t *)addr)[0]) = (uint8_t) value;
}

static uint32_t dbg_disassemble(uint32_t addr, char *buf, size_t bufsize) {
   char instr[64];
   M6502 mpu;
   mpu.memory=0;
   char * start = buf;
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
   uint32_t opstringlen= strlen(instr);
   strncpy(buf, instr, bufsize);
   buf += opstringlen;
   bufsize -= opstringlen;
   darm_t dis;
   darm_str_t dis_str;
   uint32_t padding = 28;
   uint32_t offset =  (uint32_t)(buf - start);
   uint32_t arminstr = oplen<<1;
   uint32_t jitletaddr = JITLET+ ( addr<<3);
   do {
       strncpy(buf,"                            ",padding-offset);
       buf += padding-1-offset;
       bufsize -= padding-1-offset;

       uint32_t instrarm = * (uint32_t *) (jitletaddr);

       len = (uint32_t)snprintf(buf, bufsize, "%08"PRIx32" ", jitletaddr);
      buf += len;
      bufsize -= len;
       if (darm_armv7_disasm(&dis, instrarm) == 0) {
            dis.addr = (jitletaddr);
            darm_str2(&dis, &dis_str, 0);
            strncpy(buf, dis_str.total, bufsize);
            buf += strlen(dis_str.total);
            bufsize -= strlen(dis_str.total);
            strncpy(buf, "\r\n", bufsize);

            buf += 2;
            bufsize -= 2;
       }
       offset =0;
      jitletaddr+=4;
      arminstr--;
   } while (arminstr);
   buf[-1] = 0;
   return (uint32_t)(addr + oplen);
}

// Get a register - which is the index into the names above
static uint32_t dbg_reg_get(int which) {
   uint32_t * ptr = jit_get_regs();
   switch (which) {
   case i_A:
      return ptr[3]& 0xFF;
   case i_X:
      return ptr[4]>>24;
   case i_Y:
      return ptr[1]>>24;
   case i_P:
      return ptr[2];
   case i_S:
      return ptr[0];
   case i_PC:
      return ((ptr[5]-JITLET)>>3);
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
