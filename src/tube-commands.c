#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "swi.h"
#include "tube-swi.h"
#include "tube.h"
#include "tube-client.h"
#include "tube-lib.h"
#include "tube-commands.h"
#include "darm/darm.h"
#include "tube-defs.h"
#include "tube-ula.h"
#include "gitversion.h"
#include "copro-defs.h"
#include "utils.h"
#include "programs.h"

static int doCmdHelp    (const char *params);
static int doCmdTest    (const char *params);
static int doCmdGo      (const char *params);
static int doCmdMem     (const char *params);
static int doCmdDis     (const char *params);
static int doCmdFill    (const char *params);
static int doCmdCrc     (const char *params);
static int doCmdArmBasic(const char *params);
static int doCmdPiVDU   (const char *params);
static int doCmdPiLIFE  (const char *params);
static int doCmdFX      (const char *params);

// Include ARM Basic
#include "armbasic.h"

// For fb_set_vdu_device (used by *PIVDU)
#include "framebuffer/framebuffer.h"

static const char *help = "Native ARM Tube Client ("RELEASENAME"/"GITVERSION")\r\n";

static const char *help_key = "ARM";

static const char *copro_key = "COPROS";

#define WRCVEC 0x020E

#define WRCHANDLER 0x0900

#define MAX_CMD_LEN 256

/***********************************************************
 * Build in Commands
 ***********************************************************/

typedef struct {
  const char *name;
  const char *options;
  int (*fn)(const char *);
  const int mode;
  const int vdu;
} cmd_type;

cmd_type cmds[] = {
  { "ARMBASIC", "[ <arg> ... ]",                               doCmdArmBasic, MODE_USER, 0 },
  { "CRC",      "<start> <end>",                               doCmdCrc,      MODE_USER, 0 },
  { "DIS",      "<address>",                                   doCmdDis,      MODE_USER, 0 },
  { "FILL",     "<start> <end> <data>",                        doCmdFill,     MODE_USER, 0 },
  { "FX",       "<a>, <x>, <y>",                               doCmdFX,       MODE_USER, 1 },
  { "HELP",     "[ <command> ]",                               doCmdHelp,     MODE_USER, 0 },
  { "GO",       "<address>",                                   doCmdGo,       MODE_USER, 0 },
  { "MEM",      "<address>",                                   doCmdMem,      MODE_USER, 0 },
  { "PIVDU",    "<device: 0..3>",                              doCmdPiVDU,    MODE_USER, 1 },
  { "PILIFE",   "[ <generations> [ <x size> [ <y size> ] ] ]", doCmdPiLIFE,   MODE_USER, 1 },
  { "TEST",     "",                                            doCmdTest,     MODE_USER, 0 },
};

#define NUM_CMDS (sizeof(cmds) / sizeof(cmd_type))

static cmd_type *last_cmd = NULL;

