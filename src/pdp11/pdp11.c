#include <stdint.h>
#include <stdio.h>
#include <setjmp.h>
#include "pdp11.h"
#include "../tube.h"
#include "../copro-pdp11.h"

#ifdef INCLUDE_DEBUGGER
#include "pdp11_debug.h"
#include "../cpu_debug.h"
#endif

// Uncomment to allow unaligned (odd) word accesses to proceed
// Certain PDP-11 models work like this (e.g. the PDP 11/03 aka LSI-11)
// #define ALLOW_UNALIGNED


jmp_buf trapbuf;

enum {
   INTBUS    = 0004,
   INTINVAL  = 0010,
   INTDEBUG  = 0014,
   INTIOT    = 0020,
   INTTTYIN  = 0060,
   INTTTYOUT = 0064,
   INTFAULT  = 0250,
   INTCLOCK  = 0100,
   INTRK     = 0220
};

enum {
   DEBUG_INTER = false,
};

enum {
   FLAGN = 8,
   FLAGZ = 4,
   FLAGV = 2,
   FLAGC = 1
};

// Encapsulate the persistent CPU state
pdp11_state cpu;

pdp11_state *m_pdp11 = &cpu;

static bool N() { return cpu.PS & FLAGN; }

static bool Z() { return cpu.PS & FLAGZ; }

static bool V() { return cpu.PS & FLAGV; }

static bool C() { return cpu.PS & FLAGC; }

static uint16_t trap(uint16_t vec) {
   longjmp(trapbuf, vec);
   return vec; // not reached
}

static uint16_t read8(uint16_t a) {
   // read8(mmu::decode(a, false, curuser));
   return copro_pdp11_read8(a);
}

static uint16_t read16(uint16_t a) {
   // read16(mmu::decode(a, false, curuser));
   if (a & 1) {
#ifdef ALLOW_UNALIGNED
      printf("word read from odd address %06o (%04x) at %06o\r\n", a, a, cpu.PC);
      a &= ~1;
#else
      trap(INTBUS);
#endif
   }
   return copro_pdp11_read16(a);
}

static void write8(uint16_t a, const uint16_t v) {
   // write8(mmu::decode(a, true, curuser), v);
   copro_pdp11_write8(a, v);
}

static void write16(uint16_t a, const uint16_t v) {
   // write16(mmu::decode(a, true, curuser), v);
   if (a & 1) {
#ifdef ALLOW_UNALIGNED
      printf("word write to odd address %06o (%04x) at %06o\r\n", a, a, cpu.PC);
      a &= ~1;
#else
      trap(INTBUS);
#endif
   }
   copro_pdp11_write16(a, v);
}

void printstate() {
   printf("R0 %06o R1 %06o R2 %06o R3 %06o R4 %06o R5 %06o R6 %06o R7 %06o\r\n",
          (uint16_t)cpu.R[0], (uint16_t)cpu.R[1], (uint16_t)cpu.R[2],
          (uint16_t)cpu.R[3], (uint16_t)cpu.R[4], (uint16_t)cpu.R[5],
          (uint16_t)cpu.R[6], (uint16_t)cpu.R[7]);
   printf("[%s%s%s%s%s%s", cpu.prevuser ? "u" : "k", cpu.curuser ? "U" : "K",
          (cpu.PS & FLAGN) ? "N" : " ", (cpu.PS & FLAGZ) ? "Z" : " ",
          (cpu.PS & FLAGV) ? "V" : " ", (cpu.PS & FLAGC) ? "C" : " ");
   printf("]  instr %06o: %06o\t ", cpu.PC, read16(cpu.PC));
   printf("\r\n");
}

void panic() {
   cpu.halted = 1;
#ifndef TEST_MODE
   printstate();
   while (1);
#endif
}

static bool isReg(const uint16_t a) { return (a & 0177770) == 0170000; }

static uint16_t memread16(const uint16_t a) {
   if (isReg(a)) {
      return cpu.R[a & 7];
   }
   return read16(a);
}

static uint16_t memread(uint16_t a, uint8_t l) {
   if (isReg(a)) {
      const uint8_t r = a & 7;
      if (l == 2) {
         return cpu.R[r];
      } else {
         return cpu.R[r] & 0xFF;
      }
   }
   if (l == 2) {
      return read16(a);
   }
   return read8(a);
}

static void memwrite16(const uint16_t a, const uint16_t v) {
   if (isReg(a)) {
      cpu.R[a & 7] = v;
   } else {
      write16(a, v);
   }
}

static void memwrite(const uint16_t a, const uint8_t l, const uint16_t v) {
   if (isReg(a)) {
      const uint8_t r = a & 7;
      if (l == 2) {
         cpu.R[r] = v;
      } else {
         cpu.R[r] &= 0xFF00;
         cpu.R[r] |= v;
      }
      return;
   }
   if (l == 2) {
      write16(a, v);
   } else {
      write8(a, v);
   }
}

static uint16_t fetch16() {
   const uint16_t val = read16(cpu.R[7]);
   cpu.R[7] += 2;
   return val;
}

static void push(const uint16_t v) {
   cpu.R[6] -= 2;
   write16(cpu.R[6], v);
}

static uint16_t pop() {
   const uint16_t val = read16(cpu.R[6]);
   cpu.R[6] += 2;
   return val;
}

