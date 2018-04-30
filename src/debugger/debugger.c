#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "debugger.h"
#include "linenoise.h"

#include "../rpi-aux.h"
#include "../cpu_debug.h"

#include "../lib6502_debug.h"
#include "../yaze/simz80.h"
#include "../cpu80186/cpu80186_debug.h"
#include "../mc6809nc/mc6809_debug.h"
#include "../mame/arm_debug.h"
#include "../NS32016/32016_debug.h"
#include "../opc5ls/opc5ls_debug.h"
#include "../opc6/opc6_debug.h"
#include "../opc7/opc7_debug.h"
#include "../pdp11/pdp11_debug.h"

#define USE_LINENOISE

#define HAS_IO (getCpu()->ioread != NULL)

#define BN_DISABLED 0xFFFFFFFF

static const char * prompt_str = ">> ";

extern unsigned int copro;

cpu_debug_t *cpu_debug_list[] = {
   NULL,                //  0 65tube
   NULL,                //  1 65tube
   &lib6502_cpu_debug,  //  2 lib6502
   &lib6502_cpu_debug,  //  3 lib6502
   &simz80_cpu_debug,   //  4 Z80
   &opc5ls_cpu_debug,   //  5 OPC5LS
   &opc6_cpu_debug,     //  6 OPC6
   &opc7_cpu_debug,     //  7 OPC7
   &cpu80186_cpu_debug, //  8 80x86
   &mc6809nc_cpu_debug, //  9 6809
   NULL,                // 10 unused
   &pdp11_cpu_debug,    // 11 PDP11
   &arm2_cpu_debug,     // 12 ARM2
   &n32016_cpu_debug,   // 13 32016
   NULL,                // 14 unused
   NULL,                // 15 Native ARM
};

#define NUM_CMDS 21
#define NUM_IO_CMDS 6

// The Atom CRC Polynomial
#define CRC_POLY          0x002d

// The space available for address comparators depends on the size of the CPU core
#define MAXBKPTS 8

// The number of different watch/breakpoint modes
#define NUM_MODES   3

// The following watch/breakpoint modes are defined
#define MODE_LAST     0
#define MODE_WATCH    1
#define MODE_BREAK    2

// Breakpoint Mode Strings, should match the modes above
static const char *modeStrings[NUM_MODES] = {
  "<internal>",
  "watchpoint",
  "breakpoint"
};

typedef struct {
   int      mode;
   uint32_t addr;
   uint32_t mask;
} breakpoint_t;

// Watches/Breakpoints addresses etc, stored sorted
static breakpoint_t   exec_breakpoints[MAXBKPTS + 1];
static breakpoint_t mem_rd_breakpoints[MAXBKPTS + 1];
static breakpoint_t mem_wr_breakpoints[MAXBKPTS + 1];
static breakpoint_t io_rd_breakpoints[MAXBKPTS + 1];
static breakpoint_t io_wr_breakpoints[MAXBKPTS + 1];

static void doCmdBreak(const char *params);
static void doCmdBreakIn(const char *params);
static void doCmdBreakOut(const char *params);
static void doCmdBreakRd(const char *params);
static void doCmdBreakWr(const char *params);
static void doCmdClear(const char *params);
static void doCmdContinue(const char *params);
static void doCmdCrc(const char *params);
static void doCmdDis(const char *params);
static void doCmdFill(const char *params);
static void doCmdHelp(const char *params);
static void doCmdIn(const char *params);
static void doCmdList(const char *params);
static void doCmdMem(const char *params);
static void doCmdNext(const char *params);
static void doCmdOut(const char *params);
static void doCmdRd(const char *params);
static void doCmdRegs(const char *params);
static void doCmdStep(const char *params);
static void doCmdTrace(const char *params);
static void doCmdTraps(const char *params);
static void doCmdWatch(const char *params);
static void doCmdWatchIn(const char *params);
static void doCmdWatchOut(const char *params);
static void doCmdWatchRd(const char *params);
static void doCmdWatchWr(const char *params);
static void doCmdWr(const char *params);