static void usage() {
  OS_Write0("Usage: *");
  OS_Write0(last_cmd->name);
  OS_WriteC(' ');
  OS_Write0(last_cmd->options);
  OS_Write0("\r\n");
  return;
}

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
  for (unsigned int i = 0; i < NUM_CMDS; i++) {
    // Skip vdu specific commands if vdu=0 in cmdline.txt
    if (cmds[i].vdu && !vdu_enabled) {
      continue;
    }
    const char *paramPtr = matchCommand(cmd, cmds[i].name, 1);
    if (paramPtr) {
      last_cmd = &cmds[i];
      if (cmds[i].mode == MODE_USER) {
        // Execute the command in user mode
        return user_exec_fn(cmds[i].fn, ( unsigned int) paramPtr);
      } else {
        // Execute the command in supervisor mode
        return cmds[i].fn(paramPtr);
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


static char *copy_string(const char *params) {
  static char copy[MAX_CMD_LEN];
  int i = 0;
  while (*params != 0x00 && *params != 0x0a && *params != 0x0d && i < MAX_CMD_LEN - 1) {
    copy[i++] = *params++;
  }
  // Make sure the terminator is 0, so we can parse this as a C string
  copy[i] = 0x00;
  return copy;
}

static int doCmdHelp(const char *params) {
  params = copy_string(params);
  if (*params == 0x00) {
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
    // Work out max command length for options formattiong
    unsigned int maxlen = 0;
    for (unsigned int i = 0; i < NUM_CMDS; i++) {
      // Skip vdu specific commands if vdu=0 in cmdline.txt
      if (cmds[i].vdu && !vdu_enabled) {
        continue;
      }
      unsigned int len = strlen(cmds[i].name);
      if (len > maxlen) {
        maxlen = len;
      }
    }
    for (int i = 0; i < NUM_CMDS; i++) {
      // Skip vdu specific commands if vdu=0 in cmdline.txt
      if (cmds[i].vdu && !vdu_enabled) {
        continue;
      }
      OS_Write0("  ");
      OS_Write0(cmds[i].name);
      unsigned int optlen = strlen(cmds[i].options);
      if (optlen > 0) {
         for (int pad = 0; pad <= maxlen - strlen(cmds[i].name); pad++) {
          OS_WriteC(' ');
        }
      }
      OS_Write0(cmds[i].options);
      OS_Write0("\r\n");
    }
  } else if (matchCommand(params, copro_key, 0)) {
     // *HELP COPROS
     // *HELP COPRO.
     // *HELP COPR.
     // *HELP COP.
     // *HELP CO.
     // *HELP C.
     OS_Write0(" n   Processor - *FX ");
     OS_Write0(get_elk_mode() ? "147" : "151");
     OS_Write0(",230,n\r\n");
     for (unsigned char i = 0; i < num_copros(); i++) {

        const copro_def_t *copro_def = &copro_defs[i];

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
        if (i == copro) {
           OS_WriteC('*');
        } else {
           OS_WriteC(' ');
        }
        OS_WriteC(' ');
        OS_Write0(get_copro_name(i, 32));
        OS_Write0("\r\n");
     }
  }
  // pass the command on to the CLI regardless
  return 1;
}

static int doCmdGo(const char *params) {
  unsigned int address;
  FunctionPtr_Type f;
  params = copy_string(params);
  int nargs = sscanf(params, "%x", &address);
  if (nargs != 1) {
    usage();
    return 0;
  }
  // Cast address to a generic function pointer
  f = (FunctionPtr_Type) address;
  f();
  return 0;
}

static int doCmdFX(const char *params) {
  unsigned int a = 0;
  unsigned int x = 0;
  unsigned int y = 0;
  params = copy_string(params);
  sscanf(params, "%ud%*c%ud%*c%ud", &a, &x, &y);
  OS_Byte(a, x, y, &x, &y);
  return 0;
}

static int doCmdFill(const char *params) {
  unsigned int start;
  unsigned int end;
  unsigned char data;
  params = copy_string(params);
  int nargs = sscanf(params, "%x %x %hhu", &start, &end, &data);
  if (nargs != 3) {
    usage();
    return 0;
  }
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
  char line[256];
  params = copy_string(params);
  int nargs = sscanf(params, "%x", &memAddr);
  if (nargs != 1) {
    usage();
    return 0;
  }
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
  params = copy_string(params);
  int nargs = sscanf(params, "%x", &memAddr);
  if (nargs != 1) {
    usage();
    return 0;
  }
  memAddr &= ~3u;
  do {
    for (i = 0; i < 16; i++) {
      char line[256];
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
  unsigned long crc = 0;
  char line[16];
  params = copy_string(params);
  int nargs = sscanf(params, "%x %x", &start, &end);
  if (nargs != 2) {
    usage();
    return 0;
  }
  for (i = start; i <= end; i++) {
    unsigned int data = *((unsigned char *)i);
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

// Forced Beeb-side VDU output (independent of vdu_device)
static void beeb_vdu(uint8_t c) {
   sendByte(R1_ID, c);
}

// Manage the Beeb-side cursor
static void beeb_cursor(int on) {
   beeb_vdu(23);
   beeb_vdu(1);
   beeb_vdu(on ? 1 : 0);
   beeb_vdu(0);
   beeb_vdu(0);
   beeb_vdu(0);
   beeb_vdu(0);
   beeb_vdu(0);
   beeb_vdu(0);
   beeb_vdu(0);
}

// Forced Pi-side VDU output (independent of vdu_device)
static void pi_vdu(uint8_t c) {
   fb_writec(c);
}

// Manage the Pi-side cursor
static void pi_cursor(int on) {
   pi_vdu(23);
   pi_vdu(1);
   pi_vdu(on ? 1 : 0);
   pi_vdu(0);
   pi_vdu(0);
   pi_vdu(0);
   pi_vdu(0);
   pi_vdu(0);
   pi_vdu(0);
   pi_vdu(0);
}

int doCmdPiVDU(const char *params) {
   static int saved_state = -1;
   int d;
   params = copy_string(params);
   int nargs = sscanf(params, "%d", &d);
   if (nargs != 1 || d < 0 || d > 3) {
      usage();
      OS_Write0("       0 = no output\r\n");
      OS_Write0("       1 = Beeb only\r\n");
      OS_Write0("       2 = Pi Only\r\n");
      OS_Write0("       3 = Beeb and Pi\r\n");
      return 0;
   }
   vdu_device_t device = (vdu_device_t) d;

   OS_Write0("Beeb VDU:");
   if (device == VDU_BEEB || device == VDU_BOTH) {
      OS_Write0("enabled\r\n");
   } else {
      OS_Write0("disabled\r\n");
   }
   OS_Write0("  Pi VDU:");
   if (device == VDU_PI || device == VDU_BOTH) {
      OS_Write0("enabled\r\n");
   } else {
      OS_Write0("disabled\r\n");
   }
   if (device == VDU_PI || device == VDU_BOTH) {
      // *FX 4,1 to disable cursor editing
      OS_Byte(4, 1, 0, NULL, NULL);
   } else {
      // *FX 4,0 to enable cursor editing
      OS_Byte(4, 0, 0, NULL, NULL);
   }

   // Set the VDU device variable
   fb_set_vdu_device(device);

   // Re-run the module init code, in case any handlers need to change
   swi_modules_init(device == VDU_PI || device == VDU_BOTH);

   // Restore the original host OSWRCH, so we can manage the cursor
   uint16_t default_vector_table = read_host_word(0xffb7);
   uint16_t default_oswrch = read_host_word(default_vector_table + (WRCVEC & 0xff));
   write_host_word(WRCVEC, default_oswrch);

   // Enable/disable beeb-side cursor
   beeb_cursor(device == VDU_BEEB || device == VDU_BOTH);

   // Enable/disable pi-side cursor
   pi_cursor(device == VDU_PI || device == VDU_BOTH);

   // Work around for ADFS formatting bug (#130) and *HELP MOS bug (#143)
   if (device == VDU_BEEB || device == VDU_BOTH) {
      // restore the original beeb text window left/right window limits
      if (saved_state > 0) {
         write_host_byte(0x0308, (uint8_t)(saved_state & 0xff));
         write_host_byte(0x030A, (uint8_t)((saved_state >> 8) & 0xff));
         saved_state = -1;
      }
   } else if (device == VDU_PI) {
      // force the beeb text window left/right window limits to 0/79
      if (saved_state < 0) {
         saved_state = read_host_byte(0x0308) | (read_host_byte(0x030A) << 8);
         write_host_byte(0x0308,  0);
         write_host_byte(0x030A, 79);
      }
   }

   // Install the appropriate Host OSWRCH driver
   switch (device) {
   case VDU_NONE:
      // Replace Host OSWRCH with RTS to swallow everything
      write_host_byte(WRCHANDLER, 0x60);
      write_host_word(WRCVEC, WRCHANDLER);
      break;
   case VDU_BEEB:
      // Leave Host OSWRCH at default_oswrch
      break;
   case VDU_PI:
      // Replace Host OSWRCH with the redirector from the 6502 Co Pro (which fakes POS)
      for (uint16_t i = 0; i < 0x40; i++) {
         write_host_byte(WRCHANDLER + i, host_oswrch_redirector[i]);
      }
      write_host_word(WRCVEC, WRCHANDLER);
      break;
   case VDU_BOTH:
      // Replace Host OSWRCH with:
      // PHA
      // LDA #&03
      // STA &FEE2
      // PLA
      // STA &FEE4
      // JMP default_oswrch
      {
         uint16_t i = WRCHANDLER;
         write_host_byte(i++, 0x48);
         write_host_byte(i++, 0xA9);
         write_host_byte(i++, 0x03);
         write_host_byte(i++, 0x8D);
         write_host_byte(i++, 0xE2);
         write_host_byte(i++, 0xFE);
         write_host_byte(i++, 0x68);
         write_host_byte(i++, 0x8D);
         write_host_byte(i++, 0xE4);
         write_host_byte(i++, 0xFE);
         write_host_byte(i++, 0x4C);
         write_host_byte(i++, (uint8_t)(default_oswrch & 0xff));
         write_host_byte(i++, (uint8_t)((default_oswrch >> 8) & 0xff));
         write_host_word(WRCVEC, WRCHANDLER);
      }
      break;
   }
   return 0;
}

int doCmdPiLIFE(const char *params) {
   unsigned int mode = 1;

   int generations = 0;
   unsigned int sx          = 0;
   unsigned int sy          = 0;

   params = copy_string(params);
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
   if (  a == NULL )
      return 0;
   uint8_t *b = calloc(sx * sy, 1);

   if ( b == NULL )
   {
      free(a);
      return 0;
   }
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