// aget resolves the operand to a vaddrescpu.
// if the operand is a register, an address in
// the range [0170000,0170007). This address range is
// technically a valid IO page, but unibus doesn't map
// any addresses here, so we can safely do thicpu.
static uint16_t aget(uint8_t v, uint8_t l) {
   if ((v & 070) == 000) {
      return 0170000 | (v & 7);
   }
   if (((v & 7) >= 6) || (v & 010)) {
      l = 2;
   }
   uint16_t addr = 0;
   switch (v & 060) {
   case 000:
      v &= 7;
      addr = cpu.R[v & 7];
      break;
   case 020:
      addr = cpu.R[v & 7];
      cpu.R[v & 7] += l;
      break;
   case 040:
      cpu.R[v & 7] -= l;
      addr = cpu.R[v & 7];
      break;
   case 060:
      addr = fetch16();
      addr += cpu.R[v & 7];
      break;
   }
   if (v & 010) {
      addr = read16(addr);
   }
   return addr;
}

static void branch(int16_t o) {
   if (o & 0x80) {
      o = -(((~o) + 1) & 0xFF);
   }
   o <<= 1;
   cpu.R[7] += o;
}

static void switchmode(const bool newm) {
   cpu.prevuser = cpu.curuser;
   cpu.curuser = newm;
   if (cpu.prevuser) {
      cpu.USP = cpu.R[6];
   } else {
      cpu.KSP = cpu.R[6];
   }
   if (cpu.curuser) {
      cpu.R[6] = cpu.USP;
   } else {
      cpu.R[6] = cpu.KSP;
   }
   cpu.PS &= 0007777;
   if (cpu.curuser) {
      cpu.PS |= (1 << 15) | (1 << 14);
   }
   if (cpu.prevuser) {
      cpu.PS |= (1 << 13) | (1 << 12);
   }
}
static void setZ(bool b) {
   if (b)
      cpu.PS |= FLAGZ;
}

static void MOV(const uint16_t instr) {
   const uint8_t d = instr & 077;
   const uint8_t s = (instr & 07700) >> 6;
   uint8_t l = 2 - (instr >> 15);
   const uint16_t msb = l == 2 ? 0x8000 : 0x80;
   uint16_t uval = memread(aget(s, l), l);
   const uint16_t da = aget(d, l);
   cpu.PS &= 0xFFF1;
   if (uval & msb) {
      cpu.PS |= FLAGN;
   }
   setZ(uval == 0);
   if ((isReg(da)) && (l == 1)) {
      l = 2;
      if (uval & msb) {
         uval |= 0xFF00;
      }
   }
   memwrite(da, l, uval);
}

static void CMP(uint16_t instr) {
   const uint8_t d = instr & 077;
   const uint8_t s = (instr & 07700) >> 6;
   const uint8_t l = 2 - (instr >> 15);
   const uint16_t msb = l == 2 ? 0x8000 : 0x80;
   const uint16_t max = l == 2 ? 0xFFFF : 0xff;
   uint16_t val1 = memread(aget(s, l), l);
   const uint16_t da = aget(d, l);
   uint16_t val2 = memread(da, l);
   const int32_t sval = (val1 - val2) & max;
   cpu.PS &= 0xFFF0;
   setZ(sval == 0);
   if (sval & msb) {
      cpu.PS |= FLAGN;
   }
   if (((val1 ^ val2) & msb) && (!((val2 ^ sval) & msb))) {
      cpu.PS |= FLAGV;
   }
   if (val1 < val2) {
      cpu.PS |= FLAGC;
   }
}

static void BIT(uint16_t instr) {
   const uint8_t d = instr & 077;
   const uint8_t s = (instr & 07700) >> 6;
   const uint8_t l = 2 - (instr >> 15);
   const uint16_t msb = l == 2 ? 0x8000 : 0x80;
   const uint16_t val1 = memread(aget(s, l), l);
   const uint16_t da = aget(d, l);
   const uint16_t val2 = memread(da, l);
   const uint16_t uval = val1 & val2;
   cpu.PS &= 0xFFF1;
   setZ(uval == 0);
   if (uval & msb) {
      cpu.PS |= FLAGN;
   }
}

static void BIC(uint16_t instr) {
   const uint8_t d = instr & 077;
   const uint8_t s = (instr & 07700) >> 6;
   const uint8_t l = 2 - (instr >> 15);
   const uint16_t msb = l == 2 ? 0x8000 : 0x80;
   const uint16_t max = l == 2 ? 0xFFFF : 0xff;
   const uint16_t val1 = memread(aget(s, l), l);
   const uint16_t da = aget(d, l);
   const uint16_t val2 = memread(da, l);
   const uint16_t uval = (max ^ val1) & val2;
   cpu.PS &= 0xFFF1;
   setZ(uval == 0);
   if (uval & msb) {
      cpu.PS |= FLAGN;
   }
   memwrite(da, l, uval);
}

static void BIS(uint16_t instr) {
   uint8_t d = instr & 077;
   uint8_t s = (instr & 07700) >> 6;
   uint8_t l = 2 - (instr >> 15);
   uint16_t msb = l == 2 ? 0x8000 : 0x80;
   uint16_t val1 = memread(aget(s, l), l);
   uint16_t da = aget(d, l);
   uint16_t val2 = memread(da, l);
   uint16_t uval = val1 | val2;
   cpu.PS &= 0xFFF1;
   setZ(uval == 0);
   if (uval & msb) {
      cpu.PS |= FLAGN;
   }
   memwrite(da, l, uval);
}

