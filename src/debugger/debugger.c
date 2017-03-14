#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "debugger.h"
#include "../rpi-aux.h"
#include "../cpu_debug.h"

#define NUM_CMDS 8

// The Atom CRC Polynomial
#define CRC_POLY          0x002d

static void doCmdRegs(char *params);
static void doCmdDis(char *params);
static void doCmdCrc(char *params);
static void doCmdFill(char *params);
static void doCmdHelp(char *params);
static void doCmdMem(char *params);
static void doCmdReadMem(char *params);
static void doCmdWriteMem(char *params);

// The command process accepts abbreviated forms, for example
// if h is entered, then help will match.

// Must be kept in step with dbgCmdFuncs (just below)
static char *dbgCmdStrings[NUM_CMDS] = {
   "help",
   "regs",
   "dis",
   "fill",
   "crc",
   "mem",
   "rdm",
   "wrm",
};

// Must be kept in step with dbgCmdStrings (just above)
static void (*dbgCmdFuncs[NUM_CMDS])(char *params) = {
   doCmdHelp,
   doCmdRegs,
   doCmdDis,
   doCmdFill,
   doCmdCrc,
   doCmdMem,
   doCmdReadMem,
   doCmdWriteMem,
};


uint32_t debug_memread (cpu_debug_t *cpu, uint32_t addr) {
   return cpu->memread(addr);
};

uint32_t debug_memfetch(cpu_debug_t *cpu, uint32_t addr) {
   if (cpu->memfetch != NULL) {
      return cpu->memfetch(addr);
   } else {
      return cpu->memread(addr);
   }
};

void debug_memwrite(cpu_debug_t *cpu, uint32_t addr, uint32_t value) {
   cpu->memwrite(addr, value);
};

void debug_preexec (cpu_debug_t *cpu, uint32_t addr) {
};

// TODO - Fix hardcoded implementation
extern cpu_debug_t n32016_cpu_debug;
cpu_debug_t *getCpu() {
   return &n32016_cpu_debug;
}

/********************************************************
 * Other global variables
 ********************************************************/

static char strbuf[1000];

static char cmd[1000];

// The current memory address (e.g. used when disassembling)
static unsigned int memAddr = 0;

/*******************************************
 * User Commands
 *******************************************/

static void doCmdHelp(char *params) {
   cpu_debug_t *cpu = getCpu();
   int i;
   printf("PiTubeDirect debugger; cpu = %s\r\n", cpu->cpu_name);
   printf("Commands:\r\n");
   for (i = 0; i < NUM_CMDS; i++) {
      printf("    %s\r\n", dbgCmdStrings[i]);
   }
}

void doCmdRegs(char *params) {
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

void doCmdDis(char *params) {
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
   long i;
   unsigned int start;
   unsigned int end;
   unsigned int data;
   sscanf(params, "%x %x %x", &start, &end, &data);
   
   printf("Wr: %04X to %04X = %02X\r\n", start, end, data);
   for (i = start; i <= end; i++) {
      cpu->memwrite(i, data);
   }
}

static void doCmdCrc(char *params) {
   cpu_debug_t *cpu = getCpu();
   long i;
   int j;
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
   printf("crc: %04X\r\n", crc);
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
      printf("%04X ", memAddr + i);
      for (j = 0; j < 16; j++) {
         printf("%02X ", row[j]);
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

void doCmdReadMem(char *params) {
   cpu_debug_t *cpu = getCpu();
   unsigned int addr;
   unsigned int data;
   sscanf(params, "%x", &addr);
   data = cpu->memread(addr);
   printf("Rd: %04X = %02X\r\n", addr, data);
}

void doCmdWriteMem(char *params) {
   cpu_debug_t *cpu = getCpu();
   unsigned int addr;
   unsigned int data;
   sscanf(params, "%x %x", &addr, &data);
   printf("Wr: %04X = %02X\r\n", addr, data);
   cpu->memwrite(addr++, data);
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
      RPI_AuxMiniUartWrite('>');
      RPI_AuxMiniUartWrite('>');
      RPI_AuxMiniUartWrite(' ');
      i = 0;
   } else if (c >= 32) {
      // Handle any other non-control character
      RPI_AuxMiniUartWrite(c);
      cmd[i++] = c;
   }
}

