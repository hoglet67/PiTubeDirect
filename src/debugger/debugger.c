#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "debugger.h"
#include "../rpi-aux.h"
#include "../cpu_debug.h"

#define NUM_CMDS 19

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

static void doCmdBreakI(char *params);
static void doCmdBreakRdMem(char *params);
static void doCmdBreakWrMem(char *params);
static void doCmdClear(char *params);
static void doCmdContinue(char *params);
static void doCmdCrc(char *params);
static void doCmdDis(char *params);
static void doCmdFill(char *params);
static void doCmdHelp(char *params);
static void doCmdList(char *params);
static void doCmdMem(char *params);
static void doCmdReadMem(char *params);
static void doCmdRegs(char *params);
static void doCmdStep(char *params);
static void doCmdTrace(char *params);
static void doCmdWatchI(char *params);
static void doCmdWatchRdMem(char *params);
static void doCmdWatchWrMem(char *params);
static void doCmdWriteMem(char *params);


// The command process accepts abbreviated forms, for example
// if h is entered, then help will match.

// Must be kept in step with dbgCmdFuncs (just below)
static char *dbgCmdStrings[NUM_CMDS] = {
  "help",
  "continue",
  "step",
  "regs",
  "dis",
  "fill",
  "crc",
  "mem",
  "rdm",
  "wrm",
  "trace",
  "blist",
  "breakx",
  "watchx",
  "breakrm",
  "watchrm",
  "breakwm",
  "watchwm",
  "clear"
};

// Must be kept in step with dbgCmdStrings (just above)
static void (*dbgCmdFuncs[NUM_CMDS])(char *params) = {
  doCmdHelp,
  doCmdContinue,
  doCmdStep,
  doCmdRegs,
  doCmdDis,
  doCmdFill,
  doCmdCrc,
  doCmdMem,
  doCmdReadMem,
  doCmdWriteMem,
  doCmdTrace,
  doCmdList,
  doCmdBreakI,
  doCmdWatchI,
  doCmdBreakRdMem,
  doCmdWatchRdMem,
  doCmdBreakWrMem,
  doCmdWatchWrMem,
  doCmdClear
};


/********************************************************
 * Other global variables
 ********************************************************/

static char strbuf[1000];

static char cmd[1000];

static volatile int stopped = 0;

// When single stepping, trace (i.e. log) event N instructions
// Setting this to 0 will disable logging
static int tracing = 1;

static int trace_counter = 0;

static int stepping = 0;

static int step_counter;

// The current memory address (e.g. used when disassembling)
static unsigned int memAddr = 0;


// TODO - Fix hardcoded implementation
//extern cpu_debug_t n32016_cpu_debug;
//cpu_debug_t *getCpu() {
//   return &n32016_cpu_debug;
//}

extern cpu_debug_t arm2_cpu_debug;
cpu_debug_t *getCpu() {
   return &arm2_cpu_debug;
}

/********************************************************
 * Hooks from CPU Emulation
 ********************************************************/

static void noprompt() {
   RPI_AuxMiniUartWrite(13);
}

static void prompt() {
   RPI_AuxMiniUartWrite('>');
   RPI_AuxMiniUartWrite('>');
   RPI_AuxMiniUartWrite(' ');
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
   cpu->disassemble(addr, strbuf, sizeof(strbuf));
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
      prompt();
   }
}

void debug_memread (cpu_debug_t *cpu, uint32_t addr, uint32_t value, uint8_t size) {
   generic_memory_access(cpu, addr, value, size, "Mem Rd", mem_rd_breakpoints);
};

void debug_memwrite(cpu_debug_t *cpu, uint32_t addr, uint32_t value, uint8_t size) {
   generic_memory_access(cpu, addr, value, size, "Mem Wr", mem_wr_breakpoints);
};

void debug_preexec (cpu_debug_t *cpu, uint32_t addr) {
   int show = 0;

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
   if (show) {
      disassemble_addr(addr);
   }
   while (stopped);
};

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

void copyBreakpoint(breakpoint_t *ptr1, breakpoint_t *ptr2) {
   ptr1->addr = ptr2->addr;
   ptr1->mask = ptr2->mask;
   ptr1->mode = ptr2->mode;
}