static void ADD(uint16_t instr) {
   uint8_t d = instr & 077;
   uint8_t s = (instr & 07700) >> 6;
   uint16_t val1 = memread16(aget(s, 2));
   uint16_t da = aget(d, 2);
   uint16_t val2 = memread16(da);
   uint16_t uval = (val1 + val2) & 0xFFFF;
   cpu.PS &= 0xFFF0;
   setZ(uval == 0);
   if (uval & 0x8000) {
      cpu.PS |= FLAGN;
   }
   if (!((val1 ^ val2) & 0x8000) && ((val2 ^ uval) & 0x8000)) {
      cpu.PS |= FLAGV;
   }
   if ((val1 + val2) > 0xFFFF) {
      cpu.PS |= FLAGC;
   }
   memwrite16(da, uval);
}

static void SUB(uint16_t instr) {
   uint8_t d = instr & 077;
   uint8_t s = (instr & 07700) >> 6;
   uint16_t val1 = memread16(aget(s, 2));
   uint16_t da = aget(d, 2);
   uint16_t val2 = memread16(da);
   uint16_t uval = (val2 - val1) & 0xFFFF;
   cpu.PS &= 0xFFF0;
   setZ(uval == 0);
   if (uval & 0x8000) {
      cpu.PS |= FLAGN;
   }
   if (((val1 ^ val2) & 0x8000) && (!((val1 ^ uval) & 0x8000))) {
      cpu.PS |= FLAGV;
   }
   if (val1 > val2) {
      cpu.PS |= FLAGC;
   }
   memwrite16(da, uval);
}

static void JSR(uint16_t instr) {
   uint8_t d = instr & 077;
   uint8_t s = (instr & 07700) >> 6;
   uint8_t l = 2 - (instr >> 15);
   uint16_t uval = aget(d, l);
   if (isReg(uval)) {
      printf("JSR called on registeri\r\n");
      trap(INTBUS);
   }
   push(cpu.R[s & 7]);
   cpu.R[s & 7] = cpu.R[7];
   cpu.R[7] = uval;
}

static void MUL(const uint16_t instr) {
   uint8_t d = instr & 077;
   uint8_t s = (instr & 07700) >> 6;
   int32_t val1 = cpu.R[s & 7];
   if (val1 & 0x8000) {
      val1 = -((0xFFFF ^ val1) + 1);
   }
   uint8_t l = 2 - (instr >> 15);
   uint16_t da = aget(d, l);
   int32_t val2 = memread16(da);
   if (val2 & 0x8000) {
      val2 = -((0xFFFF ^ val2) + 1);
   }
   int32_t sval = val1 * val2;
   cpu.R[s & 7] = sval >> 16;
   cpu.R[(s & 7) | 1] = sval & 0xFFFF;
   cpu.PS &= 0xFFF0;
   if (sval & 0x80000000) {
      cpu.PS |= FLAGN;
   }
   setZ((sval & 0xFFFFFFFF) == 0);
   if ((sval < -(1 << 15)) || (sval >= ((1 << 15) - 1))) {
      cpu.PS |= FLAGC;
   }
}

static void DIV(uint16_t instr) {
   uint8_t d = instr & 077;
   uint8_t s = (instr & 07700) >> 6;
   int32_t val1 = (cpu.R[s & 7] << 16) | (cpu.R[(s & 7) | 1]);
   uint8_t l = 2 - (instr >> 15);
   uint16_t da = aget(d, l);
   int32_t val2 = memread16(da);
   int32_t quo;
   int32_t rem;
   cpu.PS &= 0xFFF0;
   if (val2 == 0) {
      // divide by zero
      cpu.PS |= FLAGC;
      cpu.PS |= FLAGV;
      cpu.PS |= FLAGZ; // J11,11/70 compat
      return;
   }
   if (val2 == 0xffff && val1 == -2147483648) {
      // divide largest negative integer by -1
      cpu.PS |= FLAGV; // J11,11/70 compat
      return;
   }
   // sign extend val2
   if (val2 & 0x8000) {
      val2 |= 0xffff0000;
   }
   quo = val1 / val2;
   if (quo < 0) {
      cpu.PS |= FLAGN;
   }
   if (quo < -0x8000 || quo > 0x7fff) {
      cpu.PS |= FLAGV;  // J11,11/70 compat
   } else {
      rem = val1 - quo * val2;
      cpu.R[s & 7] = quo & 0xFFFF;
      cpu.R[(s & 7) | 1] = rem & 0xFFFF;
      setZ(cpu.R[s & 7] == 0);
   }
}

static void ASH(uint16_t instr) {
   uint8_t d = instr & 077;
   uint8_t s = (instr & 07700) >> 6;
   uint16_t val1 = cpu.R[s & 7];
   uint16_t da = aget(d, 2);
   uint16_t val2 = memread16(da) & 077;
   cpu.PS &= 0xFFF0;
   int32_t sval;
   if (val2 & 040) {
      // shift right, no overflow is possible
      val2 = (077 ^ val2) + 1;
      if (val1 & 0100000) {
         sval = 0xFFFF ^ (0xFFFF >> val2);
         sval |= val1 >> val2;
      } else {
         sval = val1 >> val2;
      }
      if (val1 & (1 << (val2 - 1))) {
         cpu.PS |= FLAGC;
      }
   } else {
      // shift left, overflow if *any* of the bits shifted out don't match the final sign
      sval = (val1 << val2) & 0xFFFF;
      // carry and overflow can only be set if the shift amount is non-zero
      if (val2 > 0) {
         if (val1 & (1 << (16 - val2))) {
            cpu.PS |= FLAGC;
         }
         if (val2 >= 16) {
            // if shifting 16 bits or more, any non zero value will cause overflow
            if (val1 != 0) {
               cpu.PS |= FLAGV;
            }
         } else {
            // calculate the bits that have been shifted out of the top
            uint16_t sovf1 = val1 >> (16 - val2);
            // if sval is negative, all the shifted bits should have been one, otherise overflow occurred
            // if sval is positive, all the shifted bits should have been zero, otherise overflow occurred
            uint16_t sovf2 = (sval & 0x8000) ? (1 << val2) - 1 : 0;
            if (sovf1 != sovf2) {
               cpu.PS |= FLAGV;
            }
         }
      }
   }
   cpu.R[s & 7] = sval;
   setZ(sval == 0);
   if (sval & 0100000) {
      cpu.PS |= FLAGN;
   }
}

