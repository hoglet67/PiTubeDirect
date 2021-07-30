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

static int doCmdHelp(const char *params);
static int doCmdTest(const char *params);
static int doCmdGo(const char *params);
static int doCmdMem(const char *params);
static int doCmdDis(const char *params);
static int doCmdFill(const char *params);
static int doCmdCrc(const char *params);
static int doCmdArmBasic(const char *params);
static int doCmdPiVDU(const char *params);
static int doCmdPiLIFE(const char *params);

// Include ARM Basic
#include "armbasic.h"

// For fb_set_vdu_device (used by *PIVDU)
#include "framebuffer/framebuffer.h"

// Include the framebuffer for fb_set_vdu_device

static const char *help = "Native ARM Tube Client ("RELEASENAME"/"GITVERSION")\r\n";

static const char *help_key = "ARM";

static const char *copro_key = "COPROS";

static char line[256];

#define WRCVEC 0x020E

#define WRCHANDLER 0x0900

/***********************************************************
 * Build in Commands
 ***********************************************************/

#define NUM_CMDS 10

// Must be kept in step with cmdFuncs (just below)
static const char *cmdStrings[NUM_CMDS] = {
  "HELP",
  "TEST",
  "GO",
  "MEM",
  "DIS",
  "FILL",
  "CRC",
  "ARMBASIC",
  "PIVDU",
  "PILIFE"
};

static int (*cmdFuncs[NUM_CMDS])(const char *params) = {
  doCmdHelp,
  doCmdTest,
  doCmdGo,
  doCmdMem,
  doCmdDis,
  doCmdFill,
  doCmdCrc,
  doCmdArmBasic,
  doCmdPiVDU,
  doCmdPiLIFE,
};

static const int cmdMode[NUM_CMDS] = {
  MODE_USER,
  MODE_USER,
  MODE_USER,
  MODE_USER,
  MODE_USER,
  MODE_USER,
  MODE_USER,
  MODE_USER,
  MODE_USER,
  MODE_USER
};

static const char *matchCommand(const char *cmdPtr, const char *refPtr, int minLen) {
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
        return user_exec_fn(cmdFuncs[i], ( unsigned int) paramPtr);
      } else {
        // Execute the command in supervisor mode
        return cmdFuncs[i](paramPtr);
      }
    }
  }
  // non-zero means pass the command onto the CLI processor
  return 1;
}

static int doCmdTest(const char *params) {
  OS_Write0("doCmdTest\r\n");
  return 0;
}

