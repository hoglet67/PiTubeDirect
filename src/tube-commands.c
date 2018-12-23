#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "swi.h"
#include "tube-swi.h"
#include "tube-commands.h"
#include "darm/darm.h"

const char *help = "ARM Tube Client 0.10\r\n";

char line[256];

/***********************************************************
 * Build in Commands
 ***********************************************************/

#define NUM_CMDS 7

// Must be kept in step with cmdFuncs (just below)
char *cmdStrings[NUM_CMDS] = {
  "HELP",
  "TEST",
  "GO",
  "MEM",
  "DIS",
  "FILL",
  "CRC"
};

int (*cmdFuncs[NUM_CMDS])(const char *params) = {
  doCmdHelp,
  doCmdTest,
  doCmdGo,
  doCmdMem,
  doCmdDis,
  doCmdFill,
  doCmdCrc
};

int cmdMode[NUM_CMDS] = {
  MODE_USER,
  MODE_USER,
  MODE_USER,
  MODE_USER,
  MODE_USER,
  MODE_USER,
  MODE_USER
};

int dispatchCmd(char *cmd) {
  int i;
  int c;
  int r;
  char *cmdPtr;
  char *refPtr;
  //  skip any leading space
  while (isblank((int) *cmd)) {
    cmd++;
  }
  // Match the command 
  for (i = 0; i < NUM_CMDS; i++) {
    cmdPtr = cmd;
    refPtr = cmdStrings[i];
    do {
      c = tolower((int)*cmdPtr);      
      r = tolower((int)*refPtr);
      // a command can be terminated with any non-alpha character
      if ((r == 0 && !isalpha(c)) || c == '.') {
        // skip any trailing space becore the params
        while (isblank((int)*cmdPtr)) {
          cmdPtr++;
        }
        if (cmdMode[i] == MODE_USER) {
          // Execute the command in user mode
          return user_exec_fn(cmdFuncs[i], (int) cmdPtr);
        } else {
          // Execute the command in supervisor mode
          return cmdFuncs[i](cmdPtr);
        }
      }
      cmdPtr++;
      refPtr++;
    } while (c != 0 && c == r);
  }
  // non-zero means pass the command onto the CLI processor
  return 1;
}

int doCmdTest(const char *params) {
  OS_Write0("doCmdTest\r\n");
  return 0;
}

int doCmdHelp(const char *params) {
  int i;
  OS_Write0(help);
  if (strncasecmp(params, "ARM", 3) == 0) {
    for (i = 0; i < NUM_CMDS; i++) {
      OS_Write0("  ");
      OS_Write0(cmdStrings[i]);
      OS_Write0("\r\n");
    }
    return 0;
  }
  // pass the command on to the CLI
  return 1;
}

int doCmdGo(const char *params) {
  unsigned int address;
  FunctionPtr_Type f;
  sscanf(params, "%x", &address);
  // Cast address to a generic function pointer
  f = (FunctionPtr_Type) address;
  f();
  return 0;
}

int doCmdFill(const char *params) {
  unsigned int i;
  unsigned int start;
  unsigned int end;
  unsigned int data;
  sscanf(params, "%x %x %x", &start, &end, &data);
  for (i = start; i <= end; i++) {
    *((unsigned char *)i) = data;
  }
  return 0;
}

int doCmdMem(const char *params) {
  int i, j;
  unsigned char c;
  char *ptr;
  unsigned int flags;
  unsigned int memAddr;
  sscanf(params, "%x", &memAddr);
  do {
    for (i = 0; i < 16; i++) {
      ptr = line;
      // Generate the address
      ptr += sprintf(ptr, "%08X ", memAddr);    
      // Generate the hex values
      for (j = 0; j < 16; j++) {
        c = *((unsigned char *)(memAddr + j));
        ptr += sprintf(ptr, "%02X ", c);
      }
      // Generate the ascii values
      for (j = 0; j < 16; j++) {
        c = *((unsigned char *)(memAddr + j));
        if (c < 32 || c > 126) {
          c = '.';
        }
        ptr += sprintf(ptr, "%c", c);
      }
      ptr += sprintf(ptr, "\r\n");
      OS_Write0(line);
      memAddr += 0x10;
    }
    OS_ReadC(&flags);   
  } while ((flags & CARRY_MASK) == 0);
  OS_Byte(0x7e, 0x00, 0x00, NULL, NULL);
  return 0;
}

int doCmdDis(const char *params) {
  darm_t d;
  darm_str_t str;
  int i;
  unsigned int flags;
  unsigned int opcode;
  unsigned int memAddr;
  sscanf(params, "%x", &memAddr);
  memAddr &= ~3;
  do {
    for (i = 0; i < 16; i++) {
      opcode = *(unsigned int *)memAddr;
      sprintf(line, "%08X %08X ***\r\n", memAddr, opcode);
      if(darm_armv7_disasm(&d, opcode) == 0) {
         d.addr = memAddr;
         d.addr_mask = 0xFFFFFFC;
         if (darm_str2(&d, &str, 0) == 0) {
            sprintf(line + 18, "%s\r\n", str.total);
         }
      }
      OS_Write0(line);
      memAddr += 4;
    }
    OS_ReadC(&flags);   
  } while ((flags & CARRY_MASK) == 0);
  OS_Byte(0x7e, 0x00, 0x00, NULL, NULL);
  return 0;
}

int doCmdCrc(const char *params) {
  unsigned int i;
  unsigned int j;
  unsigned int start;
  unsigned int end;
  unsigned int data;
  unsigned long crc = 0;
  sscanf(params, "%x %x %x", &start, &end, &data);
  for (i = start; i <= end; i++) {
    data = *((unsigned char *)i);
    for (j = 0; j < 8; j++) {
      crc = crc << 1;
      crc = crc | (data & 1);
      data >>= 1;
      if (crc & 0x10000)
        crc = (crc ^ CRC_POLY) & 0xFFFF;
    }
  }
  sprintf(line, "%04X\r\n", (unsigned short)crc);
  OS_Write0(line);
  return 0;
}