static void ASHC(uint16_t instr) {
   uint8_t d = instr & 077;
   uint8_t s = (instr & 07700) >> 6;
   uint32_t val1 = cpu.R[s & 7] << 16 | cpu.R[(s & 7) | 1];
   uint16_t da = aget(d, 2);
   uint16_t val2 = memread16(da) & 077;
   cpu.PS &= 0xFFF0;
   uint32_t sval;
   if (val2 & 040) {
      // shift right, no overflow is possible
      val2 = (077 ^ val2) + 1;
      if (val2 == 32) {
         // special case shift right by 32, as this is undefined in C)
         if (val1 & 0x80000000) {
            sval = 0xFFFFFFFF;
         } else {
            sval = 0;
         }
      } else {
         if (val1 & 0x80000000) {
            sval = 0xFFFFFFFF ^ (0xFFFFFFFF >> val2);
            sval |= val1 >> val2;
         } else {
            sval = val1 >> val2;
         }
      }
      if (val1 & (1U << (val2 - 1))) {
         cpu.PS |= FLAGC;
      }
   } else {
      // shift left, overflow if *any* of the bits shifted out don't match the final sign
      sval = (val1 << val2) & 0xFFFFFFFF;
      // carry and overflow can only be set if the shift amount is non-zero
      if (val2 > 0) {
         if (val1 & (1U << (32 - val2))) {
            cpu.PS |= FLAGC;
         }
         // calculate the bits that have been shifted out of the top
         uint32_t sovf1 = val1 >> (32 - val2);
         // if sval is negative, all the shifted bits should have been one, otherise overflow occurred
         // if sval is positive, all the shifted bits should have been zero, otherise overflow occurred
         uint32_t sovf2 = (sval & 0x80000000) ? (1 << val2) - 1 : 0;
         if (sovf1 != sovf2) {
            cpu.PS |= FLAGV;
         }
      }
   }
   cpu.R[s & 7] = (sval >> 16) & 0xFFFF;
   cpu.R[(s & 7) | 1] = sval & 0xFFFF;
   setZ(sval == 0);
   if (sval & 0x80000000) {
      cpu.PS |= FLAGN;
   }
}

static void XOR(uint16_t instr) {
   const uint8_t d = instr & 077;
   const uint8_t s = (instr & 07700) >> 6;
   const uint16_t val1 = cpu.R[s & 7];
   const uint16_t da = aget(d, 2);
   const uint16_t val2 = memread16(da);
   const uint16_t uval = val1 ^ val2;
   cpu.PS &= 0xFFF1;
   setZ(uval == 0);
   if (uval & 0x8000) {
      cpu.PS |= FLAGN;
   }
   memwrite16(da, uval);
}

static void SOB(const uint16_t instr) {
   const uint8_t s = (instr & 07700) >> 6;
   uint8_t o = instr & 0xFF;
   cpu.R[s & 7] = (cpu.R[s & 7] - 1) & 0xffff;
   if (cpu.R[s & 7]) {
      o &= 077;
      o <<= 1;
      cpu.R[7] -= o;
   }
}

static void CLR(uint16_t instr) {
   const uint8_t d = instr & 077;
   const uint8_t l = 2 - (instr >> 15);
   cpu.PS &= 0xFFF0;
   cpu.PS |= FLAGZ;
   const uint16_t da = aget(d, l);
   memwrite(da, l, 0);
}

static void COM(uint16_t instr) {
   uint8_t d = instr & 077;
   uint8_t l = 2 - (instr >> 15);
   uint16_t msb = l == 2 ? 0x8000 : 0x80;
   uint16_t max = l == 2 ? 0xFFFF : 0xff;
   uint16_t da = aget(d, l);
   uint16_t uval = memread(da, l) ^ max;
   cpu.PS &= 0xFFF0;
   cpu.PS |= FLAGC;
   if (uval & msb) {
      cpu.PS |= FLAGN;
   }
   setZ(uval == 0);
   memwrite(da, l, uval);
}

static void INC(const uint16_t instr) {
   const uint8_t d = instr & 077;
   const uint8_t l = 2 - (instr >> 15);
   const uint16_t msb = l == 2 ? 0x8000 : 0x80;
   const uint16_t max = l == 2 ? 0xFFFF : 0xff;
   const uint16_t da = aget(d, l);
   const uint16_t uval = (memread(da, l) + 1) & max;
   cpu.PS &= 0xFFF1;
   if (uval & msb) {
      cpu.PS |= FLAGN;
   }
   if (uval == msb) {
      // Overflow set if dst = Max Positive Integer
      cpu.PS |= FLAGV;
   }
   setZ(uval == 0);
   memwrite(da, l, uval);
}

static void _DEC(uint16_t instr) {
   uint8_t d = instr & 077;
   uint8_t l = 2 - (instr >> 15);
   uint16_t msb = l == 2 ? 0x8000 : 0x80;
   uint16_t max = l == 2 ? 0xFFFF : 0xff;
   uint16_t maxp = l == 2 ? 0x7FFF : 0x7f;
   uint16_t da = aget(d, l);
   uint16_t uval = (memread(da, l) - 1) & max;
   cpu.PS &= 0xFFF1;
   if (uval & msb) {
      cpu.PS |= FLAGN;
   }
   if (uval == maxp) {
      cpu.PS |= FLAGV;
   }
   setZ(uval == 0);
   memwrite(da, l, uval);
}