// The command process accepts abbreviated forms, for example
// if h is entered, then help will match.

// Must be kept in step with dbgCmdFuncs (just below)
static char *dbgCmdStrings[NUM_CMDS + NUM_IO_CMDS] = {
  "help",
  "continue",
  "step",
  "next",
  "regs",
  "traps",
  "dis",
  "fill",
  "crc",
  "mem",
  "rd",
  "wr",
  "trace",
  "clear",
  "list",
  "breakx",
  "watchx",
  "breakr",
  "watchr",
  "breakw",
  "watchw",
  "in",
  "out",
  "breaki",
  "watchi",
  "breako",
  "watcho",
};

// Must be kept in step with dbgCmdStrings (just above)
static void (*dbgCmdFuncs[NUM_CMDS + NUM_IO_CMDS])(const char *params) = {
  doCmdHelp,
  doCmdContinue,
  doCmdStep,
  doCmdNext,
  doCmdRegs,
  doCmdTraps,
  doCmdDis,
  doCmdFill,
  doCmdCrc,
  doCmdMem,
  doCmdRd,
  doCmdWr,
  doCmdTrace,
  doCmdClear,
  doCmdList,
  doCmdBreak,
  doCmdWatch,
  doCmdBreakRd,
  doCmdWatchRd,
  doCmdBreakWr,
  doCmdWatchWr,
  doCmdIn,
  doCmdOut,
  doCmdBreakIn,
  doCmdWatchIn,
  doCmdBreakOut,
  doCmdWatchOut
};


/********************************************************
 * Other global variables
 ********************************************************/

static char strbuf[1000];

//static char cmd[1000];

static volatile int stopped = 0;

// When single stepping, trace (i.e. log) event N instructions
// Setting this to 0 will disable logging
static int tracing = 1;

static int trace_counter = 0;

static int stepping = 0;

static int step_counter;

static uint32_t break_next_addr = BN_DISABLED;

static uint32_t next_addr;

// The current memory address (e.g. used when disassembling)
static unsigned int memAddr = 0;

cpu_debug_t *getCpu() {
   return cpu_debug_list[copro];
}

static int internal = 0;

static uint32_t memread(cpu_debug_t *cpu, uint32_t addr) {
   internal = 1;
   uint32_t result = cpu->memread(addr);
   internal = 0;
   return result;
}

static void memwrite(cpu_debug_t *cpu, uint32_t addr, uint32_t value) {
   internal = 1;
   cpu->memwrite(addr, value);
   internal = 0;
}

static uint32_t ioread(cpu_debug_t *cpu, uint32_t addr) {
   internal = 1;
   uint32_t result = cpu->ioread(addr);
   internal = 0;
   return result;
}

static void iowrite(cpu_debug_t *cpu, uint32_t addr, uint32_t value) {
   internal = 1;
   cpu->iowrite(addr, value);
   internal = 0;
}

static char *format_hex(const int width, uint32_t i) {
   static char result[10];
   if (width == WIDTH_32BITS) {
      sprintf(result, "%08"PRIx32, i);
   } else if (width == WIDTH_16BITS) {
      sprintf(result, "%04"PRIx32, i);
   } else {
      sprintf(result, "%02"PRIx32, i);
   }
   return result;
}

/********************************************************
 * Hooks from CPU Emulation
 ********************************************************/

static void noprompt() {
   RPI_AuxMiniUartWrite(13);
}

static void prompt() {
   int i=0;
   while ( prompt_str[i] )  {
      RPI_AuxMiniUartWrite(prompt_str[i++]);
   }
}

static void cpu_stop() {
//   if (!stopped) {
//      printf("Stopped\r\n");
//   }
   stopped = 1;
}