static int doCmdHelp(const char *params) {
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
     for (unsigned char i = 0; i < num_copros(); i++) {

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

static int doCmdGo(const char *params) {
  unsigned int address;
  FunctionPtr_Type f;
  sscanf(params, "%x", &address);
  // Cast address to a generic function pointer
  f = (FunctionPtr_Type) address;
  f();
  return 0;
}

static int doCmdFill(const char *params) {
  unsigned int start;
  unsigned int end;
  unsigned char data;
  sscanf(params, "%x %x %hhu", &start, &end, &data);
  for (unsigned int i = start; i <= end; i++) {
    *((unsigned char *)i) = data;
  }
  return 0;
}

static int doCmdMem(const char *params) {
  unsigned char c;
  char *ptr;
  unsigned int flags;
  unsigned int memAddr;
  sscanf(params, "%x", &memAddr);
  do {
    for (unsigned int i = 0; i < 16; i++) {
      ptr = line;
      // Generate the address
      ptr += sprintf(ptr, "%08X ", memAddr);
      // Generate the hex values
      for (unsigned int j = 0; j < 16; j++) {
        c = *((unsigned char *)(memAddr + j));
        ptr += sprintf(ptr, "%02X ", c);
      }
      // Generate the ascii values
      for (unsigned int j = 0; j < 16; j++) {
        c = *((unsigned char *)(memAddr + j));
        if (c < 32 || c > 126) {
          c = '.';
        }
        ptr += sprintf(ptr, "%c", c);
      }
      sprintf(ptr, "\r\n");
      OS_Write0(line);
      memAddr += 0x10;
    }
    OS_ReadC(&flags);
  } while ((flags & CARRY_MASK) == 0);
  OS_Byte(0x7e, 0x00, 0x00, NULL, NULL);
  return 0;
}

static int doCmdDis(const char *params) {
  darm_t d;
  darm_str_t str;
  int i;
  unsigned int flags;
  unsigned int opcode;
  unsigned int memAddr;
  sscanf(params, "%x", &memAddr);
  memAddr &= ~3u;
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

static int doCmdCrc(const char *params) {
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

static int doCmdArmBasic(const char *params) {
  FunctionPtr_Type f;
  armbasic = 1;
  // Cast address to a generic function pointer
  f = (FunctionPtr_Type) ARM_BASIC_EXEC;
  f(params);
  return 0;
}

static uint8_t read_host_byte(uint16_t address) {
   unsigned int block[2] = {address, 0};
   OS_Word(5, block);
   return block[1] & 0xff;
}

static uint16_t read_host_word(uint16_t address) {
   return (uint16_t) ( (read_host_byte(address + 1) << 8) | read_host_byte(address));
}

static void write_host_byte(uint16_t address, uint8_t data) {
   unsigned int block[2] = {address, data};
   OS_Word(6, block);
}

static void write_host_word(uint16_t address, uint16_t data) {
   write_host_byte(address + 0,(unsigned char)  data       & 0xff);
   write_host_byte(address + 1,(unsigned char) (data >> 8) & 0xff);
}


int doCmdPiVDU(const char *params) {
   int device = atoi(params);
   OS_Write0("Beeb VDU:");
   if (device & VDU_BEEB) {
      OS_Write0("enabled\r\n");
   } else {
      OS_Write0("disabled\r\n");
   }
   OS_Write0("  Pi VDU:");
   if (device & VDU_PI) {
      OS_Write0("enabled\r\n");
   } else {
      OS_Write0("disabled\r\n");
   }
   if (device & VDU_PI) {
      // *FX 4,1 to disable cursor editing
      OS_Byte(4, 1, 0, NULL, NULL);
   } else {
      // *FX 4,0 to enable cursor editing
      OS_Byte(4, 0, 0, NULL, NULL);
   }
   fb_set_vdu_device(device);

   if (device & VDU_BEEB) {
      // Restore the original host OSWRCH
      uint16_t default_vector_table = read_host_word(0xffb7);
      uint16_t default_oswrch = read_host_word(default_vector_table + (WRCVEC & 0xff));
      write_host_word(WRCVEC, default_oswrch);
   } else {
      // Install a host oswrch redirector (for output of OSCLI commands)
      // PHA        48
      // LDA #&03   A9 03
      // STA &FEE2  8D E2 FE
      // PLA        68
      // STA &FEE4  8D E4 FE
      // RTS        60
      uint8_t data[] = { 0x48, 0xa9, 0x03, 0x8d, 0xe2, 0xfe, 0x68, 0x8d, 0xe4, 0xfe, 0x60} ;
      for (uint16_t i = 0; i < sizeof(data); i++) {
         write_host_byte(WRCHANDLER + i, data[i]);
      }
      write_host_word(WRCVEC, WRCHANDLER);
   }
   return 0;
}

int doCmdPiLIFE(const char *params) {
   unsigned int mode = 1;

   int generations = 0;
   unsigned int sx          = 0;
   unsigned int sy          = 0;

   int nargs = sscanf(params, "%d %u %u", &generations, &sx, &sy);

   if (nargs < 1) {
      generations = 1000;
   }
   if (nargs < 2) {
      sx = 256;
   }
   if (nargs < 3) {
      sy = 256;
   }

   // Read the current screen mode
   OS_Byte(135, 0, 0, NULL, &mode);

   // Read the s/y eigfactors
   int xeigfactor = OS_ReadModeVariable(mode, 4);
   int yeigfactor = OS_ReadModeVariable(mode, 5);

   uint8_t *a = calloc(sx * sy, 1);
   uint8_t *b = calloc(sx * sy, 1);

   // Randomize interior
   for (unsigned int y = 1; y < sy - 1; y++) {
      uint8_t *p = a + y * sx + 1;
      for (unsigned int x = 1; x < sx - 1; x++) {
         *p++ = random() & 1;
      }
   }

   // Set Foreground colour to white (0xCC)
   // (GCOL 0,63)
   OS_WriteC(18);
   OS_WriteC(0);
   OS_WriteC(63);

   // Set Background colour to black (0x00)
   // (GCOL 0,128)
   OS_WriteC(18);
   OS_WriteC(0);
   OS_WriteC(128);

#if 0
   // (TINT 2,192)
   OS_WriteC(23);
   OS_WriteC(17);
   OS_WriteC(2);
   OS_WriteC(192);
   OS_WriteC(0);
   OS_WriteC(0);
   OS_WriteC(0);
   OS_WriteC(0);
   OS_WriteC(0);
   OS_WriteC(0);
#endif

   // Draw Initial Configuration
   for (unsigned int y = 1; y < sy - 1; y++) {
      uint8_t *p = a + y * sx + 1;
      for (unsigned int x = 1; x < sx - 1; x++) {
         if (*p++) {
            OS_WriteC(25);
            OS_WriteC(69);
            OS_WriteC((unsigned char)(x << xeigfactor) & 255);
            OS_WriteC((unsigned char)(x >> (8 - xeigfactor)) & 255);
            OS_WriteC((unsigned char)(y << yeigfactor) & 255);
            OS_WriteC((unsigned char)(y >> (8 - yeigfactor)) & 255);
         }
      }
   }

   // Run Life
   for (int gen = 0; gen < generations; gen++) {
      for (unsigned int y = 1; y < sy - 1; y++) {
         uint8_t *p1 = a + (y - 1) * sx;
         uint8_t *p2 = a +  y      * sx;
         uint8_t *p3 = a + (y + 1) * sx;
         uint8_t *p4 = b + y * sx + 1;
         for (unsigned int x = 1; x < sx - 1; x++) {
            int count =
               *p1 + *(p1 + 1) + *(p1 + 2) +
               *p2 +             *(p2 + 2) +
               *p3 + *(p3 + 1) + *(p3 + 2);
            uint8_t o = *(p2 + 1);
            uint8_t n = ((count == 2 && o == 1) || count == 3) ? 1 : 0;
            *p4 = n;
            p1++;
            p2++;
            p3++;
            p4++;
            if (n != o) {
               OS_WriteC(25);
               OS_WriteC(n ? 69 : 71);
               OS_WriteC((unsigned char)(x << xeigfactor) & 255);
               OS_WriteC((unsigned char)(x >> (8 - xeigfactor)) & 255);
               OS_WriteC((unsigned char)(y << yeigfactor) & 255);
               OS_WriteC((unsigned char)(y >> (8 - yeigfactor)) & 255);
            }
         }
      }
      // Swap a and b
      uint8_t *tmp = a;
      a = b;
      b = tmp;
   }

   free(a);
   free(b);

   return 0;
}