// A generic helper that does most of the work of the watch/breakpoint commands
void genericBreakpoint(char *params, char *type, breakpoint_t *list, unsigned int mode) {
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

static void doCmdHelp(char *params) {
   cpu_debug_t *cpu = getCpu();
   int i;
   printf("PiTubeDirect debugger\r\n");
   printf("    cpu = %s\r\n", cpu->cpu_name);
   printf("Commands:\r\n");
   for (i = 0; i < NUM_CMDS; i++) {
      printf("    %s\r\n", dbgCmdStrings[i]);
   }
}

static void doCmdRegs(char *params) {
   cpu_debug_t *cpu = getCpu();
   const char **regs = cpu->reg_names;
   int i = 0;
   while (*regs) {
      cpu->reg_print(i, strbuf, sizeof(strbuf));
      printf("%8s = %s\r\n", *regs, &strbuf[0]);
      regs++;
      i++;
   }
}

static void doCmdDis(char *params) {
   cpu_debug_t *cpu = getCpu();
   int i;
   sscanf(params, "%x", &memAddr);
   for (i = 0; i < 10; i++) {
      memAddr = cpu->disassemble(memAddr, strbuf, sizeof(strbuf));
      printf("%s\r\n", &strbuf[0]);
   }
}

static void doCmdFill(char *params) {
   cpu_debug_t *cpu = getCpu();
   unsigned int i;
   unsigned int start;
   unsigned int end;
   unsigned int data;
   sscanf(params, "%x %x %x", &start, &end, &data);

   printf("Wr: %x to %x = %02x\r\n", start, end, data);
   for (i = start; i <= end; i++) {
      cpu->memwrite(i, data);
   }
}

static void doCmdCrc(char *params) {
   cpu_debug_t *cpu = getCpu();
   unsigned int i;
   unsigned int j;
   unsigned int start;
   unsigned int end;
   unsigned int data;
   unsigned int crc = 0;
   sscanf(params, "%x %x", &start, &end);
   for (i = start; i <= end; i++) {
      data = cpu->memread(i);
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

static void doCmdMem(char *params) {
   cpu_debug_t *cpu = getCpu();
   int i, j;
   unsigned int row[16];
   sscanf(params, "%x", &memAddr);
   for (i = 0; i < 0x100; i+= 16) {
      for (j = 0; j < 16; j++) {
         row[j] = cpu->memread(memAddr + i + j);
      }
      printf("%04x ", memAddr + i);
      for (j = 0; j < 16; j++) {
         printf("%02x ", row[j]);
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

static void doCmdReadMem(char *params) {
   cpu_debug_t *cpu = getCpu();
   unsigned int addr;
   unsigned int data;
   sscanf(params, "%x", &addr);
   data = cpu->memread(addr);
   printf("Rd: %x = %02x\r\n", addr, data);
}

static void doCmdWriteMem(char *params) {
   cpu_debug_t *cpu = getCpu();
   unsigned int addr;
   unsigned int data;
   sscanf(params, "%x %x", &addr, &data);
   printf("Wr: %x = %02x\r\n", addr, data);
   cpu->memwrite(addr++, data);
}


static void doCmdStep(char *params) {
  int i = 1;
  sscanf(params, "%d", &i);
  if (i <= 0) {
    printf("Number of instuctions must be positive\r\n");
    return;
  }
  stepping = i;
  step_counter = 0;
  if (stepping) {
     printf("Stepping %d instructions\r\n", i);
  } else {
     printf("Stepping disabled\r\n");
  }
  cpu_continue();
}

static void doCmdTrace(char *params) {
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

static void genericList(char *type, breakpoint_t *list) {
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

static void doCmdList(char *params) {
  genericList("Exec", exec_breakpoints);
  genericList("Mem Rd", mem_rd_breakpoints);
  genericList("Mem Wr", mem_wr_breakpoints);
}

static void doCmdBreakI(char *params) {
   genericBreakpoint(params, "Exec", exec_breakpoints, MODE_BREAK);
}

static void doCmdWatchI(char *params) {
   genericBreakpoint(params, "Exec", exec_breakpoints, MODE_WATCH);
}

static void doCmdBreakRdMem(char *params) {
   genericBreakpoint(params, "Mem Rd", mem_rd_breakpoints, MODE_BREAK);
}

static void doCmdWatchRdMem(char *params) {
   genericBreakpoint(params, "Mem Rd", mem_rd_breakpoints, MODE_WATCH);
}

static void doCmdBreakWrMem(char *params) {
   genericBreakpoint(params, "Mem Wr", mem_wr_breakpoints, MODE_BREAK);
}

static void doCmdWatchWrMem(char *params) {
   genericBreakpoint(params, "Mem Wr", mem_wr_breakpoints, MODE_WATCH);
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


static void doCmdClear(char *params) {
   int found = 0;
   unsigned int addr = 0;

   sscanf(params, "%x", &addr);
   found |= genericClear(addr, "Exec", exec_breakpoints);
   found |= genericClear(addr, "Mem Rd", mem_rd_breakpoints);
   found |= genericClear(addr, "Mem Wr", mem_wr_breakpoints);
   if (!found) {
      printf("No breakpoints set at %x\r\n", addr);
   }
}

static void doCmdContinue(char *params) {
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
   if (exec_breakpoints[0].mode != MODE_LAST) {
      enable = 1;
   }
   if (mem_rd_breakpoints[0].mode != MODE_LAST) {
      enable = 1;
   }
   if (mem_wr_breakpoints[0].mode != MODE_LAST) {
      enable = 1;
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
   for (i = 0; i < NUM_CMDS; i++) {
      cmdString = dbgCmdStrings[i];
      cmdStringLen = strlen(cmdString);
      minLen = cmdLen < cmdStringLen ? cmdLen : cmdStringLen;
      if (strncmp(cmdString, cmd, minLen) == 0) {
         (*dbgCmdFuncs[i])(cmd + cmdLen);
         updateDebugFlag();
         return;
      }
   }
   printf("Unknown command %s\r\n", cmd);
}

/********************************************************
 * External interface
 ********************************************************/

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