static void NEG(uint16_t instr) {
   uint8_t d = instr & 077;
   uint8_t l = 2 - (instr >> 15);
   uint16_t msb = l == 2 ? 0x8000 : 0x80;
   uint16_t max = l == 2 ? 0xFFFF : 0xff;
   uint16_t da = aget(d, l);
   int32_t sval = (-memread(da, l)) & max;
   cpu.PS &= 0xFFF0;
   if (sval & msb) {
      cpu.PS |= FLAGN;
   }
   if (sval == 0) {
      cpu.PS |= FLAGZ;
   } else {
      cpu.PS |= FLAGC;
   }
   if (sval == msb) {
      cpu.PS |= FLAGV;
   }
   memwrite(da, l, sval);
}

static void _ADC(uint16_t instr) {
   uint8_t d = instr & 077;
   uint8_t l = 2 - (instr >> 15);
   uint16_t msb = l == 2 ? 0x8000 : 0x80;
   uint16_t max = l == 2 ? 0xFFFF : 0xff;
   uint16_t da = aget(d, l);
   uint16_t uval = memread(da, l);
   if (cpu.PS & FLAGC) {
      cpu.PS &= 0xFFF0;
      if ((uval + 1) & msb) {
         cpu.PS |= FLAGN;
      }
      setZ(uval == max);
      if (uval == msb - 1) {
         cpu.PS |= FLAGV;
      }
      if (uval == max) {
         cpu.PS |= FLAGC;
      }
      memwrite(da, l, (uval + 1) & max);
   } else {
      cpu.PS &= 0xFFF0;
      if (uval & msb) {
         cpu.PS |= FLAGN;
      }
      setZ(uval == 0);
   }
}

static void SBC(uint16_t instr) {
   uint8_t d = instr & 077;
   uint8_t l = 2 - (instr >> 15);
   uint16_t msb = l == 2 ? 0x8000 : 0x80;
   uint16_t max = l == 2 ? 0xFFFF : 0xff;
   uint16_t da = aget(d, l);
   int32_t sval = memread(da, l);
   if (cpu.PS & FLAGC) {
      cpu.PS &= 0xFFF0;
      if ((sval - 1) & msb) {
         cpu.PS |= FLAGN;
      }
      setZ((sval & max) == 1);
      if ((sval & max) == 0) {
         cpu.PS |= FLAGC;
      }
      if (sval == msb) {
         cpu.PS |= FLAGV;
      }
      memwrite(da, l, (sval - 1) & max);
   } else {
      cpu.PS &= 0xFFF0;
      if (sval & msb) {
         cpu.PS |= FLAGN;
      }
      setZ(sval == 0);
      if (sval == 0100000) {
         cpu.PS |= FLAGV;
      }
   }
}

static void TST(uint16_t instr) {
   uint8_t d = instr & 077;
   uint8_t l = 2 - (instr >> 15);
   uint16_t msb = l == 2 ? 0x8000 : 0x80;
   uint16_t uval = memread(aget(d, l), l);
   cpu.PS &= 0xFFF0;
   if (uval & msb) {
      cpu.PS |= FLAGN;
   }
   setZ(uval == 0);
}

static void ROR(uint16_t instr) {
   uint8_t d = instr & 077;
   uint8_t l = 2 - (instr >> 15);
   int32_t max = l == 2 ? 0xFFFF : 0xff;
   uint16_t da = aget(d, l);
   int32_t sval = memread(da, l);
   if (cpu.PS & FLAGC) {
      sval |= max + 1;
   }
   cpu.PS &= 0xFFF0;
   if (sval & 1) {
      cpu.PS |= FLAGC;
      cpu.PS ^= FLAGV;
   }
   // watch out for integer wrap around
   if (sval & (max + 1)) {
      cpu.PS |= FLAGN;
      cpu.PS ^= FLAGV;
   }
   sval >>= 1;
   setZ(!(sval & max));
   memwrite(da, l, sval);
}

static void ROL(uint16_t instr) {
   uint8_t d = instr & 077;
   uint8_t l = 2 - (instr >> 15);
   uint16_t msb = l == 2 ? 0x8000 : 0x80;
   int32_t max = l == 2 ? 0xFFFF : 0xff;
   uint16_t da = aget(d, l);
   int32_t sval = memread(da, l) << 1;
   if (cpu.PS & FLAGC) {
      sval |= 1;
   }
   cpu.PS &= 0xFFF0;
   if (sval & (max + 1)) {
      cpu.PS |= FLAGC;
   }
   if (sval & msb) {
      cpu.PS |= FLAGN;
   }
   setZ(!(sval & max));
   if ((sval ^ (sval >> 1)) & msb) {
      cpu.PS |= FLAGV;
   }
   sval &= max;
   memwrite(da, l, sval);
}

static void ASR(uint16_t instr) {
   uint8_t d = instr & 077;
   uint8_t l = 2 - (instr >> 15);
   uint16_t msb = l == 2 ? 0x8000 : 0x80;
   uint16_t da = aget(d, l);
   uint16_t uval = memread(da, l);
   cpu.PS &= 0xFFF0;
   if (uval & 1) {
      cpu.PS |= FLAGC;
      cpu.PS ^= FLAGV;
   }
   if (uval & msb) {
      cpu.PS |= FLAGN;
      cpu.PS ^= FLAGV;
   }
   uval = (uval & msb) | (uval >> 1);
   setZ(uval == 0);
   memwrite(da, l, uval);
}

