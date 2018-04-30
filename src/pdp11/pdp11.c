#include <stdint.h>
#include <stdio.h>
#include <setjmp.h>
#include "../tube.h"
#include "../copro-pdp11.h"

jmp_buf trapbuf;

#define false  0
#define true   1
#define bool int
#define xor    ^

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
   PRINTSTATE = false
};

enum {
   FLAGN = 8,
   FLAGZ = 4,
   FLAGV = 2,
   FLAGC = 1
};

#define ITABN 8

typedef struct {
   uint8_t vec;
   uint8_t pri;
} intr;

uint16_t clkcounter;

intr itab[ITABN];

// signed integer registers
int32_t R[8];

uint16_t PS;       // processor status
uint16_t PC;       // address of current instruction
uint16_t KSP, USP; // kernel and user stack pointer
uint16_t LKS;
bool curuser, prevuser;

static bool N() { return PS & FLAGN; }

static bool Z() { return PS & FLAGZ; }

static bool V() { return PS & FLAGV; }

static bool C() { return PS & FLAGC; }

static uint16_t read8(const uint16_t a) {
   // read8(mmu::decode(a, false, curuser));
   return copro_pdp11_read8(a);
}

static uint16_t read16(const uint16_t a) {
   // read16(mmu::decode(a, false, curuser));
   return copro_pdp11_read16(a);
}

static void write8(const uint16_t a, const uint16_t v) {
   // write8(mmu::decode(a, true, curuser), v);
   copro_pdp11_write8(a, v);
}

static void write16(const uint16_t a, const uint16_t v) {
   // write16(mmu::decode(a, true, curuser), v);
   copro_pdp11_write16(a, v);
}

void printstate() {
   printf("R0 %06o R1 %06o R2 %06o R3 %06o R4 %06o R5 %06o R6 %06o R7 %06o\r\n",
          (uint16_t)R[0], (uint16_t)R[1], (uint16_t)R[2],
          (uint16_t)R[3], (uint16_t)R[4], (uint16_t)R[5],
          (uint16_t)R[6], (uint16_t)R[7]);
   printf("[%s%s%s%s%s%s", prevuser ? "u" : "k", curuser ? "U" : "K",
          PS & FLAGN ? "N" : " ", PS & FLAGZ ? "Z" : " ",
          PS & FLAGV ? "V" : " ", PS & FLAGC ? "C" : " ");
   printf("]  instr %06o: %06o\t ", PC, read16(PC));
   //disasm(mmu::decode(PC, false, curuser));
}

void panic() {
   printstate();
   while (1);
}

static uint16_t trap(uint16_t vec) {
   longjmp(trapbuf, INTBUS);
   return vec; // not reached
}

static bool isReg(const uint16_t a) { return (a & 0177770) == 0170000; }

static uint16_t memread16(const uint16_t a) {
   if (isReg(a)) {
      return R[a & 7];
   }
   return read16(a);
}

static uint16_t memread(uint16_t a, uint8_t l) {
   if (isReg(a)) {
      const uint8_t r = a & 7;
      if (l == 2) {
         return R[r];
      } else {
         return R[r] & 0xFF;
      }
   }
   if (l == 2) {
      return read16(a);
   }
   return read8(a);
}

static void memwrite16(const uint16_t a, const uint16_t v) {
   if (isReg(a)) {
      R[a & 7] = v;
   } else {
      write16(a, v);
   }
}

