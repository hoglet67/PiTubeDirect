#include <stdio.h>
#include <stdint.h>
#include <inttypes.h>
#include <string.h>

#include "../cpu_debug.h"
#include "../copro-f100.h"

#include "f100.h"
#include "f100_debug.h"

/*****************************************************
 * CPU Debug Interface
 *****************************************************/

int f100_debug_enabled = 0;

enum register_numbers {
   i_ACC,
   i_OR,
   i_PC,
   i_PSR
};

// NULL pointer terminated list of register names.
static const char *dbg_reg_names[] = {
   "ACC",
   "OR",
   "PC",
   "PSR",
   NULL
};


static const char *bit_names[] = {
   "???",    // F=0; T=0; S=3; J=0 (Not to be used)
   "???",    // F=0; T=0; S=3; J=1 (Not to be used)
   "SET",    // F=0; T=0; S=3; J=2
   "CLR"     // F=0; T=0; S=3; J=3
};

static const char *jmp_names[] = {
   "JBC",    // F=0; T=0; S=2; J=0
   "JBS",    // F=0; T=0; S=2; J=1
   "JCS",    // F=0; T=0; S=2; J=2
   "JSC"     // F=0; T=0; S=2; J=3
 };

static const char *shift_names[] = {
   "SRA",    // F=0; T=0; S=0; J=0
   "SRA",    // F=0; T=0; S=0; J=1
   "SRL",    // F=0; T=0; S=0; J=2
   "SRE",    // F=0; T=0; S=0; J=3
   "SLA",    // F=0; T=0; S=1; J=0
   "SLA",    // F=0; T=0; S=1; J=1
   "SLL",    // F=0; T=0; S=1; J=2
   "SLE"     // F=0; T=0; S=1; J=3
};

static const char *rtn_names[] = {
   "RTN",    // F=3; I=0
   "RTC",    // F=3; I=1
 };

static const char *opcode_names[] = {
   "",       // F=0 - broken out above
   "SJM",    // F=1
   "CAL",    // F=2
   "",       // F=3 - broken out above
   "STO",    // F=4
   "ADS",    // F=5
   "SBS",    // F=6
   "ICZ",    // F=7
   "LDA",    // F=8
   "ADD",    // F=9
   "SUB",    // F=10
   "CMP",    // F=11
   "AND",    // F=12
   "NEQ",    // F=13
   "F14",    // F=14
   "JMP"     // F=15
};

static uint8_t* data_map = NULL;

// NULL pointer terminated list of trap names.
static const char *dbg_trap_names[] = {
   NULL
};

// enable/disable debugging on this CPU, returns previous value.
static int dbg_debug_enable(int newvalue) {
   int oldvalue = f100_debug_enabled;
   f100_debug_enabled = newvalue;
   return oldvalue;
}

// CPU's usual memory read function for data.
static uint32_t dbg_memread(uint32_t addr) {
   return copro_f100_read_mem((uint16_t)addr);
}

// CPU's usual memory write function.
static void dbg_memwrite(uint32_t addr, uint32_t value) {
   copro_f100_write_mem((uint16_t)addr, (uint16_t)value);
}