static void ASL(uint16_t instr) {
   uint8_t d = instr & 077;
   uint8_t l = 2 - (instr >> 15);
   uint16_t msb = l == 2 ? 0x8000 : 0x80;
   uint16_t max = l == 2 ? 0xFFFF : 0xff;
   uint16_t da = aget(d, l);
   // TODO(dfc) doesn't need to be an sval
   int32_t sval = memread(da, l);
   cpu.PS &= 0xFFF0;
   if (sval & msb) {
      cpu.PS |= FLAGC;
   }
   if (sval & (msb >> 1)) {
      cpu.PS |= FLAGN;
   }
   if ((sval ^ (sval << 1)) & msb) {
      cpu.PS |= FLAGV;
   }
   sval = (sval << 1) & max;
   setZ(sval == 0);
   memwrite(da, l, sval);
}

static void SXT(uint16_t instr) {
   uint8_t d = instr & 077;
   uint8_t l = 2 - (instr >> 15);
   uint16_t max = l == 2 ? 0xFFFF : 0xff;
   uint16_t da = aget(d, l);
   cpu.PS &= ~(FLAGV | FLAGZ);
   if (cpu.PS & FLAGN) {
      memwrite(da, l, max);
   } else {
      cpu.PS |= FLAGZ;
      memwrite(da, l, 0);
   }
}

static void JMP(uint16_t instr) {
   uint8_t d = instr & 077;
   uint16_t uval = aget(d, 2);
   if (isReg(uval)) {
      printf("JMP called with register dest\r\n");
      trap(INTBUS);
   }
   cpu.R[7] = uval;
}

static void SWAB(uint16_t instr) {
   uint8_t d = instr & 077;
   uint8_t l = 2 - (instr >> 15);
   uint16_t da = aget(d, l);
   uint16_t uval = memread(da, l);
   uval = ((uval >> 8) | (uval << 8)) & 0xFFFF;
   cpu.PS &= 0xFFF0;
   setZ((uval & 0xFF) == 0);
   if (uval & 0x80) {
      cpu.PS |= FLAGN;
   }
   memwrite(da, l, uval);
}

static void MARK(uint16_t instr) {
   cpu.R[6] = cpu.R[7] + ((instr & 077) << 1);
   cpu.R[7] = cpu.R[5];
   cpu.R[5] = pop();
}

static void MFPI(uint16_t instr) {
   uint8_t d = instr & 077;
   uint16_t da = aget(d, 2);
   uint16_t uval;
   if (da == 0170006) {
      // val = (curuser == prevuser) ? cpu.R[6] : (prevuser ? k.cpu.USP : cpu.KSP);
      if (cpu.curuser == cpu.prevuser) {
         uval = cpu.R[6];
      } else {
         if (cpu.prevuser) {
            uval = cpu.USP;
         } else {
            uval = cpu.KSP;
         }
      }
   } else if (isReg(da)) {
      printf("invalid MFPI instruction\r\n");
      trap(INTBUS);
      return; // unreached
   } else {
      uval = read16((uint16_t)da);
   }
   push(uval);
   cpu.PS &= 0xFFF1;
   setZ(uval == 0);
   if (uval & 0x8000) {
      cpu.PS |= FLAGN;
   }
}

static void MTPI(uint16_t instr) {
   uint8_t d = instr & 077;
   uint16_t da = aget(d, 2);
   uint16_t uval = pop();
   if (da == 0170006) {
      if (cpu.curuser == cpu.prevuser) {
         cpu.R[6] = uval;
      } else {
         if (cpu.prevuser) {
            cpu.USP = uval;
         } else {
            cpu.KSP = uval;
         }
      }
   } else if (isReg(da)) {
      printf("invalid MTPI instrution\r\n");
      trap(INTBUS);
   } else {
      write16((uint16_t)da, uval);
   }
   cpu.PS &= 0xFFF1;
   setZ(uval == 0);
   if (uval & 0x8000) {
      cpu.PS |= FLAGN;
   }
}

static void MTPS(uint16_t instr) {
   const uint8_t s = instr & 077;
   uint16_t uval = memread(aget(s, 1), 1) & 0x00EF;
   cpu.PS &= 0xff00;
   cpu.PS |= uval;
}

static void MFPS(uint16_t instr) {
   int l = 1;
   const uint8_t d = instr & 077;
   const uint16_t da = aget(d, l);
   uint16_t uval = cpu.PS & 0xff;
   cpu.PS &= 0xff01;
   setZ(uval == 0);
   if (uval & 0x80) {
      cpu.PS |= FLAGN;
   }
   if (isReg(da)) {
      l = 2;
      if (uval & 0x80) {
         uval |= 0xFF00;
      }
   }
   memwrite(da, l, uval);
}

static void RTS(uint16_t instr) {
   uint8_t d = instr & 077;
   cpu.R[7] = cpu.R[d & 7];
   cpu.R[d & 7] = pop();
}

static void SPL(uint16_t instr) {
   if (!cpu.curuser) {
      // only work in kernel mode, NOP in user mode (no trap occurs)
      cpu.PS &= 0xff1f;
      cpu.PS |= (instr & 7) << 5;
   }
}

static void EMTX(uint16_t instr) {
   uint16_t uval;
   if ((instr & 0177400) == 0104000) {
      uval = 030;
   } else if ((instr & 0177400) == 0104400) {
      uval = 034;
   } else if (instr == 3) {
      uval = 014;
   } else {
      uval = 020;
   }
   uint16_t prev = cpu.PS;
   switchmode(false);
   push(prev);
   push(cpu.R[7]);
   cpu.R[7] = read16(uval);
   cpu.PS = read16(uval + 2);
   if (cpu.prevuser) {
      cpu.PS |= (1 << 13) | (1 << 12);
   }
}