static void cpu_continue() {
//   if (stopped) {
//      printf("Running\r\n");
//   }
   stopped = 0;
}

static void disassemble_addr(uint32_t addr) {
   cpu_debug_t *cpu = getCpu();
   internal = 1;
   next_addr = cpu->disassemble(addr, strbuf, sizeof(strbuf));
   internal = 0;
   noprompt();
   printf("%s\r\n", &strbuf[0]);
   prompt();
}


static inline breakpoint_t *check_for_breakpoints(uint32_t addr, breakpoint_t *ptr) {
   while (ptr->mode != MODE_LAST) {
      if ((addr & ptr->mask) == ptr->addr) {
         if (ptr->mode == MODE_BREAK) {
            cpu_stop();
         }
         return ptr;
      }
      ptr++;
   }
   return NULL;
}

// TODO: size should not be ignored!

static inline void generic_memory_access(cpu_debug_t *cpu, uint32_t addr, uint32_t value, uint8_t size,
                                         const char *type, breakpoint_t *list) {
   breakpoint_t *ptr = check_for_breakpoints(addr, list);
   if (ptr) {
      uint32_t pc = cpu->get_instr_addr();
      if (ptr->mode == MODE_BREAK) {
         noprompt();
         printf("%s breakpoint hit at %"PRIx32" : %"PRIx32" = %"PRIx32"\r\n", type, pc, addr, value);
         prompt();
         disassemble_addr(pc);
      } else {
         noprompt();
         printf("%s watchpoint hit at %"PRIx32" : %"PRIx32" = %"PRIx32"\r\n", type, pc, addr, value);
         prompt();
      }
      while (stopped);
   }
}

void debug_memread (cpu_debug_t *cpu, uint32_t addr, uint32_t value, uint8_t size) {
   if (!internal) {
      generic_memory_access(cpu, addr, value, size, "Mem Rd", mem_rd_breakpoints);
   }
};

void debug_memwrite(cpu_debug_t *cpu, uint32_t addr, uint32_t value, uint8_t size) {
   if (!internal) {
      generic_memory_access(cpu, addr, value, size, "Mem Wr", mem_wr_breakpoints);
   }
};

void debug_ioread (cpu_debug_t *cpu, uint32_t addr, uint32_t value, uint8_t size) {
   if (!internal) {
      generic_memory_access(cpu, addr, value, size, "IO Rd", io_rd_breakpoints);
   }
};

void debug_iowrite(cpu_debug_t *cpu, uint32_t addr, uint32_t value, uint8_t size) {
   if (!internal) {
      generic_memory_access(cpu, addr, value, size, "IO Wr", io_wr_breakpoints);
   }
};

void debug_preexec (cpu_debug_t *cpu, uint32_t addr) {
   int show = 0;

   if (addr == break_next_addr) {
      break_next_addr = BN_DISABLED;
      cpu_stop();
      show = 1;

   } else {
      breakpoint_t *ptr = check_for_breakpoints(addr, exec_breakpoints);

      if (ptr) {
         if (ptr->mode == MODE_BREAK) {
            noprompt();
            printf("Exec breakpoint hit at %"PRIx32"\r\n", addr);
            prompt();
            show = 1;

         } else {
            noprompt();
            printf("Exec watchpoint hit at %"PRIx32"\r\n", addr);
            prompt();
         }
      }

      if (stepping) {
         if (tracing) {
            if (++trace_counter == tracing) {
               trace_counter = 0;
               show = 1;
            }
         }
         if (++step_counter == stepping) {
            step_counter = 0;
            cpu_stop();
            show = 1;
         }
      }
   }

   if (show) {
      disassemble_addr(addr);
   }
   while (stopped);
};

void debug_trap(const cpu_debug_t *cpu, uint32_t addr, int reason) {
   const char *desc = cpu->trap_names[reason];
   noprompt();
   printf("Trap: %s at %"PRIx32"\r\n", desc, addr);
   prompt();
}