static uint32_t dbg_disassemble(uint32_t addr, char *buf, size_t bufsize) {
   int len;

   int need_newline = 1;

   // Track words to skip following instruction
   int skip = 0;

   // Check if the word is mapped as data
   if (data_map && data_map[addr]) {

      skip = 1;
      need_newline = 0;

   } else {

      // Temporary buffer for formatting the operand
      char op_buf[16];

      // Read the instruction
      uint16_t instr = copro_f100_read_mem((uint16_t)addr);

      // Decode the instruction
      uint16_t f = (instr >> 12) & 0x000F;
      uint16_t i = (instr >> 11) & 0x0001;
      uint16_t t = (instr >> 10) & 0x0003; // to qualify opcode=0
      uint16_t r = (instr >>  8) & 0x0003;
      uint16_t s = (instr >>  6) & 0x0003;
      uint16_t j = (instr >>  4) & 0x0003;
      uint16_t n = (instr      ) & 0x07FF;
      uint16_t p = (instr      ) & 0x00FF;
      uint16_t b = (instr      ) & 0x000F;

      // Track the instruction length
      uint32_t oplen = 1;

      // Format the operand
      uint16_t operand = 0;
      if (f == 0) {
         if (t == 0) {
            // Shift, Bit Manipulation, Jump
            if (r == 1) {
               sprintf(op_buf, "CR");
            } else if (r == 3) {
               operand = copro_f100_read_mem((uint16_t)(addr + oplen)) & 0x7FFF;
               oplen++;
               sprintf(op_buf, "%04"PRIx16, operand);
            } else {
               sprintf(op_buf, "A");
            }
         }
      } else if (f != 1 && f != 3 && f != 14) {
         // Default group
         if (i == 0) {
            if (n == 0) {
               operand = copro_f100_read_mem((uint16_t)(addr + oplen));
               oplen++;
               sprintf(op_buf, ",%04"PRIx16, operand);
            } else {
               sprintf(op_buf, " %04"PRIx16, n);
            }
         } else {
            if (p == 0) {
               operand = copro_f100_read_mem((uint16_t)(addr + oplen)) & 0x7FFF;
               oplen++;
               sprintf(op_buf, ".%04"PRIx16, operand);
            } else {
               // Exclude CAL (f=2) from /P+ /P- as these have no effect
               if (r == 1 && f != 2) {
                  sprintf(op_buf, "/%04"PRIx16"+", p);
               } else if (r == 3 && f != 2) {
                  sprintf(op_buf, "/%04"PRIx16"-", p);
               } else {
                  sprintf(op_buf, "/%04"PRIx16, p);
               }
            }
         }
      }

      // Is there a additional target word
      uint16_t target = 0;
      if ((f == 0 && t == 0 && s == 2) || (f == 7)) {
         // Conditional Jump and ICZ
         target = copro_f100_read_mem((uint16_t)(addr + oplen)) & 0x7FFF;
         oplen++;
      }

      // Output address in hex and a separator
      len = snprintf(buf, bufsize, "%04"PRIx32" : ", addr);
      buf += len;
      bufsize -= (uint32_t)len;


      // Output the opcodes words in hex
      for (uint32_t z = 0; z < 3; z++) {
         if (z < oplen) {
            len = snprintf(buf, bufsize, "%04"PRIx16" ", copro_f100_read_mem((uint16_t)(addr + z)));
         } else {
            len = snprintf(buf, bufsize, "     ");
         }
         buf += len;
         bufsize -= (uint32_t)len;
      }

      // Output a separator
      len = snprintf(buf, bufsize, ": ");
      buf += len;
      bufsize -= (uint32_t)len;


      // Optput the instruction

      switch (f) {

      case 0:
         if (t == 0) {
            // Shift, Bit Manipulation, Jump
            if (s == 2) {
               // Conditional Jump
               len = snprintf(buf, bufsize, "%s   %x %s %04"PRIx16, jmp_names[j], b, op_buf, target);
            } else if (s == 3) {
               // Bit Manipulation
               len = snprintf(buf, bufsize, "%s   %x %s", bit_names[j], b, op_buf);
            } else {
               // Shift
               len = snprintf(buf, bufsize, "%s   %x %s", shift_names[s * 4 + j], b, op_buf);
               // TODO: How to distinguish the double length shifts
               // as these depend on CR.M. This makes an accurate disassembler
               // impossible!
            }
         } else if (t == 1) {
            // Halt
            len = snprintf(buf, bufsize, "HALT ,%04"PRIx16, n & 0x3FF);
         } else {
            // External Functions
            len = snprintf(buf, bufsize, "EXT  ,%04"PRIx16, n);
            if (n >= 0x400 && n <= 0x40f) {
               // F101L Co Pro instructions have three word addresses as args
               skip = 3;
            }
         }
         break;

      case 1:
         // SJM
      case 14:
         // F14
         len = snprintf(buf, bufsize, "%s", opcode_names[f]);
         break;

      case 3:
         // RTN/RTC
         len = snprintf(buf, bufsize, "%s", rtn_names[i]);
         break;

      case 7:
         // ICZ
         len = snprintf(buf, bufsize, "%s  %s %04"PRIx16, opcode_names[f], op_buf, target);
         break;

      default:
         len = snprintf(buf, bufsize, "%s  %s", opcode_names[f], op_buf);
         break;
      }
      buf += len;
      bufsize -= (uint32_t)len;

      addr += oplen;
   }

   // Output any additional words to be skipped following the instruction
   for (int i = 0; i < skip; i++) {
      if (need_newline) {
         len = snprintf(buf, bufsize, "\n");
         buf += len;
         bufsize -= (uint32_t)len;
      }
      len = snprintf(buf, bufsize, "%04"PRIx32" : %04"PRIX16"           : DATA", addr, copro_f100_read_mem((uint16_t)addr));
      buf += len;
      bufsize -= (uint32_t)len;
      addr++;
      need_newline = 1;
   }

   return addr;
}

// Get a register - which is the index into the names above
static uint32_t dbg_reg_get(int which) {
   if (which == i_ACC) {
      return m_f100->acc;
   } else if (which == i_OR) {
      return m_f100->or;
   } else if (which == i_PC) {
      return m_f100->pc;
   } else {
      // TODO: Would be nice to re-used PACK_FLAGS
      uint32_t psr = 0;
      if (m_f100->I) {
         psr |= 1;
      }
      if (m_f100->Z) {
         psr |= 2;
      }
      if (m_f100->V) {
         psr |= 4;
      }
      if (m_f100->S) {
         psr |= 8;
      }
      if (m_f100->C) {
         psr |= 16;
      }
      if (m_f100->M) {
         psr |= 32;
      }
      if (m_f100->F) {
         psr |= 64;
      }
      return psr;
   }
}