static void _RTT(uint16_t instr) {
   cpu.R[7] = pop();
   uint16_t uval = pop();
   if (cpu.curuser) {
      uval &= 047;
      uval |= cpu.PS & 0177730;
   }
   // In some PDP-11's the PSW is memory mapped, but not for now in ours
   // write16(0777776, uval);
   switch (uval >> 14) {
   case 0:
      switchmode(false);
      break;
   case 3:
      switchmode(true);
      break;
   default:
      printf("invalid mode\n");
      panic();
   }
   switch ((uval >> 12) & 3) {
   case 0:
      cpu.prevuser = false;
      break;
   case 3:
      cpu.prevuser = true;
      break;
   default:
      printf("invalid mode\n");
      panic();
   }
   cpu.PS = uval;
   return;
}

static void RESET(uint16_t instr) {
   if (cpu.curuser) {
      return;
   }
   // cons::clearterminal();
   //rk11::reset();
}

static void step() {
   cpu.PC = cpu.R[7];

#ifdef INCLUDE_DEBUGGER
      if (pdp11_debug_enabled) {
         debug_preexec(&pdp11_cpu_debug, cpu.PC);
      }
#endif

   uint16_t instr = read16(cpu.PC);
   cpu.R[7] += 2;

   switch ((instr >> 12) & 007) {
   case 001: // MOV
      MOV(instr);
      return;
   case 002: // CMP
      CMP(instr);
      return;
   case 003: // BIT
      BIT(instr);
      return;
   case 004: // BIC
      BIC(instr);
      return;
   case 005: // BIS
      BIS(instr);
      return;
   }
   switch ((instr >> 12) & 017) {
   case 006: // ADD
      ADD(instr);
      return;
   case 016: // SUB
      SUB(instr);
      return;
   }
   switch ((instr >> 9) & 0177) {
   case 0004: // JSR
      JSR(instr);
      return;
   case 0070: // MUL
      MUL(instr);
      return;
   case 0071: // DIV
      DIV(instr);
      return;
   case 0072: // ASH
      ASH(instr);
      return;
   case 0073: // ASHC
      ASHC(instr);
      return;
   case 0074: // XOR
      XOR(instr);
      return;
   case 0077: // SOB
      SOB(instr);
      return;
   }
   switch ((instr >> 6) & 00777) {
   case 00050: // CLR
      CLR(instr);
      return;
   case 00051: // COM
      COM(instr);
      return;
   case 00052: // INC
      INC(instr);
      return;
   case 00053: // DEC
      _DEC(instr);
      return;
   case 00054: // NEG
      NEG(instr);
      return;
   case 00055: // ADC
      _ADC(instr);
      return;
   case 00056: // SBC
      SBC(instr);
      return;
   case 00057: // TST
      TST(instr);
      return;
   case 00060: // ROR
      ROR(instr);
      return;
   case 00061: // ROL
      ROL(instr);
      return;
   case 00062: // ASR
      ASR(instr);
      return;
   case 00063: // ASL
      ASL(instr);
      return;
   }
   switch (instr & 0177700) {
   case 0000100: // JMP
      JMP(instr);
      return;
   case 0000300: // SWAB
      SWAB(instr);
      return;
   case 0006400: // MARK
      MARK(instr);
      return;
   case 0006500: // MFPI
      MFPI(instr);
      return;
   case 0006600: // MTPI
      MTPI(instr);
      return;
   case 0006700: // SXT
      SXT(instr);
      return;
   case 0106400: // MTPS
      MTPS(instr);
      return;
   case 0106700: // MFPS
      MFPS(instr);
      return;
   }
   if ((instr & 0177770) == 0000200) { // RTS
      RTS(instr);
      return;
   }
   if ((instr & 0177770) == 0000230) { // SPL
      SPL(instr);
      return;
   }
   switch (instr & 0177400) {
   case 0000400:
      branch(instr & 0xFF);
      return;
   case 0001000:
      if (!Z()) {
         branch(instr & 0xFF);
      }
      return;
   case 0001400:
      if (Z()) {
         branch(instr & 0xFF);
      }
      return;
   case 0002000:
      if (!((!N()) xor (!V()))) {
         branch(instr & 0xFF);
      }
      return;
   case 0002400:
      if ((!N()) xor (!V())) {
         branch(instr & 0xFF);
      }
      return;
   case 0003000:
      if ((!((!N()) xor (!V()))) && (!Z())) {
         branch(instr & 0xFF);
      }
      return;
   case 0003400:
      if (((!N()) xor (!V())) || Z()) {
         branch(instr & 0xFF);
      }
      return;
   case 0100000:
      if (!N()) {
         branch(instr & 0xFF);
      }
      return;
   case 0100400:
      if (N()) {
         branch(instr & 0xFF);
      }
      return;
   case 0101000:
      if ((!C()) && (!Z())) {
         branch(instr & 0xFF);
      }
      return;
   case 0101400:
      if (C() || Z()) {
         branch(instr & 0xFF);
      }
      return;
   case 0102000:
      if (!V()) {
         branch(instr & 0xFF);
      }
      return;
   case 0102400:
      if (V()) {
         branch(instr & 0xFF);
      }
      return;
   case 0103000:
      if (!C()) {
         branch(instr & 0xFF);
      }
      return;
   case 0103400:
      if (C()) {
         branch(instr & 0xFF);
      }
      return;
   }
   if (((instr & 0177000) == 0104000) || (instr == 3) ||
       (instr == 4)) { // EMT TRAP IOT BPT
      EMTX(instr);
      return;
   }
   if ((instr & 0177740) == 0240) { // CL?, SE?
      if (instr & 020) {
         cpu.PS |= instr & 017;
      } else {
         cpu.PS &= ~(instr & 017);
      }
      return;
   }
   switch (instr & 7) {
   case 00: // HALT
      if (cpu.curuser) {
         break;
      }
      printf("HALT\r\n");
      panic();
      return;
   case 01: // WAIT
      if (cpu.curuser) {
         break;
      }
      return;
   case 02: // RTI

   case 06: // RTT
      _RTT(instr);
      return;
   case 05: // RESET
      RESET(instr);
      return;
   }
   if (instr ==
       0170011) { // SETD ; not needed by UNIX, but used; therefore ignored
      return;
   }
   printf("invalid instruction\r\n");
   trap(INTINVAL);
}