/*******************************************
 * Helpers
 *******************************************/

// Set the breakpoint state variables
void setBreakpoint(breakpoint_t *ptr, char *type, unsigned int addr, unsigned int mask, unsigned int mode) {
   printf("%s %s set at %x\r\n", type, modeStrings[mode], addr);
   ptr->addr = addr & mask;
   ptr->mask = mask;
   ptr->mode = mode;
}

void copyBreakpoint(breakpoint_t *ptr1, const breakpoint_t *ptr2) {
   ptr1->addr = ptr2->addr;
   ptr1->mask = ptr2->mask;
   ptr1->mode = ptr2->mode;
}

// A generic helper that does most of the work of the watch/breakpoint commands
void genericBreakpoint(const char *params, char *type, breakpoint_t *list, unsigned int mode) {
   int i = 0;
   unsigned int addr;
   unsigned int mask = 0xFFFFFFFF;
   sscanf(params, "%x %x", &addr, &mask);

   while (list[i].mode != MODE_LAST) {
      if (list[i].addr == addr) {
         setBreakpoint(list + i, type, addr, mask, mode);
         return;
      }
      i++;
   }

  if (i == MAXBKPTS) {
     printf("All %d %s breakpoints are already set\r\n", i, type);
     return;
  }
  // Extending the list, so add a new end marker
  list[i + 1].mode = MODE_LAST;
  while (i >= 0) {
     if (i == 0 || list[i - 1].addr < addr) {
        setBreakpoint(list + i, type, addr, mask, mode);
        return;
     } else {
        copyBreakpoint(list + i, list + i - 1);
     }
     i--;
  }
}

/*******************************************
 * User Commands
 *******************************************/

static void doCmdHelp(const char *params) {
   cpu_debug_t *cpu = getCpu();
   int i;
   printf("PiTubeDirect debugger\r\n");
   printf("    cpu = %s\r\n", cpu->cpu_name);
   printf("Commands:\r\n");
   int n = NUM_CMDS;
   if (HAS_IO) {
      n += NUM_IO_CMDS;
   }
   for (i = 0; i < n; i++) {
      printf("    %s\r\n", dbgCmdStrings[i]);
   }
}

static void doCmdRegs(const char *params) {
   cpu_debug_t *cpu = getCpu();
   const char **reg = cpu->reg_names;
   char name[100];
   char value[100];
   int num_params = sscanf(params, "%99s %99s", name, value);
   if (num_params > 0) {
      int i = 0;
      while (*reg) {
         if (strcasecmp(name, *reg) == 0) {
            if (num_params == 2) {
               // Write the register
               cpu->reg_parse(i, value);
            }
            // Read the register
            cpu->reg_print(i, strbuf, sizeof(strbuf));
            printf("%8s = %s\r\n", *reg, &strbuf[0]);
            return;
         }
         reg++;
         i++;
      }
      printf("Register %s does not exist in the %s\r\n", name, cpu->cpu_name);         
   } else {
      int i = 0;
      while (*reg) {
         cpu->reg_print(i, strbuf, sizeof(strbuf));
         printf("%8s = %s\r\n", *reg, &strbuf[0]);
         reg++;
         i++;
      }
   }
}

static void doCmdTraps(const char *params) {
   cpu_debug_t *cpu = getCpu();
   const char **trap = cpu->trap_names;
   if (*trap) {
      while (*trap) {
         printf("%s\r\n", *trap);
         trap++;
      }
   } else {
      printf("No traps implemented in %s\r\n", cpu->cpu_name);
   }
}

static void doCmdDis(const char *params) {
   cpu_debug_t *cpu = getCpu();
   int i;
   sscanf(params, "%x", &memAddr);
   for (i = 0; i < 10; i++) { 
      internal = 1;
      memAddr = cpu->disassemble(memAddr, strbuf, sizeof(strbuf));
      internal = 0;
      printf("%s\r\n", &strbuf[0]);
   }
}