static void memwrite(const uint16_t a, const uint8_t l, const uint16_t v) {
   if (isReg(a)) {
      const uint8_t r = a & 7;
      if (l == 2) {
         R[r] = v;
      } else {
         R[r] &= 0xFF00;
         R[r] |= v;
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
   const uint16_t val = read16(R[7]);
   R[7] += 2;
   return val;
}

static void push(const uint16_t v) {
   R[6] -= 2;
   write16(R[6], v);
}

static uint16_t pop() {
   const uint16_t val = read16(R[6]);
   R[6] += 2;
   return val;
}

// aget resolves the operand to a vaddress.
// if the operand is a register, an address in
// the range [0170000,0170007). This address range is
// technically a valid IO page, but unibus doesn't map
// any addresses here, so we can safely do this.
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
      addr = R[v & 7];
      break;
   case 020:
      addr = R[v & 7];
      R[v & 7] += l;
      break;
   case 040:
      R[v & 7] -= l;
      addr = R[v & 7];
      break;
   case 060:
      addr = fetch16();
      addr += R[v & 7];
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
   R[7] += o;
}

void switchmode(const bool newm) {
   prevuser = curuser;
   curuser = newm;
   if (prevuser) {
      USP = R[6];
   } else {
      KSP = R[6];
   }
   if (curuser) {
      R[6] = USP;
   } else {
      R[6] = KSP;
   }
   PS &= 0007777;
   if (curuser) {
      PS |= (1 << 15) | (1 << 14);
   }
   if (prevuser) {
      PS |= (1 << 13) | (1 << 12);
   }
}

static void setZ(bool b) {
   if (b)
      PS |= FLAGZ;
}

static void MOV(const uint16_t instr) {
   const uint8_t d = instr & 077;
   const uint8_t s = (instr & 07700) >> 6;
   uint8_t l = 2 - (instr >> 15);
   const uint16_t msb = l == 2 ? 0x8000 : 0x80;
   uint16_t uval = memread(aget(s, l), l);
   const uint16_t da = aget(d, l);
   PS &= 0xFFF1;
   if (uval & msb) {
      PS |= FLAGN;
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
   PS &= 0xFFF0;
   setZ(sval == 0);
   if (sval & msb) {
      PS |= FLAGN;
   }
   if (((val1 ^ val2) & msb) && (!((val2 ^ sval) & msb))) {
      PS |= FLAGV;
   }
   if (val1 < val2) {
      PS |= FLAGC;
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
   PS &= 0xFFF1;
   setZ(uval == 0);
   if (uval & msb) {
      PS |= FLAGN;
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
   PS &= 0xFFF1;
   setZ(uval == 0);
   if (uval & msb) {
      PS |= FLAGN;
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
   PS &= 0xFFF1;
   setZ(uval == 0);
   if (uval & msb) {
      PS |= FLAGN;
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
   PS &= 0xFFF0;
   setZ(uval == 0);
   if (uval & 0x8000) {
      PS |= FLAGN;
   }
   if (!((val1 ^ val2) & 0x8000) && ((val2 ^ uval) & 0x8000)) {
      PS |= FLAGV;
   }
   if ((val1 + val2) >= 0xFFFF) {
      PS |= FLAGC;
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
   PS &= 0xFFF0;
   setZ(uval == 0);
   if (uval & 0x8000) {
      PS |= FLAGN;
   }
   if (((val1 ^ val2) & 0x8000) && (!((val2 ^ uval) & 0x8000))) {
      PS |= FLAGV;
   }
   if (val1 > val2) {
      PS |= FLAGC;
   }
   memwrite16(da, uval);
}

static void JSR(uint16_t instr) {
   uint8_t d = instr & 077;
   uint8_t s = (instr & 07700) >> 6;
   uint8_t l = 2 - (instr >> 15);
   uint16_t uval = aget(d, l);
   if (isReg(uval)) {
      printf("JSR called on registeri\n");
      panic();
   }
   push(R[s & 7]);
   R[s & 7] = R[7];
   R[7] = uval;
}

static void MUL(const uint16_t instr) {
   uint8_t d = instr & 077;
   uint8_t s = (instr & 07700) >> 6;
   int32_t val1 = R[s & 7];
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
   R[s & 7] = sval >> 16;
   R[(s & 7) | 1] = sval & 0xFFFF;
   PS &= 0xFFF0;
   if (sval & 0x80000000) {
      PS |= FLAGN;
   }
   setZ((sval & 0xFFFFFFFF) == 0);
   if ((sval < (1 << 15)) || (sval >= ((1 << 15) - 1))) {
      PS |= FLAGC;
   }
}

static void DIV(uint16_t instr) {
   uint8_t d = instr & 077;
   uint8_t s = (instr & 07700) >> 6;
   int32_t val1 = (R[s & 7] << 16) | (R[(s & 7) | 1]);
   uint8_t l = 2 - (instr >> 15);
   uint16_t da = aget(d, l);
   int32_t val2 = memread16(da);
   PS &= 0xFFF0;
   if (val2 == 0) {
      PS |= FLAGC;
      return;
   }
   if ((val1 / val2) >= 0x10000) {
      PS |= FLAGV;
      return;
   }
   R[s & 7] = (val1 / val2) & 0xFFFF;
   R[(s & 7) | 1] = (val1 % val2) & 0xFFFF;
   setZ(R[s & 7] == 0);
   if (R[s & 7] & 0100000) {
      PS |= FLAGN;
   }
   if (val1 == 0) {
      PS |= FLAGV;
   }
}

static void ASH(uint16_t instr) {
   uint8_t d = instr & 077;
   uint8_t s = (instr & 07700) >> 6;
   uint16_t val1 = R[s & 7];
   uint16_t da = aget(d, 2);
   uint16_t val2 = memread16(da) & 077;
   PS &= 0xFFF0;
   int32_t sval;
   if (val2 & 040) {
      val2 = (077 ^ val2) + 1;
      if (val1 & 0100000) {
         sval = 0xFFFF ^ (0xFFFF >> val2);
         sval |= val1 >> val2;
      } else {
         sval = val1 >> val2;
      }
      if (val1 & (1 << (val2 - 1))) {
         PS |= FLAGC;
      }
   } else {
      sval = (val1 << val2) & 0xFFFF;
      if (val1 & (1 << (16 - val2))) {
         PS |= FLAGC;
      }
   }
   R[s & 7] = sval;
   setZ(sval == 0);
   if (sval & 0100000) {
      PS |= FLAGN;
   }
   if ((sval & 0100000)xor(val1 & 0100000)) {
      PS |= FLAGV;
   }
}

static void ASHC(uint16_t instr) {
   uint8_t d = instr & 077;
   uint8_t s = (instr & 07700) >> 6;
   uint16_t val1 = R[s & 7] << 16 | R[(s & 7) | 1];
   uint16_t da = aget(d, 2);
   uint16_t val2 = memread16(da) & 077;
   PS &= 0xFFF0;
   int32_t sval;
   if (val2 & 040) {
      val2 = (077 ^ val2) + 1;
      if (val1 & 0x80000000) {
         sval = 0xFFFFFFFF ^ (0xFFFFFFFF >> val2);
         sval |= val1 >> val2;
      } else {
         sval = val1 >> val2;
      }
      if (val1 & (1 << (val2 - 1))) {
         PS |= FLAGC;
      }
   } else {
      sval = (val1 << val2) & 0xFFFFFFFF;
      if (val1 & (1 << (32 - val2))) {
         PS |= FLAGC;
      }
   }
   R[s & 7] = (sval >> 16) & 0xFFFF;
   R[(s & 7) | 1] = sval & 0xFFFF;
   setZ(sval == 0);
   if (sval & 0x80000000) {
      PS |= FLAGN;
   }
   if ((sval & 0x80000000)xor(val1 & 0x80000000)) {
      PS |= FLAGV;
   }
}

static void XOR(uint16_t instr) {
   const uint8_t d = instr & 077;
   const uint8_t s = (instr & 07700) >> 6;
   const uint16_t val1 = R[s & 7];
   const uint16_t da = aget(d, 2);
   const uint16_t val2 = memread16(da);
   const uint16_t uval = val1 ^ val2;
   PS &= 0xFFF1;
   setZ(uval == 0);
   if (uval & 0x8000) {
      PS |= FLAGN;
   }
   memwrite16(da, uval);
}

static void SOB(const uint16_t instr) {
   const uint8_t s = (instr & 07700) >> 6;
   uint8_t o = instr & 0xFF;
   R[s & 7]--;
   if (R[s & 7]) {
      o &= 077;
      o <<= 1;
      R[7] -= o;
   }
}

static void CLR(uint16_t instr) {
   const uint8_t d = instr & 077;
   const uint8_t l = 2 - (instr >> 15);
   PS &= 0xFFF0;
   PS |= FLAGZ;
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
   PS &= 0xFFF0;
   PS |= FLAGC;
   if (uval & msb) {
      PS |= FLAGN;
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
   PS &= 0xFFF1;
   if (uval & msb) {
      PS |= FLAGN | FLAGV;
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
   PS &= 0xFFF1;
   if (uval & msb) {
      PS |= FLAGN;
   }
   if (uval == maxp) {
      PS |= FLAGV;
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
   PS &= 0xFFF0;
   if (sval & msb) {
      PS |= FLAGN;
   }
   if (sval == 0) {
      PS |= FLAGZ;
   } else {
      PS |= FLAGC;
   }
   if (sval == 0x8000) {
      PS |= FLAGV;
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
   if (PS & FLAGC) {
      PS &= 0xFFF0;
      if ((uval + 1) & msb) {
         PS |= FLAGN;
      }
      setZ(uval == max);
      if (uval == 0077777) {
         PS |= FLAGV;
      }
      if (uval == 0177777) {
         PS |= FLAGC;
      }
      memwrite(da, l, (uval + 1) & max);
   } else {
      PS &= 0xFFF0;
      if (uval & msb) {
         PS |= FLAGN;
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
   if (PS & FLAGC) {
      PS &= 0xFFF0;
      if ((sval - 1) & msb) {
         PS |= FLAGN;
      }
      setZ(sval == 1);
      if (sval) {
         PS |= FLAGC;
      }
      if (sval == 0100000) {
         PS |= FLAGV;
      }
      memwrite(da, l, (sval - 1) & max);
   } else {
      PS &= 0xFFF0;
      if (sval & msb) {
         PS |= FLAGN;
      }
      setZ(sval == 0);
      if (sval == 0100000) {
         PS |= FLAGV;
      }
      PS |= FLAGC;
   }
}

static void TST(uint16_t instr) {
   uint8_t d = instr & 077;
   uint8_t l = 2 - (instr >> 15);
   uint16_t msb = l == 2 ? 0x8000 : 0x80;
   uint16_t uval = memread(aget(d, l), l);
   PS &= 0xFFF0;
   if (uval & msb) {
      PS |= FLAGN;
   }
   setZ(uval == 0);
}

static void ROR(uint16_t instr) {
   uint8_t d = instr & 077;
   uint8_t l = 2 - (instr >> 15);
   int32_t max = l == 2 ? 0xFFFF : 0xff;
   uint16_t da = aget(d, l);
   int32_t sval = memread(da, l);
   if (PS & FLAGC) {
      sval |= max + 1;
   }
   PS &= 0xFFF0;
   if (sval & 1) {
      PS |= FLAGC;
   }
   // watch out for integer wrap around
   if (sval & (max + 1)) {
      PS |= FLAGN;
   }
   setZ(!(sval & max));
   if ((sval & 1)xor(sval & (max + 1))) {
      PS |= FLAGV;
   }
   sval >>= 1;
   memwrite(da, l, sval);
}

static void ROL(uint16_t instr) {
   uint8_t d = instr & 077;
   uint8_t l = 2 - (instr >> 15);
   uint16_t msb = l == 2 ? 0x8000 : 0x80;
   int32_t max = l == 2 ? 0xFFFF : 0xff;
   uint16_t da = aget(d, l);
   int32_t sval = memread(da, l) << 1;
   if (PS & FLAGC) {
      sval |= 1;
   }
   PS &= 0xFFF0;
   if (sval & (max + 1)) {
      PS |= FLAGC;
   }
   if (sval & msb) {
      PS |= FLAGN;
   }
   setZ(!(sval & max));
   if ((sval ^ (sval >> 1)) & msb) {
      PS |= FLAGV;
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
   PS &= 0xFFF0;
   if (uval & 1) {
      PS |= FLAGC;
   }
   if (uval & msb) {
      PS |= FLAGN;
   }
   if ((uval & msb)xor(uval & 1)) {
      PS |= FLAGV;
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
   PS &= 0xFFF0;
   if (sval & msb) {
      PS |= FLAGC;
   }
   if (sval & (msb >> 1)) {
      PS |= FLAGN;
   }
   if ((sval ^ (sval << 1)) & msb) {
      PS |= FLAGV;
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
   if (PS & FLAGN) {
      memwrite(da, l, max);
   } else {
      PS |= FLAGZ;
      memwrite(da, l, 0);
   }
}

static void JMP(uint16_t instr) {
   uint8_t d = instr & 077;
   uint16_t uval = aget(d, 2);
   if (isReg(uval)) {
      printf("JMP called with register dest\n");
      panic();
   }
   R[7] = uval;
}

static void SWAB(uint16_t instr) {
   uint8_t d = instr & 077;
   uint8_t l = 2 - (instr >> 15);
   uint16_t da = aget(d, l);
   uint16_t uval = memread(da, l);
   uval = ((uval >> 8) | (uval << 8)) & 0xFFFF;
   PS &= 0xFFF0;
   setZ(uval & 0xFF);
   if (uval & 0x80) {
      PS |= FLAGN;
   }
   memwrite(da, l, uval);
}

static void MARK(uint16_t instr) {
   R[6] = R[7] + ((instr & 077) << 1);
   R[7] = R[5];
   R[5] = pop();
}

static void MFPI(uint16_t instr) {
   uint8_t d = instr & 077;
   uint16_t da = aget(d, 2);
   uint16_t uval;
   if (da == 0170006) {
      // val = (curuser == prevuser) ? R[6] : (prevuser ? k.USP : KSP);
      if (curuser == prevuser) {
         uval = R[6];
      } else {
         if (prevuser) {
            uval = USP;
         } else {
            uval = KSP;
         }
      }
   } else if (isReg(da)) {
      printf("invalid MFPI instruction\n");
      panic();
      return; // unreached
   } else {
      uval = read16((uint16_t)da);
   }
   push(uval);
   PS &= 0xFFF0;
   PS |= FLAGC;
   setZ(uval == 0);
   if (uval & 0x8000) {
      PS |= FLAGN;
   }
}

static void MTPI(uint16_t instr) {
   uint8_t d = instr & 077;
   uint16_t da = aget(d, 2);
   uint16_t uval = pop();
   if (da == 0170006) {
      if (curuser == prevuser) {
         R[6] = uval;
      } else {
         if (prevuser) {
            USP = uval;
         } else {
            KSP = uval;
         }
      }
   } else if (isReg(da)) {
      printf("invalid MTPI instrution\n");
      panic();
   } else {
      write16((uint16_t)da, uval);
   }
   PS &= 0xFFF0;
   PS |= FLAGC;
   setZ(uval == 0);
   if (uval & 0x8000) {
      PS |= FLAGN;
   }
}

static void RTS(uint16_t instr) {
   uint8_t d = instr & 077;
   R[7] = R[d & 7];
   R[d & 7] = pop();
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
   uint16_t prev = PS;
   switchmode(false);
   push(prev);
   push(R[7]);
   R[7] = read16(uval);
   PS = read16(uval + 2);
   if (prevuser) {
      PS |= (1 << 13) | (1 << 12);
   }
}

static void _RTT(uint16_t instr) {
   R[7] = pop();
   uint16_t uval = pop();
   if (curuser) {
      uval &= 047;
      uval |= PS & 0177730;
   }
   write16(0177776, uval);
}

static void RESET(uint16_t instr) {
   if (curuser) {
      return;
   }
   // cons::clearterminal();
   //rk11::reset();
}

static void step() {
   PC = R[7];
   uint16_t instr = read16(PC);
   R[7] += 2;

   if (PRINTSTATE)
      printstate();

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
   case 00067: // SXT
      SXT(instr);
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
      break;
   case 0006500: // MFPI
      MFPI(instr);
      return;
   case 0006600: // MTPI
      MTPI(instr);
      return;
   }
   if ((instr & 0177770) == 0000200) { // RTS
      RTS(instr);
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
      if (!(N() xor V())) {
         branch(instr & 0xFF);
      }
      return;
   case 0002400:
      if (N() xor V()) {
         branch(instr & 0xFF);
      }
      return;
   case 0003000:
      if ((!(N() xor V())) && (!Z())) {
         branch(instr & 0xFF);
      }
      return;
   case 0003400:
      if ((N() xor V()) || Z()) {
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
         PS |= instr & 017;
      } else {
         PS &= ~instr & 017;
      }
      return;
   }
   switch (instr & 7) {
   case 00: // HALT
      if (curuser) {
         break;
      }
      printf("HALT\n");
      panic();
   case 01: // WAIT
      if (curuser) {
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
   printf("invalid instruction\n");
   trap(INTINVAL);
}

static void trapat(uint16_t vec) { // , msg string) {
   if (vec & 1) {
      printf("Thou darst calling trapat() with an odd vector number?\n");
      panic();
   }
   printf("trap: %x\n", vec);

   /*var prev uint16
     defer func() {
     t = recover()
     switch t = t.(type) {
     case trap:
     writedebug("red stack trap!\n")
     memory[0] = uint16(k.R[7])
     memory[1] = prev
     vec = 4
     panic("fatal")
     case nil:
     break
     default:
     panic(t)
     }
   */
   uint16_t prev = PS;
   switchmode(false);
   push(prev);
   push(R[7]);

   R[7] = read16(vec);
   PS = read16(vec + 2);
   if (prevuser) {
      PS |= (1 << 13) | (1 << 12);
   }
}

// pop the top interrupt off the itab.
static void popirq() {
   uint8_t i;
   for (i = 0; i < ITABN - 1; i++) {
      itab[i] = itab[i + 1];
   }
   itab[ITABN - 1].vec = 0;
   itab[ITABN - 1].pri = 0;
}

static void handleinterrupt() {
   uint8_t vec = itab[0].vec;
   if (DEBUG_INTER) {
      printf("IRQ: %x\n", vec);
   }
   uint16_t vv = setjmp(trapbuf);
   if (vv == 0) {
      uint16_t prev = PS;
      switchmode(false);
      push(prev);
      push(R[7]);
   } else {
      trapat(vv);
   }

   R[7] = read16(vec);
   PS = read16(vec + 2);
   if (prevuser) {
      PS |= (1 << 13) | (1 << 12);
   }
   popirq();
}

void pdp11_reset(uint16_t address) {
   LKS = 1 << 7;
   R[7] = address;
}

void pdp11_interrupt(uint8_t vec, uint8_t pri) {
   if (vec & 1) {
      printf("Thou darst calling interrupt() with an odd vector number?\n");
      panic();
   }
   // fast path
   if (itab[0].vec == 0) {
      itab[0].vec = vec;
      itab[0].pri = pri;
      return;
   }
   uint8_t i;
   for (i = 0; i < ITABN; i++) {
      if ((itab[i].vec == 0) || (itab[i].pri < pri)) {
         break;
      }
   }
   for (; i < ITABN; i++) {
      if ((itab[i].vec == 0) || (itab[i].vec >= vec)) {
         break;
      }
   }
   if (i >= ITABN) {
      printf("interrupt table full\n");
      panic();
   }
   uint8_t j;
   for (j = i + 1; j < ITABN; j++) {
      itab[j] = itab[j - 1];
   }
   itab[i].vec = vec;
   itab[i].pri = pri;
}

static void loop0() {
   while (tubeContinueRunning()) {
      if ((itab[0].vec > 0) && (itab[0].pri >= ((PS >> 5) & 7))) {
         handleinterrupt();
         uint8_t i;
         for (i = 0; i < ITABN - 1; i++) {
            itab[i] = itab[i + 1];
         }
         itab[ITABN - 1].vec = 0;
         itab[ITABN - 1].pri = 0;
         return; // exit from loop to reset trapbuf
      }
      step();
      if (++clkcounter > 39999) {
         clkcounter = 0;
         LKS |= (1 << 7);
         if (LKS & (1 << 6)) {
            pdp11_interrupt(INTCLOCK, 6);
         }
      }
      // cons::poll();
   }
}

void pdp11_execute() {
   uint16_t vec = setjmp(trapbuf);
   if (vec == 0) {
      loop0();
   } else {
      trapat(vec);
   }
}