// Set a register.
static void  dbg_reg_set(int which, uint32_t value) {
   if (which == i_ACC) {
      m_f100->acc = (uint16_t)value;
   } else if (which == i_OR) {
      m_f100->or = (uint16_t)value;
   } else if (which == i_PC) {
      m_f100->pc = (uint16_t)value;
   } else {
      // TODO: Would be nice to re-use UNPACK_FLAGS
      m_f100->I = (value >> 0) & 1;
      m_f100->Z = (value >> 1) & 1;
      m_f100->V = (value >> 2) & 1;
      m_f100->S = (value >> 3) & 1;
      m_f100->C = (value >> 4) & 1;
      m_f100->M = (value >> 5) & 1;
      m_f100->F = (value >> 6) & 1;
   }
}

static const char* flagname = "F M C S V Z I ";

// Print register value in CPU standard form.
static size_t dbg_reg_print(int which, char *buf, size_t bufsize) {
   if (which == i_PSR) {
      uint32_t bit;
      char c;
      const char *flagnameptr = flagname;
      uint32_t psr = dbg_reg_get(i_PSR);

      if (bufsize < 40) {
         strncpy(buf, "buffer too small!!!", bufsize);
      }

      // Print the remaining flag bits
      bit = 0x40;
      for (int i = 0; i < 7; i++) {
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
      *buf++ = '\0';
      return strlen(buf);
   } else {
      return (size_t) snprintf(buf, bufsize, "%04"PRIx32, dbg_reg_get(which));
   }
}

// Parse a value into a register.
static void dbg_reg_parse(int which, const char *strval) {
   uint32_t val = 0;
   sscanf(strval, "%"SCNx32, &val);
   dbg_reg_set(which, val);
}

static uint32_t dbg_get_instr_addr() {
   return m_f100->saved_pc;
}

const cpu_debug_t f100_cpu_debug = {
   .cpu_name       = "F100",
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
   .mem_width      = WIDTH_16BITS
};

/*****************************************************
 * Standalone operation as a disassembler
 *****************************************************/

#ifdef STANDALONE

#define MEM_SIZE 0x10000

cpu_t *m_f100 = NULL;

uint16_t memory[MEM_SIZE];

void copro_f100_write_mem(uint16_t addr, uint16_t data) {
   memory[addr] = data;
}

uint16_t copro_f100_read_mem(uint16_t addr) {
   return memory[addr];
}

void usage() {
   fprintf(stderr, "usage: f100dis [-a <address> ] [ -x <xor pattern> ] [ -d <data map file>] <binary file>...\n");
   exit(1);
}

void main(int argc, char *argv[]) {
   char buf[1024];
   int addr = 0;
   int lowest = MEM_SIZE;
   int highest = 0;
   char *data_mapfile = NULL;
   uint16_t xor = 0;

   const char whitespace[] = " \f\n\r\t\v";

   int s = 1;
   while (s <= argc && argv[s][0] == '-') {
      if (strcmp(argv[s], "-a") == 0) {
         addr = strtol(argv[s + 1], NULL, 16);
         s += 2;
      } else if (strcmp(argv[s], "-x") == 0) {
         xor = strtol(argv[s + 1], NULL, 16);
         s += 2;
      } else if (strcmp(argv[s], "-d") == 0) {
         data_mapfile = argv[s + 1];
         s += 2;
      } else {
         usage();
      }
   }

   if (data_mapfile) {
      data_map = (uint8_t *)malloc(MEM_SIZE);
      memset(data_map, 0, MEM_SIZE);

      FILE *f = fopen(data_mapfile, "r");
      if (f == 0) {
         perror(data_mapfile);
         exit(1);
      }
      int done = 0;
      do {
         char *line = NULL;
         size_t len = 0;
         ssize_t lineSize = 0;
         lineSize = getline(&line, &len, f);
         if (lineSize > 0) {
            char *start = line + strspn(line, whitespace);
            if (start[0] != '#') {
               unsigned int data_start = 0;
               unsigned int data_end   = 0;
               int n = sscanf(start, "%x,%x", &data_start, &data_end);
               if (n < 2) {
                  data_end = data_start;
               }
               if (n > 0) {
                  for (int i = data_start; i <= data_end; i++) {
                     data_map[i] = 1;
                  }
               }
            }
            free(line);
         } else {
            done = 1;
         }
      } while (!done);
   }

   for (int i = 0; i < MEM_SIZE; i++) {
      memory[i] = 0;
   }

   for (int i = s; i < argc; i++) {
      FILE *f = fopen(argv[i], "r");
      if (f == NULL) {
         perror(argv[i]);
         exit(1);
      }
      size_t num;
      do {
         int actual = addr ^ xor;
         num = fread(memory + actual, sizeof(uint16_t), 1, f);
         if (num > 0) {
            if (actual < lowest) {
               lowest = actual;
            }
            if (addr > highest) {
               highest = addr;
            }
            addr++;
         }
      } while (num > 0);
   }

   addr = lowest;
   do {
      addr = dbg_disassemble(addr, buf, sizeof(buf));
      printf("%s\n", buf);
   } while (addr <= highest);

   if (data_map) {
      free(data_map);
   }
}

#endif