static void doCmdFill(const char *params) {
   cpu_debug_t *cpu = getCpu();
   unsigned int i;
   unsigned int start;
   unsigned int end;
   unsigned int data;
   sscanf(params, "%x %x %x", &start, &end, &data);

   printf("Wr: %x to %x = %s\r\n", start, end, format_hex(cpu->mem_width, data));
   for (i = start; i <= end; i++) {
      memwrite(cpu, i, data);
   }
}

static void doCmdCrc(const char *params) {
   cpu_debug_t *cpu = getCpu();
   unsigned int i;
   unsigned int j;
   unsigned int start;
   unsigned int end;
   unsigned int data;
   unsigned int crc = 0;
   sscanf(params, "%x %x", &start, &end);
   for (i = start; i <= end; i++) {
      data = memread(cpu, i);
      for (j = 0; j < 8; j++) {
         crc = crc << 1;
         crc = crc | (data & 1);
         data >>= 1;
         if (crc & 0x10000)
            crc = (crc ^ CRC_POLY) & 0xFFFF;
      }
   }
   printf("crc: %04x\r\n", crc);
}

static void doCmdMem(const char *params) {
   cpu_debug_t *cpu = getCpu();
   int i, j;
   unsigned int row[16];
   sscanf(params, "%x", &memAddr);
   for (i = 0; i < 0x100; i+= 16) {
      for (j = 0; j < 16; j++) {
         row[j] = memread(cpu, memAddr + i + j);
      }
      printf("%04x ", memAddr + i);
      for (j = 0; j < 16; j++) {
         printf("%s ", format_hex(cpu->mem_width, row[j]));
      }
      printf(" ");
      for (j = 0; j < 16; j++) {
         unsigned int c = row[j];
         if (c < 32 || c > 126) {
            c = '.';
         }
         printf("%c", c);
      }
      printf("\r\n");
   }
   memAddr += 0x100;
}

static void doCmdRd(const char *params) {
   cpu_debug_t *cpu = getCpu();
   unsigned int addr;
   unsigned int data;
   sscanf(params, "%x", &addr);
   data = memread(cpu, addr);
   printf("Rd Mem: %x = %s\r\n", addr, format_hex(cpu->mem_width, data));
}

static void doCmdWr(const char *params) {
   cpu_debug_t *cpu = getCpu();
   unsigned int addr;
   unsigned int data;
   sscanf(params, "%x %x", &addr, &data);
   printf("Wr Mem: %x = %s\r\n", addr, format_hex(cpu->mem_width, data));
   memwrite(cpu, addr++, data);
}

static void doCmdIn(const char *params) {
   cpu_debug_t *cpu = getCpu();
   unsigned int addr;
   unsigned int data;
   sscanf(params, "%x", &addr);
   data = ioread(cpu, addr);
   printf("Rd IO: %x = %s\r\n", addr, format_hex(cpu->io_width, data));
}

static void doCmdOut(const char *params) {
   cpu_debug_t *cpu = getCpu();
   unsigned int addr;
   unsigned int data;
   sscanf(params, "%x %x", &addr, &data);
   printf("Wr IO: %x = %s\r\n", addr, format_hex(cpu->io_width, data));
   iowrite(cpu, addr++, data);
}


static void doCmdStep(const char *params) {
  int i = 1;
  sscanf(params, "%d", &i);
  if (i <= 0) {
    printf("Number of instuctions must be positive\r\n");
    return;
  }
  stepping = i;
  step_counter = 0;
  printf("Stepping %d instructions\r\n", i);
  cpu_continue();
}

static void doCmdNext(const char *params) {
   stepping = 0;
   break_next_addr = next_addr; 
   printf("Skipping to %"PRIx32"\r\n", next_addr);
   cpu_continue();
}

