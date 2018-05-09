#include <stdio.h>
#include <inttypes.h>
#include "../pdp11.h"
#include "../pdp11_debug.h"

// Uncomment to show a detailed execution trace
#define DEBUG

// Uncomment to allow unaligned (odd) word accesses to proceed
// Certain PDP-11 models work like this (e.g. the PDP 11/03 aka LSI-11)
#define ALLOW_UNALIGNED

int tube_irq = 1;

uint8_t memory[0x10000];

void copro_pdp11_write8(uint16_t addr, const uint8_t data) {
   if (addr == 0177566) {
      fprintf(stderr, "%c", data);
   } else if (addr == 0177776) {
      printf("byte writes to PSW unsupported\r\n");
   } else {
      *(memory + addr) = data;
   }
}

uint8_t copro_pdp11_read8(uint16_t addr) {
   if (addr == 0177564) {
      return -1;
   } else if (addr == 0177776) {
      printf("byte reads to PSW unsupported\r\n");
   }
   return *(memory + addr);
}

void copro_pdp11_write16(uint16_t addr, const uint16_t data) {
#ifdef ALLOW_UNALIGNED
   if (addr & 1) {
      printf("word write to odd address %06o (%04x)\r\n", addr, addr);
      addr &= ~1;
   }
#endif
   if (addr == 0177776) {
      m_pdp11->PS = data & 0xffef; // Mask off T bit

      switch (data >> 14) {
      case 0:
         pdp11_switchmode(false);
         break;
      case 3:
         pdp11_switchmode(true);
         break;
      default:
         printf("invalid mode\n");
      }
   } else {
      *(uint16_t *)(memory + addr) = data;
   }
}

uint16_t copro_pdp11_read16(uint16_t addr) {
#ifdef ALLOW_UNALIGNED
   if (addr & 1) {
      printf("word read to odd address %06o (%04x)\r\n", addr, addr);
      addr &= ~1;
   }
#endif
   if (addr == 0177776) {
      return m_pdp11->PS;
   } else {
      return *(uint16_t *)(memory + addr);
   }
}

#define   SIG_LSB 1
#define   SIG_MSB 2
#define   LEN_LSB 3
#define   LEN_MSB 4
#define  ADDR_LSB 5
#define  ADDR_MSB 6
#define      DATA 7
#define  CHECKSUM 8

int loader() {

   int c;

   int state = SIG_LSB;
   int len;
   int addr;
   int checksum = 0;
   int total = 0;
   int last = 0;

   while ((c = getchar()) != EOF) {

      checksum += c;

      switch (state) {
      case   SIG_LSB:
         if (c == 1) {
            state = SIG_MSB;
         }
         break;
      case   SIG_MSB:
         if (c == 0) {
            state = LEN_LSB;
         } else if (c != 1) {
            checksum = 0;
            state = SIG_LSB;
         }
         break;
      case   LEN_LSB:
         len = c;
         state = LEN_MSB;
         break;
      case   LEN_MSB:
         len |= c << 8;
         fprintf(stderr,  "len = %04x; ", len);
         len -= 6;
         state = ADDR_LSB;
         break;
      case  ADDR_LSB:
         addr = c;
         state = ADDR_MSB;
         break;
      case  ADDR_MSB:
         addr |= c << 8;
         fprintf(stderr,  "addr = %04x; ", addr);
         if (len == 0) {
            state = CHECKSUM;
         } else {
            state = DATA;
         }
         break;
      case DATA:
         len--;
         total++;
         // fprintf(stdout, "%c", c);
         copro_pdp11_write8(addr, c);
         last = addr;
         addr++;
         if (len == 0) {
            state = CHECKSUM;
         }
         break;
      case  CHECKSUM:
         fprintf(stderr, "checksum = %02x\n", checksum & 0xff);
         checksum = 0;
         state = SIG_LSB;
         break;
      }
   }
   fprintf(stderr, "total length = %d\n", total);
   return last;
}

char strbuf[1000];

void dump_state(cpu_debug_t *cpu) {
   const char **reg = cpu->reg_names;
   int i = 0;
   while (*reg) {
      cpu->reg_print(i, strbuf, sizeof(strbuf));
      printf("%8s = %s\r\n", *reg, &strbuf[0]);
      reg++;
      i++;
   }
}

void  main() {

   int i = 0;
   cpu_debug_t *cpu = &pdp11_cpu_debug;
   unsigned int memAddr = 0000;
   unsigned int startAddr = memAddr;
   unsigned int endAddr = loader();
   fprintf(stderr, "start = %06o, end = %06o\r\n", startAddr, endAddr);
   do {
      memAddr = cpu->disassemble(memAddr, strbuf, sizeof(strbuf));
      printf("%s\r\n", &strbuf[0]);
   } while (memAddr <= endAddr);


   pdp11_reset(0200);

   while (!m_pdp11->halted) {
#ifdef DEBUG
      dump_state(cpu);
      cpu->disassemble(m_pdp11->R[7], strbuf, sizeof(strbuf));
      printf("%s\r\n", &strbuf[0]);
#endif
      pdp11_execute();
      i++;
   }

   dump_state(cpu);
   for (memAddr = 0000; memAddr < 0400; memAddr += 2) {
      printf("%06o = %06o\r\n", memAddr, copro_pdp11_read16(memAddr));
   }
   printf("%d instructions\r\n", i);
}
