#include <stdio.h>
#include <stdint.h>
#include <inttypes.h>
#include <string.h>

#include "../cpu_debug.h"
#include "../copro-riscv.h"

#include "riscv_disas.h"
#include "riscv_debug.h"

/*****************************************************
 * CPU Debug Interface
 *****************************************************/

int riscv_debug_enabled = 0;

enum register_numbers {
   i_X0,
   i_X1,
   i_X2,
   i_X3,
   i_X4,
   i_X5,
   i_X6,
   i_X7,
   i_X8,
   i_X9,
   i_X10,
   i_X11,
   i_X12,
   i_X13,
   i_X14,
   i_X15,
   i_X16,
   i_X17,
   i_X18,
   i_X19,
   i_X20,
   i_X21,
   i_X22,
   i_X23,
   i_X24,
   i_X25,
   i_X26,
   i_X27,
   i_X28,
   i_X29,
   i_X30,
   i_X31,
   i_PC,
   i_MSTATUS,
   i_CYCLEL,
   i_CYCLEH,
   i_TIMERL,
   i_TIMERH,
   i_TIMERMATCHL,
   i_TIMERMATCHH,
   i_MSCRATCH,
   i_MTVEC,
   i_MIE,
   i_MIP,
   i_MEPC,
   i_MTVAL,
   i_MCAUSE,
   i_EXTRAFLAGS,
};


// NULL pointer terminated list of register names.
static const char * const dbg_reg_names[] = {
   "zero", // X0
   "ra",   // X1
   "sp",   // X2
   "gp",   // X3
   "tp",   // X4
   "t0",   // X5
   "t1",   // X6
   "r2",   // X7
   "s0",   // X8
   "s1",   // X9
   "a0",   // X10
   "a1",   // X11
   "a2",   // X12
   "a3",   // X13
   "a4",   // X14
   "a5",   // X15
   "a6",   // X16
   "a7",   // X17
   "s2",   // X18
   "s3",   // X19
   "s4",   // X20
   "s5",   // X21
   "s6",   // X22
   "s7",   // X23
   "s8",   // X24
   "s9",   // X25
   "s10",  // X26
   "s11",  // X27
   "t3",   // X28
   "t4",   // X29
   "t5",   // X30
   "t6",   // X31
   "pc",
   "mstatus",
   "cyclel",
   "cycleh",
   "timerl",
   "timerh",
   "timermatchl",
   "timermatchh",
   "mscratch",
   "mtvec",
   "mie",
   "mip",
   "mepc",
   "mtval",
   "mcause",
   "extraflags",
   NULL
};


// NULL pointer terminated list of trap names.
static const char * const dbg_trap_names[] = {
   NULL
};

// enable/disable debugging on this CPU, returns previous value.
static int dbg_debug_enable(int newvalue) {
   int oldvalue = riscv_debug_enabled;
   riscv_debug_enabled = newvalue;
   return oldvalue;
}

// CPU's usual memory read function for data.
static uint32_t dbg_memread(uint32_t addr) {
   return copro_riscv_read_mem32(addr);
}

// CPU's usual memory write function.
static void dbg_memwrite(uint32_t addr, uint32_t value) {
   copro_riscv_write_mem32(addr, value);
}

static uint32_t dbg_disassemble(uint32_t addr, char *buf, size_t bufsize) {
   rv_inst inst = (uint64_t) copro_riscv_read_mem32(addr);
   riscv_disasm_inst(buf, bufsize, rv32, (uint64_t) addr, inst);
   return addr + 4;
}

// Get a register - which is the index into the names above
static uint32_t dbg_reg_get(int which) {
   switch (which) {
   case i_PC:          return riscv_state->pc;
   case i_MSTATUS:     return riscv_state->mstatus;
   case i_CYCLEL:      return riscv_state->cyclel;
   case i_CYCLEH:      return riscv_state->cycleh;
   case i_TIMERL:      return riscv_state->timerl;
   case i_TIMERH:      return riscv_state->timerh;
   case i_TIMERMATCHL: return riscv_state->timermatchl;
   case i_TIMERMATCHH: return riscv_state->timermatchh;
   case i_MSCRATCH:    return riscv_state->mscratch;
   case i_MTVEC:       return riscv_state->mtvec;
   case i_MIE:         return riscv_state->mie;
   case i_MIP:         return riscv_state->mip;
   case i_MEPC:        return riscv_state->mepc;
   case i_MTVAL:       return riscv_state->mtval;
   case i_MCAUSE:      return riscv_state->mcause;
   case i_EXTRAFLAGS:  return riscv_state->extraflags;
   default:
      if (which < 32) {
         return riscv_state->regs[which];
      } else {
         return 0;
      }
   }
}

// Set a register.
static void  dbg_reg_set(int which, uint32_t value) {
   switch (which) {
   case i_PC:          riscv_state->pc          = value; break;
   case i_MSTATUS:     riscv_state->mstatus     = value; break;
   case i_CYCLEL:      riscv_state->cyclel      = value; break;
   case i_CYCLEH:      riscv_state->cycleh      = value; break;
   case i_TIMERL:      riscv_state->timerl      = value; break;
   case i_TIMERH:      riscv_state->timerh      = value; break;
   case i_TIMERMATCHL: riscv_state->timermatchl = value; break;
   case i_TIMERMATCHH: riscv_state->timermatchh = value; break;
   case i_MSCRATCH:    riscv_state->mscratch    = value; break;
   case i_MTVEC:       riscv_state->mtvec       = value; break;
   case i_MIE:         riscv_state->mie         = value; break;
   case i_MIP:         riscv_state->mip         = value; break;
   case i_MEPC:        riscv_state->mepc        = value; break;
   case i_MTVAL:       riscv_state->mtval       = value; break;
   case i_MCAUSE:      riscv_state->mcause      = value; break;
   case i_EXTRAFLAGS:  riscv_state->extraflags  = value; break;
   default:
      if (which < 32) {
         riscv_state->regs[which] = value;
      }
   }
}

// Print register value in CPU standard form.
static size_t dbg_reg_print(int which, char *buf, size_t bufsize) {
   return (size_t)snprintf(buf, bufsize, "%08"PRIx32, dbg_reg_get(which));
}

// Parse a value into a register.
static void dbg_reg_parse(int which, const char *strval) {
   uint32_t val = 0;
   sscanf(strval, "%"SCNx32, &val);
   dbg_reg_set(which, val);
}

static uint32_t dbg_get_instr_addr() {
   return riscv_state->pc;
}

cpu_debug_t const riscv_cpu_debug = {
   .cpu_name       = "RISCV",
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
   .mem_width      = WIDTH_32BITS,
};