static void doCmdTrace(const char *params) {
  int i = 1;
  sscanf(params, "%d", &i);
  if (i < 0) {
    printf("Number of instuctions must be positive or zero\r\n");
    return;
  }
  tracing = i;
  trace_counter = 0;
  if (tracing) {
     printf("Tracing every %d instructions\r\n", tracing);
  } else {
     printf("Tracing disabled\r\n");
  }
}

static void genericList(const char *type, const breakpoint_t *list) {
   int i = 0;
   printf("%s\r\n", type);
   while (list[i].mode != MODE_LAST) {
      printf("    addr:%"PRIx32"; mask:%"PRIx32"; type:%s\r\n", list[i].addr, list[i].mask, modeStrings[list[i].mode]);
      i++;
   }
   if (i == 0) {
      printf("    none\r\n");
   }
}

static void doCmdList(const char *params) {
   if (break_next_addr != BN_DISABLED) {
      printf("Transient\r\n    addr:%"PRIx32"\r\n", break_next_addr);
   }
   genericList("Exec", exec_breakpoints);
   genericList("Mem Rd", mem_rd_breakpoints);
   genericList("Mem Wr", mem_wr_breakpoints);
   if (HAS_IO) {
      genericList("IO Rd", io_rd_breakpoints);
      genericList("IO Wr", io_wr_breakpoints);
   }
}

static void doCmdBreak(const char *params) {
   genericBreakpoint(params, "Exec", exec_breakpoints, MODE_BREAK);
}

static void doCmdWatch(const char *params) {
   genericBreakpoint(params, "Exec", exec_breakpoints, MODE_WATCH);
}

static void doCmdBreakRd(const char *params) {
   genericBreakpoint(params, "Mem Rd", mem_rd_breakpoints, MODE_BREAK);
}

static void doCmdWatchRd(const char *params) {
   genericBreakpoint(params, "Mem Rd", mem_rd_breakpoints, MODE_WATCH);
}

static void doCmdBreakWr(const char *params) {
   genericBreakpoint(params, "Mem Wr", mem_wr_breakpoints, MODE_BREAK);
}

static void doCmdWatchWr(const char *params) {
   genericBreakpoint(params, "Mem Wr", mem_wr_breakpoints, MODE_WATCH);
}

static void doCmdBreakIn(const char *params) {
   genericBreakpoint(params, "IO Rd", io_rd_breakpoints, MODE_BREAK);
}

static void doCmdWatchIn(const char *params) {
   genericBreakpoint(params, "IO Rd", io_rd_breakpoints, MODE_WATCH);
}

static void doCmdBreakOut(const char *params) {
   genericBreakpoint(params, "IO Wr", io_wr_breakpoints, MODE_BREAK);
}

static void doCmdWatchOut(const char *params) {
   genericBreakpoint(params, "IO Wr", io_wr_breakpoints, MODE_WATCH);
}


int genericClear(uint32_t addr, char *type, breakpoint_t *list) {

   int i = 0;

   // Assume addr is an address, and try to map to an index
   while (list[i].mode != MODE_LAST) {
      if (list[i].addr == addr) {
         break;
      }
      i++;
   }
   if (list[i].mode == MODE_LAST) {
      if (addr < i) {
         i = addr;
      } else {
         i = -1;
      }
   }
   if (i >= 0) {
      printf("Removed %s breakpoint at %"PRIx32"\r\n", type, list[i].addr);
      do {
         copyBreakpoint(list + i, list + i + 1);
         i++;
      } while (list[i - 1].mode != MODE_LAST);
      return 1;
   }
   return 0;
}


