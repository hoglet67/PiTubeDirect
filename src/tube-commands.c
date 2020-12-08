#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "swi.h"
#include "tube-swi.h"
#include "tube-client.h"
#include "tube-commands.h"
#include "darm/darm.h"
#include "tube-defs.h"
#include "gitversion.h"
#include "copro-defs.h"
#include "utils.h"

// Include ARM Basic
#include "armbasic.h"

const char *help = "Native ARM Tube Client ("RELEASENAME"/"GITVERSION")\r\n";

const char *help_key = "ARM";

const char *copro_key = "COPROS";

char line[256];

/***********************************************************
 * Build in Commands
 ***********************************************************/

#define NUM_CMDS 8

// Must be kept in step with cmdFuncs (just below)
char *cmdStrings[NUM_CMDS] = {
  "HELP",
  "TEST",
  "GO",
  "MEM",
  "DIS",
  "FILL",
  "CRC",
  "ARMBASIC"
};

int (*cmdFuncs[NUM_CMDS])(const char *params) = {
  doCmdHelp,
  doCmdTest,
  doCmdGo,
  doCmdMem,
  doCmdDis,
  doCmdFill,
  doCmdCrc,
  doCmdArmBasic
};

int cmdMode[NUM_CMDS] = {
  MODE_USER,
  MODE_USER,
  MODE_USER,
  MODE_USER,
  MODE_USER,
  MODE_USER,
  MODE_USER,
  MODE_USER
};

const char *matchCommand(const char *cmdPtr, const char *refPtr, int minLen) {
  int c;
  int r;
  int index = 0;
  do {
    c = tolower((int)*cmdPtr);
    r = tolower((int)*refPtr);
    // a command can be terminated with any non-alpha character
    if ((r == 0 && !isalpha(c)) || (c == '.' && index >= minLen)) {
      // if the terminator was a . then skip over it
      if (r != 0 && c == '.') {
        cmdPtr++;
      }
      // skip any trailing space becore the params
      while (isblank((int)*cmdPtr)) {
        cmdPtr++;
      }
      return cmdPtr;
    }
    cmdPtr++;
    refPtr++;
    index++;
  } while (c != 0 && c == r);
  return NULL;
}

int dispatchCmd(char *cmd) {
  //  skip any leading space
  while (isblank((int) *cmd)) {
    cmd++;
  }
  // Match the command
  for (int i = 0; i < NUM_CMDS; i++) {
    const char *paramPtr = matchCommand(cmd, cmdStrings[i], 1);
    if (paramPtr) {
      if (cmdMode[i] == MODE_USER) {
        // Execute the command in user mode
        return user_exec_fn(cmdFuncs[i], (int) paramPtr);
      } else {
        // Execute the command in supervisor mode
        return cmdFuncs[i](paramPtr);
      }
    }
  }
  // non-zero means pass the command onto the CLI processor
  return 1;
}

int doCmdTest(const char *params) {
  OS_Write0("doCmdTest\r\n");
  return 0;
}

int doCmdHelp(const char *params) {
  char buff[10];
  if (*params == 0x00 || *params == 0x0a || *params == 0x0d) {
    // *HELP without any parameters
    OS_Write0(help);
    OS_Write0("  ");
    OS_Write0(help_key);
    OS_Write0("\r\n");
    OS_Write0("  ");
    OS_Write0(copro_key);
    OS_Write0("\r\n");
  } else if (matchCommand(params, help_key, 0)) {
    // *HELP ARM
    // *HELP AR.
    // *HELP A.
    // *HELP .
    OS_Write0(help);
    for (int i = 0; i < NUM_CMDS; i++) {
      OS_Write0("  ");
      OS_Write0(cmdStrings[i]);
      OS_Write0("\r\n");
    }
  } else if (matchCommand(params, copro_key, 0)) {
     // *HELP COPROS
     // *HELP COPRO.
     // *HELP COPR.
     // *HELP COP.
     // *HELP CO.
     // *HELP C.
     OS_Write0(" n Processor - *FX ");
     OS_Write0(get_elk_mode() ? "147" : "151");
     OS_Write0(",230,n\r\n");
     for (int i = 0; i < num_copros(); i++) {

        copro_def_t *copro_def = &copro_defs[i];

        if (copro_def->type == TYPE_HIDDEN) {
           continue;
        }

        if (i >= 10) {
           OS_WriteC('0' + i / 10);
        } else {
           OS_WriteC(' ');
        }
        OS_WriteC('0' + i % 10);
        OS_WriteC(' ');
        OS_Write0(copro_def->name);
        if (i == 1 || i == 3) {
          if (snprintf(buff, 10, " (%uMHz)", get_copro_mhz(i)) < 10) {
            OS_Write0(buff);
          }
        }
        OS_Write0("\r\n");
     }
  }
  // pass the command on to the CLI regardless
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

int doCmdArmBasic(const char *params) {
  FunctionPtr_Type f;
  armbasic = 1;
  // Cast address to a generic function pointer
  f = (FunctionPtr_Type) ARM_BASIC_EXEC;
  f(params);
  return 0;
}