static void trapat(uint16_t vec) { // , msg string) {
   if (vec & 1) {
      printf("Thou darst calling trapat() with an odd vector number?\r\n");
      panic();
   }
   printf("trap: %x\r\n", vec);

   /*var prev uint16
     defer func() {
     t = recover()
     switch t = t.(type) {
     case trap:
     writedebug("red stack trap!\r\n")
     memory[0] = uint16(k.cpu.R[7])
     memory[1] = prev
     vec = 4
     panic("fatal")
     case nil:
     break
     default:
     panic(t)
     }
   */
   uint16_t prev = cpu.PS;
   switchmode(false);
   push(prev);
   push(cpu.R[7]);

   cpu.R[7] = read16(vec);
   cpu.PS = read16(vec + 2);
   if (cpu.prevuser) {
      cpu.PS |= (1 << 13) | (1 << 12);
   }
}

// pop the top interrupt off the itab.
static void popirq() {
   uint8_t i;
   for (i = 0; i < ITABN - 1; i++) {
      cpu.itab[i] = cpu.itab[i + 1];
   }
   cpu.itab[ITABN - 1].vec = 0;
   cpu.itab[ITABN - 1].pri = 0;
}

static void handleinterrupt() {
   uint8_t vec = cpu.itab[0].vec;
   if (DEBUG_INTER) {
      printf("IRQ: %x\r\n", vec);
   }
   uint16_t vv = setjmp(trapbuf);
   if (vv == 0) {
      uint16_t prev = cpu.PS;
      switchmode(false);
      push(prev);
      push(cpu.R[7]);
   } else {
      trapat(vv);
   }

   cpu.R[7] = read16(vec);
   cpu.PS = read16(vec + 2);
   if (cpu.prevuser) {
      cpu.PS |= (1 << 13) | (1 << 12);
   }
   popirq();
}

void pdp11_reset(uint16_t address) {
   cpu.LKS = 1 << 7;
   cpu.R[7] = address;
   cpu.PS = 0;
   cpu.halted = 0;
   uint8_t i;
   for (i = 0; i < ITABN; i++) {
      cpu.itab[i].vec = 0;
      cpu.itab[i].pri = 0;
   }
}

void pdp11_interrupt(uint8_t vec, uint8_t pri) {
   if (vec & 1) {
      printf("Thou darst calling interrupt() with an odd vector number?\r\n");
      panic();
   }
   // fast path
   if (cpu.itab[0].vec == 0) {
      cpu.itab[0].vec = vec;
      cpu.itab[0].pri = pri;
      if (DEBUG_INTER) {
         printf("itab[0] = %04x, %d\r\n", vec, pri);
      }
      return;
   }
   uint8_t i;
   for (i = 0; i < ITABN; i++) {
      if ((cpu.itab[i].vec == 0) || (cpu.itab[i].pri < pri)) {
         break;
      }
   }
   for (; i < ITABN; i++) {
      if ((cpu.itab[i].vec == 0) || (cpu.itab[i].vec >= vec)) {
         break;
      }
   }
   if (i >= ITABN) {
      printf("interrupt table full\r\n");
      panic();
   }
   uint8_t j;
   for (j = ITABN - 1; j >= i + 1; j--) {
      cpu.itab[j] = cpu.itab[j - 1];
   }
   cpu.itab[i].vec = vec;
   cpu.itab[i].pri = pri;
   if (DEBUG_INTER) {
      printf("itab[%d] = %04x, %d\r\n", i, vec, pri);
   }
}

static void loop0() {
   do {
      if ((cpu.itab[0].vec > 0) && (cpu.itab[0].pri >= ((cpu.PS >> 5) & 7))) {
         handleinterrupt();
         uint8_t i;
         for (i = 0; i < ITABN - 1; i++) {
            cpu.itab[i] = cpu.itab[i + 1];
         }
         cpu.itab[ITABN - 1].vec = 0;
         cpu.itab[ITABN - 1].pri = 0;
         return; // exit from loop to reset trapbuf
      }
      step();
#if 0
      if (++cpu.clkcounter > 39999) {
         cpu.clkcounter = 0;
         cpu.LKS |= (1 << 7);
         if (cpu.LKS & (1 << 6)) {
            pdp11_interrupt(INTCLOCK, 6);
         }
      }
      cons::poll();
#endif
   } while (tubeContinueRunning());
}

void pdp11_execute() {
   uint16_t vec = setjmp(trapbuf);
   if (vec == 0) {
      loop0();
   } else {
      trapat(vec);
   }
}

void pdp11_switchmode(const bool newm) {
   switchmode(newm);
}