static void doCmdClear(const char *params) {
   int found = 0;
   unsigned int addr = 0;

   sscanf(params, "%x", &addr);
   if (break_next_addr == addr) {
      break_next_addr = BN_DISABLED;
      found = 1;
   }
   found |= genericClear(addr, "Exec", exec_breakpoints);
   found |= genericClear(addr, "Mem Rd", mem_rd_breakpoints);
   found |= genericClear(addr, "Mem Wr", mem_wr_breakpoints);
   if (HAS_IO) {
      found |= genericClear(addr, "IO Rd", io_rd_breakpoints);
      found |= genericClear(addr, "IO Wr", io_wr_breakpoints);
   }
   if (!found) {
      printf("No breakpoints set at %x\r\n", addr);
   }
}

static void doCmdContinue(const char *params) {
   stepping = 0;
   printf("Running\r\n");
   cpu_continue();
}


static void updateDebugFlag() {
   cpu_debug_t *cpu = getCpu();
   int enable = 0;
   if (stepping) {
      enable = 1;
   }
   if (break_next_addr != BN_DISABLED) {
      enable = 1;
   }
   if (exec_breakpoints[0].mode != MODE_LAST) {
      enable = 1;
   }
   if (mem_rd_breakpoints[0].mode != MODE_LAST) {
      enable = 1;
   }
   if (mem_wr_breakpoints[0].mode != MODE_LAST) {
      enable = 1;
   }
   if (HAS_IO) {
      if (io_rd_breakpoints[0].mode != MODE_LAST) {
         enable = 1;
      }
      if (io_wr_breakpoints[0].mode != MODE_LAST) {
         enable = 1;
      }
   }
   if (cpu->debug_enable(enable) != enable) {
      printf("cpu: %s debug enable = %d\r\n", cpu->cpu_name, enable);
   }
}

/********************************************************
 * User Command Processor
 ********************************************************/

static void dispatchCmd(char *cmd) {
   int i;
   char *cmdString;
   int minLen;
   int cmdStringLen;
   int cmdLen = 0;
   while (cmd[cmdLen] >= 'a' && cmd[cmdLen] <= 'z') {
      cmdLen++;
   }
   int n = NUM_CMDS;
   if (HAS_IO) {
      n += NUM_IO_CMDS;
   }
   for (i = 0; i < n; i++) {
      cmdString = dbgCmdStrings[i];
      cmdStringLen = strlen(cmdString);
      minLen = cmdLen < cmdStringLen ? cmdLen : cmdStringLen;
      if (strncmp(cmdString, cmd, minLen) == 0) {
         cpu_debug_t *cpu = getCpu();
         if (cpu == NULL) {
            printf("No debugger available for this co pro\r\n");
         } else {
            (*dbgCmdFuncs[i])(cmd + cmdLen);
            updateDebugFlag();
         }
         return;
      }
   }
   printf("Unknown command %s\r\n", cmd);
}

/********************************************************
 * External interface
 ********************************************************/


#ifdef USE_LINENOISE

void debugger_rx_char(char c) {
   char *buf = linenoise_async_rxchar(c, prompt_str);
   if (buf) {
      printf("\r\n");
      if (buf[0]) {
         dispatchCmd(buf);
      }
      prompt();
   }
}

#else

void debugger_rx_char(char c) {
   static int i = 0;
   if (c == 8) {
      // Handle backspace/delete
      if (i > 0) {
         i--;
         RPI_AuxMiniUartWrite(c);
         RPI_AuxMiniUartWrite(32);
         RPI_AuxMiniUartWrite(c);
      }
   } else if (c == 13) {
      // Handle return
      if (i == 0) {		
         while (cmd[i]) {		
            RPI_AuxMiniUartWrite(cmd[i++]);		
         }		
      } else {		
         cmd[i] = 0;		
      }
      RPI_AuxMiniUartWrite(10);
      RPI_AuxMiniUartWrite(13);
      dispatchCmd(cmd);
      prompt();
      i = 0;
   } else if (c >= 32) {
      // Handle any other non-control character
      RPI_AuxMiniUartWrite(c);
      cmd[i++] = c;
   }
}

#endif
