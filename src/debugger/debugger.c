#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "debugger.h"
#include "../rpi-aux.h"

#define NUM_CMDS 4

// The Atom CRC Polynomial
#define CRC_POLY          0x002d

static void doCmdCrc(char *params);
static void doCmdFill(char *params);
static void doCmdHelp(char *params);
static void doCmdMem(char *params);

// The command process accepts abbreviated forms, for example
// if h is entered, then help will match.

// Must be kept in step with dbgCmdFuncs (just below)
static char *dbgCmdStrings[NUM_CMDS] = {
  "help",
  "fill",
  "crc",
  "mem",
};

// Must be kept in step with dbgCmdStrings (just above)
static void (*dbgCmdFuncs[NUM_CMDS])(char *params) = {
  doCmdHelp,
  doCmdFill,
  doCmdCrc,
  doCmdMem,
};


/********************************************************
 * Other global variables
 ********************************************************/

char cmd[1000];


// The current memory address (e.g. used when disassembling)
unsigned int memAddr = 0;

/*******************************************
 * Memory accessors
 *******************************************/

static void writeMem(uint32_t address, uint8_t data) {
  *(uint8_t *)(address) = data;
}

static uint8_t readMem(uint32_t address) {
  return *(uint8_t *)(address);
}

/*******************************************
 * User Commands
 *******************************************/

static void doCmdHelp(char *params) {
  int i;
  printf("Commands:\r\n");
  for (i = 0; i < NUM_CMDS; i++) {
    printf("    %s\r\n", dbgCmdStrings[i]);
  }
}

static void doCmdFill(char *params) {
  long i;
  unsigned int start;
  unsigned int end;
  unsigned int data;
  sscanf(params, "%x %x %x", &start, &end, &data);
  printf("Wr: %04X to %04X = %02X\r\n", start, end, data);
  for (i = start; i <= end; i++) {
    writeMem(i, data);
  }
}

static void doCmdCrc(char *params) {
  long i;
  int j;
  unsigned int start;
  unsigned int end;
  unsigned int data;
  unsigned int crc = 0;
  sscanf(params, "%x %x", &start, &end);
  for (i = start; i <= end; i++) {
    data = readMem(i);
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
  int i, j;
  unsigned int row[16];
  sscanf(params, "%x", &memAddr);
  for (i = 0; i < 0x100; i+= 16) {
    for (j = 0; j < 16; j++) {
      row[j] = readMem(memAddr + i + j);
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

