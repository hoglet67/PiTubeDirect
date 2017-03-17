// B-em v2.2 by Tom Walker
// 32016 parasite processor emulation (not working yet)

// And Simon R. Ellwood
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdint.h>
#include <inttypes.h>
#include <string.h>
#include <math.h>
#include "32016.h"
#include "mem32016.h"
#include "defs.h"
#include "Trap.h"
#include "Decode.h"

#ifdef PROFILING
#include "Profile.h"
#endif

#ifdef INCLUDE_DEBUGGER
#include "32016_debug.h"
#include "../cpu_debug.h"
#endif

#define CXP_UNUSED_WORD 0xAAAA

ProcessorRegisters PR;
uint32_t r[8];
FloatingPointRegisters FR;
uint32_t FSR;

static uint32_t pc;
uint32_t sp[2];
Temp64Type Immediate64;

uint32_t startpc;

RegLKU Regs[2];
uint32_t genaddr[2];
uint32_t *genreg[2];
int gentype[2];
OperandSizeType OpSize;

const uint32_t IndexLKUP[8] = { 0x0, 0x1, 0x4, 0x5, 0x8, 0x9, 0xC, 0xD };                    // See Page 2-3 of the manual!

/* A custom warning logger for n32016 that logs the PC */

void n32016_warn(char *fmt, ...)
{   
   char buf[1024];
   int len = snprintf(buf, sizeof(buf), "[pc=%"PRIX32"] ", pc);
   va_list ap;
   va_start(ap, fmt);
   vsnprintf(buf + len, sizeof(buf) - len, fmt, ap);
   va_end(ap);   
   log_warn("%s", buf);
}

void n32016_ShowRegs(int Option)
{
   if (Option & BIT(0))
   {
      TrapTRACE("R0=%08"PRIX32" R1=%08"PRIX32" R2=%08"PRIX32" R3=%08"PRIX32, r[0], r[1], r[2], r[3]);
      TrapTRACE("R4=%08"PRIX32" R5=%08"PRIX32" R6=%08"PRIX32" R7=%08"PRIX32, r[4], r[5], r[6], r[7]);
   }

   if (Option & BIT(1))
   {
      TrapTRACE("PC=%08"PRIX32" SB=%08"PRIX32" SP=%08"PRIX32" TRAP=%08"PRIX32, pc, sb, GET_SP(), TrapFlags);
      TrapTRACE("FP=%08"PRIX32" INTBASE=%08"PRIX32" PSR=%04"PRIX32" MOD=%04"PRIX32, fp, intbase, psr, mod);
   }

   if (nscfg.fpu_flag)
   {
      if (Option & BIT(2))
      {
         TrapTRACE("F0=%f F1=%f F2=%f F3=%f", FR.fr32[0], FR.fr32[1], FR.fr32[4], FR.fr32[5]);
         TrapTRACE("F4=%f F5=%f F6=%f F7=%f", FR.fr32[8], FR.fr32[9], FR.fr32[12], FR.fr32[13]);
      }

      if (Option & BIT(3))
      {
         TrapTRACE("D0=%lf D1=%lf D2=%lf D3=%lf", FR.fr64[0], FR.fr64[1], FR.fr64[2], FR.fr64[3]);
         TrapTRACE("D4=%lf D5=%lf D6=%lf D7=%lf", FR.fr64[4], FR.fr64[5], FR.fr64[6], FR.fr64[7]);
      }
   }
}

const uint32_t OpSizeLookup[6] =
{
   (sz8  << 24) | (sz8  << 16) | (sz8  << 8) | sz8,                // Integer byte
   (sz16 << 24) | (sz16 << 16) | (sz16 << 8) | sz16,               // Integer word
   0xFFFFFFFF,                                                     // Illegal
   (sz32 << 24) | (sz32 << 16) | (sz32 << 8) | sz32,               // Integer double-word
//   (sz32 << 24) | (sz32 << 16) | (sz32 << 8) | sz32,               // Floating Point Single Precision
//   (sz64 << 24) | (sz64 << 16) | (sz64 << 8) | sz64                // Floating Point Double Precision
   (sz32 << 8) | sz32,               // Floating Point Single Precision
   (sz64 << 8) | sz64                // Floating Point Double Precision
};

void n32016_init()
{
   init_ram();
}

void n32016_close()
{
}

void n32016_reset()
{
   n32016_reset_addr(0xF00000);
}

void n32016_reset_addr(uint32_t StartAddress)
{
   n32016_build_matrix();

   pc = StartAddress;
   psr = 0;

   FSR = 0;

   //PR.BPC = 0x20F; //Example Breakpoint
   PR.BPC = 0xFFFFFFFF;
}

uint32_t n32016_get_pc()
{
   return pc;
}

uint32_t n32016_get_startpc()
{
   return startpc;
}

void n32016_set_pc(uint32_t value)
{
   pc = value;
}

static void pushd(uint32_t val)
{
   DEC_SP(4);
   write_x32(GET_SP(), val);
}

void PushArbitary(uint64_t Value, uint32_t Size)
{
   DEC_SP(Size);
   write_Arbitary(GET_SP(), &Value, Size);
}

static uint16_t popw()
{
   uint16_t temp = read_x16(GET_SP());
   INC_SP(2);

   return temp;
}

static uint32_t popd()
{
   uint32_t temp = read_x32(GET_SP());
   INC_SP(4);

   return temp;
}

uint32_t PopArbitary(uint32_t Size)
{
   uint32_t Result = read_n(GET_SP(), Size);
   INC_SP(Size);

   return Result;
}

int32_t GetDisplacement(uint32_t* pPC)
{
   // Displacements are in Little Endian and need to be sign extended
   int32_t Value;

   MultiReg Disp;
   Disp.u32 = SWAP32(read_x32(*pPC));

   switch (Disp.u32 >> 29)
      // Look at the top 3 bits
   {
      case 0: // 7 Bit Positive
      case 1:
      {
         Value = Disp.u8;
         (*pPC) += sizeof(int8_t);
      }
      break;

      case 2: // 7 Bit Negative
      case 3:
      {
         Value = (Disp.u8 | 0xFFFFFF80);
         (*pPC) += sizeof(int8_t);
      }
      break;

      case 4: // 14 Bit Positive
      {
         Value = (Disp.u16 & 0x3FFF);
         (*pPC) += sizeof(int16_t);
      }
      break;

      case 5: // 14 Bit Negative
      {
         Value = (Disp.u16 | 0xFFFFC000);
         (*pPC) += sizeof(int16_t);
      }
      break;

      case 6: // 30 Bit Positive
      {
         Value = (Disp.u32 & 0x3FFFFFFF);
         (*pPC) += sizeof(int32_t);
      }
      break;

      case 7: // 30 Bit Negative
      default: // Stop it moaning about Value not being set ;)
      {
         Value = Disp.u32;
         (*pPC) += sizeof(int32_t);
      }
      break;
   }

   return Value;
}

uint32_t Truncate(uint32_t Value, uint32_t Size)
{
   switch (Size)
   {
      case sz8:
         return Value & 0x000000FF;
      case sz16:
         return Value & 0x0000FFFF;
   }

   return Value;
}

uint32_t ReadGen(uint32_t c)
{
   uint32_t Temp = 0;

   switch (gentype[c])
   {
      case Memory:
      {
         switch (OpSize.Op[c])
         {
            case sz8:   return read_x8(genaddr[c]);
            case sz16:  return read_x16(genaddr[c]);
            case sz32:  return read_x32(genaddr[c]);
         }
      }
      break;

      case Register:
      {
         Temp = *genreg[c];
         return Truncate(Temp, OpSize.Op[c]);
      }
      // No break due to return

      case TOS:
      {
         return PopArbitary(OpSize.Op[c]);
      }
      // No break due to return

      case OpImmediate:
      {
         return Truncate(genaddr[c], OpSize.Op[c]);
      }
      // No break due to return
   }

   return 0;
}

uint64_t ReadGen64(uint32_t c)
{
   uint64_t Temp = 0;

   switch (gentype[c])
   {
      case Memory:
      {
         Temp = read_x64(genaddr[c]);
      }
      break;

      case Register:
      {
         Temp = *(uint64_t*)genreg[c];
      }
      break;

      case TOS:
      {
         Temp = read_x64(GET_SP());
         INC_SP(sz64);
      }
      break;

      case OpImmediate:
      {
         Temp = Immediate64.u64;
      }
      break;
   }

   return Temp;
}

uint32_t ReadAddress(uint32_t c)
{
   if (gentype[c] == Register)
   {
      return *genreg[c];
   }

   return genaddr[c];
}

static void getgen(int gen, int c)
{
   gen &= 0x1F;
   Regs[c].Whole = gen;

   if (gen >= EaPlusRn)
   {
      Regs[c].UpperByte = READ_PC_BYTE();
      //Regs[c].Whole |= READ_PC_BYTE() << 8;

      if (Regs[c].IdxType == Immediate)
      {
         SET_TRAP(IllegalImmediate);
      }
      else if (Regs[c].IdxType >= EaPlusRn)
      {
         SET_TRAP(IllegalDoubleIndexing);
      }
   }
}

static void GetGenPhase2(RegLKU gen, int c)
{
   if (gen.Whole < 0xFFFF)                                              // Does this Operand exist ?
   {
      if (gen.OpType <= R7)
      {
         switch (gen.RegType)
         {
            case Integer:
            {
               genreg[c] = &r[gen.OpType];
            }
            break;

            case SinglePrecision:
            {
               genreg[c] = (uint32_t *) &FR.fr32[IndexLKUP[gen.OpType]];
            }
            break;

            case DoublePrecision:
            {
               genreg[c] = (uint32_t *) &FR.fr64[gen.OpType];
            }
            break;

            default:
            {
               PiWARN("Illegal RegType value: %u", gen.RegType);
            }
         }

         gentype[c] = Register;
         return;
      }

      if (gen.OpType == Immediate)
      {
         MultiReg temp3;

         if (OpSize.Op[c] == sz64)
         {
            temp3.u32 = SWAP32(read_x32(pc));
            Immediate64.u64 = (((uint64_t) temp3.u32) << 32);
            temp3.u32 = SWAP32(read_x32(pc + 4));
            Immediate64.u64 |= temp3.u32;
         }
         else
         {
            // Why can't they just decided on an endian and then stick to it?
            temp3.u32 = SWAP32(read_x32(pc));
            if (OpSize.Op[c] == sz8)
               genaddr[c] = temp3.u8;
            else if (OpSize.Op[c] == sz16)
               genaddr[c] = temp3.u16;
            else
               genaddr[c] = temp3.u32;
         }

         pc += OpSize.Op[c];
         gentype[c] = OpImmediate;
         return;
      }

      gentype[c] = Memory;

      if (gen.OpType <= R7_Offset)
      {
         genaddr[c] = r[gen.Whole & 7] + GetDisplacement(&pc);
         return;
      }

      uint32_t temp, temp2;

      if (gen.OpType >= EaPlusRn)
      {
         uint32_t Shift = gen.Whole & 3;
         RegLKU NewPattern;
         NewPattern.Whole = gen.IdxType;
         GetGenPhase2(NewPattern, c);

         int32_t Offset = ((int32_t) r[gen.IdxReg]) * (1 << Shift);
         if (gentype[c] != Register)
         {
            genaddr[c] += Offset;
         }
         else
         {
            genaddr[c] = (*genreg[c]) + Offset;
         }

         gentype[c] = Memory;                               // Force Memory
         return;
      }

      switch (gen.OpType)
      {
         case FrameRelative:
            temp = GetDisplacement(&pc);
            temp2 = GetDisplacement(&pc);
            genaddr[c] = read_x32(fp + temp);
            genaddr[c] += temp2;
            break;

         case StackRelative:
            temp = GetDisplacement(&pc);
            temp2 = GetDisplacement(&pc);
            genaddr[c] = read_x32(GET_SP() + temp);
            genaddr[c] += temp2;
            break;

         case StaticRelative:
            temp = GetDisplacement(&pc);
            temp2 = GetDisplacement(&pc);
            genaddr[c] = read_x32(sb + temp);
            genaddr[c] += temp2;
            break;

         case Absolute:
            genaddr[c] = GetDisplacement(&pc);
            break;

         case External:
            temp = read_x32(mod + 4);
            temp += ((int32_t) GetDisplacement(&pc)) * 4;
            temp2 = read_x32(temp);
            genaddr[c] = temp2 + GetDisplacement(&pc);
            break;

         case TopOfStack:
            genaddr[c] = GET_SP();
            gentype[c] = TOS;
            break;

         case FpRelative:
            genaddr[c] = GetDisplacement(&pc) + fp;
            break;

         case SpRelative:
            genaddr[c] = GetDisplacement(&pc) + GET_SP();
            break;

         case SbRelative:
            genaddr[c] = GetDisplacement(&pc) + sb;
            break;

         case PcRelative:
            genaddr[c] = GetDisplacement(&pc) + startpc;
            break;

         default:
            n32016_dumpregs("Bad NS32016 gen mode");
            break;
      }
   }
}

// From: http://homepage.cs.uiowa.edu/~jones/bcd/bcd.html
static uint32_t bcd_add_16(uint32_t a, uint32_t b, uint32_t *carry)
{
   uint32_t t1, t2; // unsigned 32-bit intermediate values
   //PiTRACE("bcd_add_16: in  %08x %08x %08x", a, b, *carry);
   if (*carry)
   {
      b++; // I'm 90% sure its OK to handle carry this way
   } // i.e. its OK if the ls digit of b becomes A
   t1 = a + 0x06666666;
   t2 = t1 ^ b; // sum without carry propagation
   t1 = t1 + b; // provisional sum
   t2 = t1 ^ t2; // all the binary carry bits
   t2 = ~t2 & 0x11111110; // just the BCD carry bits
   t2 = (t2 >> 2) | (t2 >> 3); // correction
   t2 = t1 - t2; // corrected BCD sum
   *carry = (t2 & 0xFFFF0000) ? 1 : 0;
   t2 &= 0xFFFF;
   //PiTRACE("bcd_add_16: out %08x %08x", t2, *carry);
   return t2;
}

static uint32_t bcd_sub_16(uint32_t a, uint32_t b, uint32_t *carry)
{
   uint32_t t1, t2; // unsigned 32-bit intermediate values
   //PiTRACE("bcd_sub_16: in  %08x %08x %08x", a, b, *carry);
   if (*carry)
   {
      b++;
   }
   *carry = 0;
   t1 = 0x9999 - b;
   t2 = bcd_add_16(t1, 1, carry);
   t2 = bcd_add_16(a, t2, carry);
   *carry = 1 - *carry;
   //PiTRACE("bcd_add_16: out %08x %08x", t2, *carry);
   return t2;
}

static uint32_t bcd_add(uint32_t a, uint32_t b, int size, uint32_t *carry)
{
   if (size == sz8)
   {
      uint32_t word = bcd_add_16(a, b, carry);
      // If anything beyond bit 7 is set, then there has been a carry out
      *carry = (word & 0xFF00) ? 1 : 0;
      return word & 0xFF;
   }
   else if (size == sz16)
   {
      return bcd_add_16(a, b, carry);
   }
   else
   {
      uint32_t word0 = bcd_add_16(a & 0xFFFF, b & 0xFFFF, carry);
      uint32_t word1 = bcd_add_16(a >> 16, b >> 16, carry);
      return word0 + (word1 << 16);
   }
}

static uint32_t bcd_sub(uint32_t a, uint32_t b, int size, uint32_t *carry)
{
   if (size == sz8)
   {
      uint32_t word = bcd_sub_16(a, b, carry);
      // If anything beyond bit 7 is set, then there has been a carry out
      *carry = (word & 0xFF00) ? 1 : 0;
      return word & 0xFF;
   }
   else if (size == sz16)
   {
      return bcd_sub_16(a, b, carry);
   }
   else
   {
      uint32_t word0 = bcd_sub_16(a & 0xFFFF, b & 0xFFFF, carry);
      uint32_t word1 = bcd_sub_16(a >> 16, b >> 16, carry);
      return word0 + (word1 << 16);
   }
}

static uint32_t AddCommon(uint32_t a, uint32_t b, uint32_t cin)
{
   uint32_t sum = a + b + cin;
   if (cin == 0)
      C_FLAG = TEST(sum < a || sum < b);
   else
      C_FLAG = TEST(sum <= a || sum <= b);
   F_FLAG = TEST((a ^ sum) & (b ^ sum) & 0x80000000);

   //PiTRACE("ADD FLAGS: C=%d F=%d", C_FLAG, F_FLAG);

   return sum;
}

static uint32_t SubCommon(uint32_t a, uint32_t b, uint32_t cin)
{
   uint32_t diff = a - b - cin;
   if (cin == 0)
      C_FLAG = TEST(a < b);
   else
      C_FLAG = TEST(a <= b);
   F_FLAG = TEST((a ^ b) & (a ^ diff) & 0x80000000);

   //PiTRACE("SUB FLAGS: C=%d F=%d", C_FLAG, F_FLAG);

   return diff;
}

// The difference between DIV and QUO occurs when the result (the quotient) is negative
// e.g. -16 QUO 3 ===> -5
// e.g. -16 DIV 3 ===> -6
// i.e. DIV rounds down to the more negative nu,ber
// This case is detected if the sign bits of the two operands differ
static uint32_t div_operator(uint32_t a, uint32_t b)
{
   uint32_t ret = 0;
   int signmask = BIT(((OpSize.Op[0] - 1) << 3) + 7);
   if ((a & signmask) && !(b & signmask))
   {
      // e.g. a = -16; b =  3 ===> a becomes -18
      a -= b - 1;
   }
   else if (!(a & signmask) && (b & signmask))
   {
      // e.g. a =  16; b = -3 ===> a becomes 18
      a -= b + 1;
   }
   switch (OpSize.Op[0])
   {
      case sz8:
         ret = (int8_t) a / (int8_t) b;
      break;

      case sz16:
         ret = (int16_t) a / (int16_t) b;
      break;

      case sz32:
         ret = (int32_t) a / (int32_t) b;
      break;
   }
   return ret;
}

static uint32_t mod_operator(uint32_t a, uint32_t b)
{
   return a - div_operator(a, b) * b;
}


// OffsetDiv8 needs to work as follows
//
//  9 =>  1
//  8 =>  1
//  7 =>  0
//  6 =>  0
//  5 =>  0
//  4 =>  0
//  3 =>  0
//  2 =>  0
//  1 =>  0
//  0 =>  0
// -1 => -1
// -2 => -1
// -3 => -1
// -4 => -1
// -5 => -1
// -6 => -1
// -7 => -1
// -8 => -1
// -9 => -2

static int32_t OffsetDiv8(int32_t Offset)
{
   if (Offset >= 0)
   {
      return Offset / 8;
   }
   else
   {
      return (Offset - 7) / 8;
   }
}

// Handle the writing to the upper half of mei/dei destination
static void handle_mei_dei_upper_write(uint64_t result)
{
   uint32_t temp;
   // Writing to an odd register is strictly speaking undefined
   // But BBC Basic relies on a particular behaviour that the NS32016 has in this case
   uint32_t *reg_addr = genreg[1] + ((Regs[1].Whole & 1) ? -1 : 1);
   switch (OpSize.Op[0])
   {
      case sz8:
         temp = (uint8_t) (result >> 8);
         if (gentype[1] == Register)
            *(uint8_t *) (reg_addr) = temp;
         else
            write_x8(genaddr[1] + 4, temp);
      break;

      case sz16:
         temp = (uint16_t) (result >> 16);
         if (gentype[1] == Register)
            *(uint16_t *) (reg_addr) = temp;
         else
            write_x16(genaddr[1] + 4, temp);
      break;

      case sz32:
         temp = (uint32_t) (result >> 32);
         if (gentype[1] == Register)
            *(uint32_t *) (reg_addr) = temp;
         else
            write_x32(genaddr[1] + 4, temp);
      break;
   }
}

uint32_t CompareCommon(uint32_t src1, uint32_t src2)
{
   L_FLAG = TEST(src1 > src2);

   if (OpSize.Op[0] == sz8)
   {
      N_FLAG = TEST(((int8_t) src1) > ((int8_t) src2));
   }
   else if (OpSize.Op[0] == sz16)
   {
      N_FLAG = TEST(((int16_t) src1) > ((int16_t) src2));
   }
   else
   {
      N_FLAG = TEST(((int32_t) src1) > ((int32_t) src2));
   }

   Z_FLAG = TEST(src1 == src2);

   return Z_FLAG;
}

uint32_t StringMatching(uint32_t opcode, uint32_t Value)
{
   uint32_t Options = (opcode >> 17) & 3;

   if (Options)
   {
      uint32_t Compare = Truncate(r[4], OpSize.Op[0]);

      if (Options == 1) // While match
      {
         if (Value != Compare)
         {
            F_FLAG = 1; // Set PSR F Bit
            return 1;
         }
      }
      else if (Options == 3) // Until Match
      {
         if (Value == Compare)
         {
            F_FLAG = 1; // Set PSR F Bit
            return 1;
         }
      }
   }

   return 0;
}

void StringRegisterUpdate(uint32_t opcode)
{
   uint32_t Size = OpSize.Op[0];

   if (opcode & BIT(Backwards)) // Adjust R1
   {
      r[1] -= Size;
   }
   else
   {
      r[1] += Size;
   }

   if (((opcode >> 10) & 0x0F) != (SKPS & 0x0F))
   {
      if (opcode & BIT(Backwards)) // Adjust R2 for all but SKPS
      {
         r[2] -= Size;
      }
      else
      {
         r[2] += Size;
      }
   }

   r[0]--; // Adjust R0
}

uint32_t CheckCondition(uint32_t Pattern)
{
   uint32_t bResult = 0;

   switch (Pattern & 0xF)
   {
      case 0x0:
         if (Z_FLAG)
            bResult = 1;
         break;
      case 0x1:
         if (!Z_FLAG)
            bResult = 1;
         break;
      case 0x2:
         if (C_FLAG)
            bResult = 1;
         break;
      case 0x3:
         if (!C_FLAG)
            bResult = 1;
         break;
      case 0x4:
         if (L_FLAG)
            bResult = 1;
         break;
      case 0x5:
         if (!L_FLAG)
            bResult = 1;
         break;
      case 0x6:
         if (N_FLAG)
            bResult = 1;
         break;
      case 0x7:
         if (!N_FLAG)
            bResult = 1;
         break;
      case 0x8:
         if (F_FLAG)
            bResult = 1;
         break;
      case 0x9:
         if (!F_FLAG)
            bResult = 1;
         break;
      case 0xA:
         if (!(psr & (0x40 | 0x04)))
         //if (!(Z_FLAG || L_FLAG))
            bResult = 1;
         break;
      case 0xB:
         if (psr & (0x40 | 0x04))
         //if (Z_FLAG || L_FLAG)
            bResult = 1;
         break;
      case 0xC:
         if (!(psr & (0x40 | 0x80)))
         //if (!(Z_FLAG || N_FLAG))
            bResult = 1;
         break;
      case 0xD:
         if (psr & (0x40 | 0x80))
         //if (Z_FLAG || N_FLAG)
            bResult = 1;
         break;
      case 0xE:
         bResult = 1;
         break;
      case 0xF:
         break;
   }

   return bResult;
}

uint32_t BitPrefix(void)
{
   int32_t Offset = ReadGen(0);
   uint32_t bit;
   SIGN_EXTEND(OpSize.Op[0], Offset);

   if (gentype[1] == Register)
   {
      // operand 0 is a register
      OpSize.Op[1] = sz32;
      bit = ((uint32_t) Offset) & 31;
   }
   else
   {
      // operand0 is memory
      genaddr[1] += OffsetDiv8(Offset);
      OpSize.Op[1] = sz8;
      bit = ((uint32_t) Offset) & 7;
   }

   WriteSize = OpSize.Op[1];

   return BIT(bit);
}

void PopRegisters(void)
{
   int c;
   int32_t temp = READ_PC_BYTE();

   for (c = 0; c < 8; c++)
   {
      if (temp & BIT(c))
      {
         r[c ^ 7] = popd();
      }
   }
}

void TakeInterrupt(uint32_t IntBase)
{
   uint32_t temp = psr;
   uint32_t temp2, temp3;

   psr &= ~0xF00;
   pushd((temp << 16) | mod);

   while (read_x8(pc) == 0xB2)                                    // Do not stack the address of a WAIT instruction!
   {
      pc++;
   }

   pushd(pc);
   temp = read_x32(IntBase);
   mod = temp & 0xFFFF;
   temp3 = temp >> 16;
   sb = read_x32(mod);
   temp2 = read_x32(mod + 8);
   pc = temp2 + temp3;
}

void WarnIfShiftInvalid(uint32_t shift, uint8_t size)
{
   size *= 8;    // 8, 16, 32
   // We allow a shift of +- 33 without warning, as we see examples
   // of this in BBC Basic.
   if ((shift > size + 1 && shift < 0xFF - size - 1) || (shift > 0xFF))
   {
      PiWARN("Invalid shift of %08"PRIX32" for size %"PRId8, shift, size);
   }
}

uint32_t ReturnCommon(void)
{
   if (U_FLAG)
   {
      return 1;                  // Trap
   }

   pc = popd();
   uint16_t unstack = popw();

   if (nscfg.de_flag == 0)
   {
      mod = unstack;
   }

   psr = popw();

   if (nscfg.de_flag == 0)
   {
      sb = read_x32(mod);
   }

   return 0;                     // OK
}

void n32016_exec()
{
   uint32_t opcode, WriteIndex;
   uint32_t temp, temp2, temp3;
   Temp64Type temp64;
   uint32_t Function;

   // Avoid a "might be uninitialized" warning
   temp = 0;
   temp64.u64 = 0;

   if (tube_irq & 2)
   {
      // NMI is edge sensitive, so it should be cleared here
      tube_irq &= ~2;
      TakeInterrupt(intbase + (1 * 4));
   }
   else if ((tube_irq & 1) && (psr & 0x800))
   {
      // IRQ is level sensitive, so the called should maintain the state
      TakeInterrupt(intbase);
   }

   while (tubecycles > 0)
   {
      tubecycles -= 8;

      CLEAR_TRAP();

      WriteSize      = szVaries;                                            // The size a result may be written as
      WriteIndex     = 1;                                                   // Default to writing operand 0
      OpSize.Whole   = 0;

      Regs[0].Whole  =
      Regs[1].Whole  = 0xFFFF;

      startpc  = pc;

#ifdef INCLUDE_DEBUGGER
      if (n32016_debug_enabled)
      {
         debug_preexec(&n32016_cpu_debug, pc);
      }
#endif

      opcode = read_x32(pc);

      if (pc == PR.BPC)
      {
         SET_TRAP(BreakPointHit);
         goto DoTrap;
      }

      BreakPoint(startpc, opcode);

      Function = FunctionLookup[opcode & 0xFF];
      uint32_t Format   = Function >> 4;

      if (Format < (FormatCount + 1))
      {
         pc += FormatSizes[Format];                                        // Add the basic number of bytes for a particular instruction
      }

      switch (Format)
      {
         case Format0:
         case Format1:
         {
            // Nothing here!
         }
         break;

         case Format2:
         {
            SET_OP_SIZE(opcode);
            WriteIndex = 0;
            getgen(opcode >> 11, 0);
         }
         break;

         case Format3:
         {
            Function += ((opcode >> 7) & 0x0F);
            SET_OP_SIZE(opcode);
            getgen(opcode >> 11, 0);
         }
         break;

         case Format4:
         {
            SET_OP_SIZE(opcode);
            getgen(opcode >> 11, 0);
            getgen(opcode >> 6, 1);
         }
         break;

         case Format5:
         {
            Function += ((opcode >> 10) & 0x0F);
            SET_OP_SIZE(opcode >> 8);
            if (Function == SETCFG)
            {
               OpSize.Whole = 0;
            }
            else if (opcode & BIT(Translation))
            {
               SET_OP_SIZE(0);         // 8 Bit
            }
         }
         break;

         case Format6:
         {
            Function += ((opcode >> 10) & 0x0F);
            SET_OP_SIZE(opcode >> 8);

            // Ordering important here, as getgen uses Operand Size
            switch (Function)
            {
               case ROT:
               case ASH:
               case LSH:
               {
                  OpSize.Op[0] = sz8;
               }
               break;
            }

            getgen(opcode >> 19, 0);
            getgen(opcode >> 14, 1);
         }
         break;

         case Format7:
         {
            Function += ((opcode >> 10) & 0x0F);
            SET_OP_SIZE(opcode >> 8);

            getgen(opcode >> 19, 0);
            getgen(opcode >> 14, 1);
         }
         break;

         case Format8:
         {
            if (opcode & 0x400)
            {
               if (opcode & 0x80)
               {
                  switch (opcode & 0x3CC0)
                  {
                     case 0x0C80:
                     {
                        Function = MOVUS;
                     }
                     break;

                     case 0x1C80:
                     {
                        Function = MOVSU;
                     }
                     break;

                     default:
                     {
                        Function = TRAP;
                     }
                     break;
                  }
               }
               else
               {
                  Function = (opcode & 0x40) ? FFS : INDEX;
               }
            }
            else
            {
               Function += ((opcode >> 6) & 3);
            }

            SET_OP_SIZE(opcode >> 8);

            if (Function == CVTP)
            {
               SET_OP_SIZE(3);               // 32 Bit
            }

            getgen(opcode >> 19, 0);
            getgen(opcode >> 14, 1);
         }
         break;

         case Format9:
         {
            if (nscfg.fpu_flag == 0)
            {
               GOTO_TRAP(UnknownInstruction);
            }

            Function += ((opcode >> 11) & 0x07);
            switch (Function)
            {
               case MOVif:
               {
                  OpSize.Op[0] = ((opcode >> 8) & 3) + 1;                           // Source Size (Integer)
                  WriteSize    =
                  OpSize.Op[1] = GET_F_SIZE(opcode & BIT(10));                      // Destination Size (Float/ Double)
                  getgen(opcode >> 19, 0);                                          // Source Operand
                  getgen(opcode >> 14, 1);                                          // Destination Operand
                  Regs[1].RegType = GET_PRECISION(opcode & BIT(10));
               }
               break;

               case ROUND:
               case TRUNC:
               case FLOOR:
               {
                  OpSize.Op[0] = GET_F_SIZE(opcode & BIT(10));                      // Source Size (Float/ Double)
                  WriteSize =
                  OpSize.Op[1] = ((opcode >> 8) & 3) + 1;                           // Destination Size (Integer)
                  getgen(opcode >> 19, 0);                                          // Source Operand
                  getgen(opcode >> 14, 1);                                          // Destination Operand
                  Regs[0].RegType = GET_PRECISION(opcode & BIT(10));
               }
               break;

               case MOVFL:
               {
                  OpSize.Op[0] = sz32;
                  WriteSize =
                  OpSize.Op[1] = sz64;
                  getgen(opcode >> 19, 0);                                          // Source Operand
                  getgen(opcode >> 14, 1);                                          // Destination Operand
                  Regs[0].RegType = SinglePrecision;
                  Regs[1].RegType = DoublePrecision;
               }
               break;

               case MOVLF:
               {
                  OpSize.Op[0] = sz64;
                  WriteSize =
                  OpSize.Op[1] = sz32;
                  getgen(opcode >> 19, 0);                                          // Source Operand
                  getgen(opcode >> 14, 1);                                          // Destination Operand
                  Regs[0].RegType = DoublePrecision;
                  Regs[1].RegType = SinglePrecision;
               }
               break;

               case LFSR:
               {
                  SET_OP_SIZE(3);
                  getgen(opcode >> 19, 0);
               }
               break;

               case SFSR:
               {
                  SET_OP_SIZE(3);
                  getgen(opcode >> 14, 1);
               }
               break;

               default:
               {
                  PiWARN("Unexpected Format 9 Decode: Function = %"PRId32, Function);
               }
               break;
            }
         }
         break;

         case Format11:
         case Format12:
         {
            if (nscfg.fpu_flag == 0)
            {
               GOTO_TRAP(UnknownInstruction);
            }

            Function += ((opcode >> 10) & 0x0F);
            WriteSize    =
            OpSize.Op[0] =
            OpSize.Op[1] = GET_F_SIZE(opcode & BIT(8));
            getgen(opcode >> 19, 0);
            getgen(opcode >> 14, 1);
            Regs[0].RegType =
            Regs[1].RegType = GET_PRECISION(opcode & BIT(8));
         }
         break;

         case Format14:
         {
            Function += ((opcode >> 10) & 0x0F);
         }
         break;

         default:
         {
            SET_TRAP(UnknownFormat);
         }
         break;
      }

#ifdef PC_SIMULATION
      uint32_t Temp = pc;
      n32016_show_instruction(startpc, &Temp, opcode, Function, &OpSize);
#endif

      GetGenPhase2(Regs[0], 0);
      GetGenPhase2(Regs[1], 1);

      if (Function <= RETT)
      {
         temp = GetDisplacement(&pc);
      }

      if (TrapFlags)
      {
         DoTrap:
         HandleTrap();
         continue;
      }

#ifdef INSTRUCTION_PROFILING
      IP[startpc]++;
#endif

#ifdef PROFILING
      ProfileAdd(Function, Regs[0].Whole, Regs[1].Whole);
#endif

      switch (Function)
      {
         // Format 0 : Branches

         case BEQ:
         case BNE:
         case BCS:
         case BCC:
         case BH:
         case BLS:
         case BGT:
         case BLE:
         case BFS:
         case BFC:
         case BLO:
         case BHS:
         case BLT:
         case BGE:
         {
            if (CheckCondition(Function) == 0)
            {
               continue;
            }
         }
         // Fall Through

         case BR:
         {
            pc = startpc + temp;
            continue;
         }
         // No break due to continue

         case BN:
         {
            continue;
         }
         // No break due to continue

         // Format 1

         case BSR:
         {
            pushd(pc);
            pc = startpc + temp;
            continue;
         }
         // No break due to continue

         case RET:
         {
            pc = popd();
            INC_SP(temp);
            continue;
         }
         // No break due to continue

         case CXP:
         {
            temp2 = read_x32(mod + 4) + ((int32_t) temp) * 4;

            temp = read_x32(temp2);   // Matching Tail with CXPD, complier do your stuff
            pushd((CXP_UNUSED_WORD << 16) | mod);
            pushd(pc);
            mod = temp & 0xFFFF;
            temp3 = temp >> 16;
            sb = read_x32(mod);
            temp2 = read_x32(mod + 8);
            pc = temp2 + temp3;
            continue;
         }
         // No break due to continue

         case RXP:
         {
            pc = popd();
            temp2 = popd();
            mod = temp2 & 0xFFFF;
            INC_SP(temp);
            sb = read_x32(mod);
            continue;
         }
         // No break due to continue

         case RETT:
         {
            if (ReturnCommon())
            {
               GOTO_TRAP(PrivilegedInstruction);
            }

            INC_SP(temp);
            continue;
         }
         // No break due to continue

         case RETI:
         {
            // No "End of Interrupt" bus cycles here!
            if (ReturnCommon())
            {
               GOTO_TRAP(PrivilegedInstruction);
            }

            continue;
         }
         // No break due to continue

         case SAVE:
         {
            int c;
            temp = READ_PC_BYTE();

            for (c = 0; c < 8; c++)                             // Matching tail with ENTER
            {
               if (temp & BIT(c))
               {
                  pushd(r[c]);
               }
            }
            continue;
         }
         // No break due to continue

         case RESTORE:
         {
            PopRegisters();
            continue;
         }
         // No break due to continue

         case ENTER:
         {
            int c;
            temp = READ_PC_BYTE();
            temp2 = GetDisplacement(&pc);
            pushd(fp);
            fp = GET_SP();
            DEC_SP(temp2);

            for (c = 0; c < 8; c++)                              // Matching tail with SAVE
            {
               if (temp & BIT(c))
               {
                  pushd(r[c]);
               }
            }
            continue;
         }
         // No break due to continue

         case EXIT:
         {
            PopRegisters();
            SET_SP(fp);
            fp = popd();
            continue;
         }
         // No break due to continue

         case NOP:
         {
            continue;
         }
         // No break due to continue

         case WAIT:                                             // Wait for interrupt then continue execution
         case DIA:                                              // Wait for interrupt and in theory never resume execution (stack manipulation would get round this)
         {
            tubecycles = 0;                                    // Exit promptly as we are waiting for an interrupt
            pc = startpc;
            continue;
         }
         // No break due to continue

         case FLAG:
         {
            GOTO_TRAP(FlagInstruction);
         }
         // No break due to goto

         case SVC:
         {
            temp = psr;
            psr &= ~0x700;
            // In SVC, the address pushed is the address of the SVC opcode
            pushd((temp << 16) | mod);
            pushd(startpc);
            temp = read_x32(intbase + (5 * 4));
            mod = temp & 0xFFFF;
            temp3 = temp >> 16;
            sb = read_x32(mod);
            temp2 = read_x32(mod + 8);
            pc = temp2 + temp3;
            continue;
         }
         // No break due to continue

         case BPT:
         {
            GOTO_TRAP(BreakPointTrap);
         }
         // No break due to goto

         // Format 2

         case ADDQ:
         {
            temp2 = (opcode >> 7) & 0xF;
            NIBBLE_EXTEND(temp2);
            temp = ReadGen(0);

            SIGN_EXTEND(OpSize.Op[0], temp);
            temp = AddCommon(temp, temp2, 0);
         }
         break;

         case CMPQ:
         {
            temp2 = (opcode >> 7) & 0xF;
            NIBBLE_EXTEND(temp2);
            temp = ReadGen(0);
            SIGN_EXTEND(OpSize.Op[0], temp);
            CompareCommon(temp2, temp);
            continue;
         }
         // No break due to continue

         case SPR:
         {
            temp2 = (opcode >> 7) & 0xF;

            if (U_FLAG)
            {
               if (PrivilegedPSR(temp2))
               {
                  GOTO_TRAP(PrivilegedInstruction);
               }
            }

            switch (temp2)
            {
               case 0x0:
                  temp = psr & 0xff;
               break;
               case 0x8:
                  temp = fp;
               break;
               case 0x9:
                  temp = GET_SP();    // returned the currently selected stack pointer
               break;
               case 0xA:
                  temp = sb;
               break;
               case 0xB:
                  temp = sp[1];       // returns the user stack pointer
               break;
               case 0xD:
                  temp = psr;
               break;
               case 0xE:
                  temp = intbase;
               break;
               case 0xF:
                  temp = mod;
               break;

               default:
               {
                  GOTO_TRAP(IllegalSpecialReading);
               }
               // No break due to goto
            }
         }
         break;

         case Scond:
         {
            temp = CheckCondition(opcode >> 7);
         }
         break;

         case ACB:
         {
            temp2 = (opcode >> 7) & 0xF;
            NIBBLE_EXTEND(temp2);
            temp = ReadGen(0);
            temp += temp2;
            temp2 = GetDisplacement(&pc);
            if (Truncate(temp, OpSize.Op[0]))
               pc = startpc + temp2;
         }
         break;

         case MOVQ:
         {
            temp = (opcode >> 7) & 0xF;
            NIBBLE_EXTEND(temp);
         }
         break;

         case LPR:
         {
            temp  = ReadGen(0);
            temp2 = (opcode >> 7) & 0xF;

            if (U_FLAG)
            {
               if (PrivilegedPSR(temp2))
               {
                  GOTO_TRAP(PrivilegedInstruction);
               }
            }

            switch (temp2)
            {
               case 0:
               {
                  psr = (psr & 0xFF00) | (temp & 0xFF);
               }
               break;

               case 5:
               case 6:
               case 7:
               {
                  GOTO_TRAP(IllegalSpecialWriting);
               }
               // No break due to goto

               case 9:
               {
                  SET_SP(temp);   // Sets the currently selected stack pointer
               }
               break;

               case 11:
               {
                  sp[1] = temp;   // Sets the user stack pointer
               }
               break;

               default:
               {
                  PR.Direct[temp2] = temp;
               }
               break;
            }

            continue;
         }
         // No break due to continue

         // Format 3

         case CXPD:
         {
            temp2 = ReadAddress(0);

            temp = read_x32(temp2);   // Matching Tail with CXPD, complier do your stuff
            pushd((CXP_UNUSED_WORD << 16) | mod);
            pushd(pc);
            mod = temp & 0xFFFF;
            temp3 = temp >> 16;
            sb = read_x32(mod);
            temp2 = read_x32(mod + 8);
            pc = temp2 + temp3;
            continue;
         }
         // No break due to continue

         case BICPSR:
         {
            if (U_FLAG)
            {
               if (OpSize.Op[0] > sz8)
               {
                  GOTO_TRAP(PrivilegedInstruction);
               }
            }

            temp = ReadGen(0);
            psr &= ~temp;
            continue;
         }
         // No break due to continue

         case JUMP:
         {
            // JUMP is in access class addr, so ReadGen() cannot be used
            pc = ReadAddress(0);
            continue;
         }
         // No break due to continue

         case BISPSR:
         {
            if (U_FLAG)
            {
               if (OpSize.Op[0] > sz8)
               {
                  GOTO_TRAP(PrivilegedInstruction);
               }
            }

            temp = ReadGen(0);
            psr |= temp;
            continue;
         }
         // No break due to continue

         case ADJSP:
         {
            temp = ReadGen(0);
            SIGN_EXTEND(OpSize.Op[0], temp);
            DEC_SP(temp);
            continue;
         }
         // No break due to continue

         case JSR:
         {
            // JSR is in access class addr, so ReadGen() cannot be used
            pushd(pc);
            pc = ReadAddress(0);
            continue;
         }
         // No break due to continue

         case CASE:
         {
            temp = ReadGen(0);
            SIGN_EXTEND(OpSize.Op[0], temp);
            pc = startpc + temp;
            continue;
         }
         // No break due to continue

         // Format 4

         case ADD:
         {
            temp2 = ReadGen(0);
            temp = ReadGen(1);

            SIGN_EXTEND(OpSize.Op[0], temp2);
            SIGN_EXTEND(OpSize.Op[1], temp);
            temp = AddCommon(temp, temp2, 0);
         }
         break;

         case CMP:
         {
            temp2 = ReadGen(0);
            temp = ReadGen(1);
            CompareCommon(temp2, temp);
            continue;
         }
         // No break due to continue

         case BIC:
         {
            temp2 = ReadGen(0);
            temp = ReadGen(1);
            temp &= ~temp2;
         }
         break;

         case ADDC:
         {
            temp2 = ReadGen(0);
            temp = ReadGen(1);

            temp3 = C_FLAG;
            SIGN_EXTEND(OpSize.Op[0], temp2);
            SIGN_EXTEND(OpSize.Op[1], temp);
            temp = AddCommon(temp, temp2, temp3);
         }
         break;

         case MOV:
         {
            temp = ReadGen(0);
         }
         break;

         case OR:
         {
            temp = ReadGen(0);
            temp2 = ReadGen(1);
            temp |= temp2;
         }
         break;

         case SUB:
         {
            temp2 = ReadGen(0);
            temp = ReadGen(1);
            SIGN_EXTEND(OpSize.Op[0], temp2);
            SIGN_EXTEND(OpSize.Op[1], temp);
            temp = SubCommon(temp, temp2, 0);
         }
         break;

         case SUBC:
         {
            temp2 = ReadGen(0);
            temp = ReadGen(1);
            temp3 = C_FLAG;
            SIGN_EXTEND(OpSize.Op[0], temp2);
            SIGN_EXTEND(OpSize.Op[1], temp);
            temp = SubCommon(temp, temp2, temp3);
         }
         break;

         case ADDR:
         {
            temp = ReadAddress(0);
         }
         break;

         case AND:
         {
            temp2 = ReadGen(0);
            temp = ReadGen(1);
            temp &= temp2;
         }
         break;

         case TBIT:
         {
            temp2 = BitPrefix();
            if (gentype[1] == TOS)
            {
               PiWARN("TBIT with base==TOS is not yet implemented");
               continue; // with next instruction
            }
            temp = ReadGen(1);
            F_FLAG = TEST(temp & temp2);
            continue;
         }
         // No break due to continue

         case XOR:
         {
            temp = ReadGen(0);
            temp2 = ReadGen(1);
            temp ^= temp2;
         }
         break;

         // Format 5

         case MOVS:
         {
            if (r[0] == 0)
            {
               F_FLAG = 0;
               continue;
            }

            temp = read_n(r[1], OpSize.Op[0]);

            if (opcode & BIT(Translation))
            {
               temp = read_x8(r[3] + temp); // Lookup the translation
            }

            if (StringMatching(opcode, temp))
            {
               continue;
            }

            write_Arbitary(r[2], &temp, OpSize.Op[0]);

            StringRegisterUpdate(opcode);
            pc = startpc; // Not finsihed so come back again!
            continue;
         }
         // No break due to continue

         case CMPS:
         {
            if (r[0] == 0)
            {
               F_FLAG = 0;
               continue;
            }

            temp = read_n(r[1], OpSize.Op[0]);

            if (opcode & BIT(Translation))
            {
               temp = read_x8(r[3] + temp);                               // Lookup the translation
            }

            if (StringMatching(opcode, temp))
            {
               continue;
            }

            temp2 = read_n(r[2], OpSize.Op[0]);

            if (CompareCommon(temp, temp2) == 0)
            {
               continue;
            }

            StringRegisterUpdate(opcode);
            pc = startpc;                                               // Not finsihed so come back again!
            continue;
         }
         // No break due to continue

         case SETCFG:
         {
            if (U_FLAG)
            {
               GOTO_TRAP(PrivilegedInstruction);
            }

            nscfg.lsb = (opcode >> 15);                                  // Only sets the bottom 8 bits of which the lower 4 are used!
            continue;
         }
         // No break due to continue

         case SKPS:
         {
            if (r[0] == 0)
            {
               F_FLAG = 0;
               continue;
            }

            temp = read_n(r[1], OpSize.Op[0]);

            if (opcode & BIT(Translation))
            {
               temp = read_x8(r[3] + temp); // Lookup the translation
               write_x8(r[1], temp); // Write back
            }

            if (StringMatching(opcode, temp))
            {
               continue;
            }

            StringRegisterUpdate(opcode);
            pc = startpc; // Not finsihed so come back again!
            continue;
         }
         // No break due to continue

         // Format 6

         case ROT:
         {
            temp2 = ReadGen(0);
            temp  = ReadGen(1);

            WarnIfShiftInvalid(temp2,  OpSize.Op[1]);

#if 1
            temp3 = OpSize.Op[1] * 8;                             // Bit size, compiler will switch to a shift all by itself ;)

            if (temp2 & 0xE0)
            {
               temp2 |= 0xE0;
               temp2 = ((temp2 ^ 0xFF) + 1);
               temp2 = temp3 - temp2;
            }
            temp = (temp << temp2) | (temp >> (temp3 - temp2));

#else
            switch (OpSize.Op[1])
            {
               case sz8:
               {
                  if (temp2 & 0xE0)
                  {
                     temp2 |= 0xE0;
                     temp2 = ((temp2 ^ 0xFF) + 1);
                     temp2 = 8 - temp2;
                  }
                  temp = (temp << temp2) | (temp >> (8 - temp2));
               }
               break;

               case sz16:
               {
                  if (temp2 & 0xE0)
                  {
                     temp2 |= 0xE0;
                     temp2 = ((temp2 ^ 0xFF) + 1);
                     temp2 = 16 - temp2;
                  }
                  temp = (temp << temp2) | (temp >> (16 - temp2));
               }
               break;

               case sz32:
               {
                  if (temp2 & 0xE0)
                  {
                     temp2 |= 0xE0;
                     temp2 = ((temp2 ^ 0xFF) + 1);
                     temp2 = 32 - temp2;
                  }
                  temp = (temp << temp2) | (temp >> (32 - temp2));
               }
               break;
            }
#endif
         }
         break;

         case ASH:
         {
            temp2 = ReadGen(0);
            temp = ReadGen(1);

            WarnIfShiftInvalid(temp2,  OpSize.Op[1]);

            // Test if the shift is negative (i.e. a right shift)
            if (temp2 & 0xE0)
            {
               temp2 |= 0xE0;
               temp2 = ((temp2 ^ 0xFF) + 1);
               if (OpSize.Op[1] == sz8)
               {
                  // Test if the operand is also negative
                  if (temp & 0x80)
                  {
                     // Sign extend in a portable way
                     temp = (temp >> temp2) | ((0xFF >> temp2) ^ 0xFF);
                  }
                  else
                  {
                     temp = (temp >> temp2);
                  }
               }
               else if (OpSize.Op[1] == sz16)
               {
                  if (temp & 0x8000)
                  {
                     temp = (temp >> temp2) | ((0xFFFF >> temp2) ^ 0xFFFF);
                  }
                  else
                  {
                     temp = (temp >> temp2);
                  }
               }
               else
               {
                  if (temp & 0x80000000)
                  {
                     temp = (temp >> temp2) | ((0xFFFFFFFF >> temp2) ^ 0xFFFFFFFF);
                  }
                  else
                  {
                     temp = (temp >> temp2);
                  }
               }
            }
            else
               temp <<= temp2;
         }
         break;

         case LSH:
         {
            temp2 = ReadGen(0);
            temp = ReadGen(1);

            WarnIfShiftInvalid(temp2,  OpSize.Op[1]);

            if (temp2 & 0xE0)
            {
               temp2 |= 0xE0;
               temp >>= ((temp2 ^ 0xFF) + 1);
            }
            else
               temp <<= temp2;

         }
         break;

         case CBIT:
         case CBITI:
         {
            // The CBITI instructions, in addition, activate the Interlocked
            // Operation output pin on the CPU, which may be used in multiprocessor systems to
            // interlock accesses to semaphore bits. This aspect is not implemented here.
            temp2 = BitPrefix();
            temp = ReadGen(1);
            F_FLAG = TEST(temp & temp2);
            temp &= ~(temp2);
         }
         break;

         case SBIT:
         case SBITI:
         {
            // The SBITI instructions, in addition, activate the Interlocked
            // Operation output pin on the CPU, which may be used in multiprocessor systems to
            // interlock accesses to semaphore bits. This aspect is not implemented here.
            temp2 = BitPrefix();
            temp = ReadGen(1);
            F_FLAG = TEST(temp & temp2);
            temp |= temp2;
         }
         break;

         case NEG:
         {
            temp = 0;
            temp2 = ReadGen(0);
            SIGN_EXTEND(OpSize.Op[0], temp2);
            temp = SubCommon(temp, temp2, 0);
         }
         break;

         case NOT:
         {
            temp = ReadGen(0);
            temp ^= 1;
         }
         break;

         case SUBP:
         {
            uint32_t carry = C_FLAG;
            temp2 = ReadGen(0);
            temp = ReadGen(1);
            temp = bcd_sub(temp, temp2, OpSize.Op[0], &carry);
            C_FLAG = TEST(carry);
            F_FLAG = 0;
         }
         break;

         case ABS:
         {
            temp = ReadGen(0);
            switch (OpSize.Op[0])
            {
               case sz8:
               {
                  if (temp == 0x80)
                  {
                     F_FLAG = 1;
                  }
                  if (temp & 0x80)
                  {
                     temp = (temp ^ 0xFF) + 1;
                  }
               }
               break;

               case sz16:
               {
                  if (temp == 0x8000)
                  {
                     F_FLAG = 1;
                  }
                  if (temp & 0x8000)
                  {
                     temp = (temp ^ 0xFFFF) + 1;
                  }
               }
               break;

               case sz32:
               {
                  if (temp == 0x80000000)
                  {
                     F_FLAG = 1;
                  }
                  if (temp & 0x80000000)
                  {
                     temp = (temp ^ 0xFFFFFFFF) + 1;
                  }
               }
               break;
            }
         }
         break;

         case COM:
         {
            temp = ReadGen(0);
            temp = ~temp;
         }
         break;

         case IBIT:
         {
            temp2 = BitPrefix();
            temp = ReadGen(1);
            F_FLAG = TEST(temp & temp2);
            temp ^= temp2;
         }
         break;

         case ADDP:
         {
            uint32_t carry = C_FLAG;
            temp2 = ReadGen(0);
            temp = ReadGen(1);
            temp = bcd_add(temp, temp2, OpSize.Op[0], &carry);
            C_FLAG = TEST(carry);
            F_FLAG = 0;
         }
         break;

         // FORMAT 7

         case MOVM:
         {
            uint32_t First    = ReadAddress(0);
            uint32_t Second   = ReadAddress(1);
            //temp = GetDisplacement(&pc) + OpSize.Op[0];                      // disp of 0 means move 1 byte
            temp = (GetDisplacement(&pc) & ~(OpSize.Op[0] - 1))  + OpSize.Op[0];
            while (temp)
            {
               temp2 = read_x8(First);
               First++;
               write_x8(Second, temp2);
               Second++;
               temp--;
            }

            continue;
         }
         // No break due to continue

         case CMPM:
         {
            uint32_t temp4    = OpSize.Op[0];                                 // disp of 0 means move 1 byte/word/dword
            uint32_t First    = ReadAddress(0);
            uint32_t Second   = ReadAddress(1);

            temp3 = (GetDisplacement(&pc) / temp4) + 1;

            //PiTRACE("CMP Size = %u Count = %u", temp4, temp3);
            while (temp3--)
            {
               temp  = read_n(First, temp4);
               temp2 = read_n(Second, temp4);

               if (CompareCommon(temp, temp2) == 0)
               {
                  break;
               }

               First += temp4;
               Second += temp4;
            }

            continue;
         }
         // No break due to continue

         case INSS:
         {
            uint32_t c;

            // Read the immediate offset (3 bits) / length - 1 (5 bits) from the instruction
            temp3 = READ_PC_BYTE();
            temp = ReadGen(0); // src operand

            // The field can be upto 32 bits, and is independent of the opcode i bits
            OpSize.Op[1] = sz32;
            temp2 = ReadGen(1); // base operand
            for (c = 0; c <= (temp3 & 0x1F); c++)
            {
               temp2 &= ~(BIT((c + (temp3 >> 5)) & 31));
               if (temp & BIT(c))
               {
                  temp2 |= BIT((c + (temp3 >> 5)) & 31);
               }
            }
            temp = temp2;
            WriteSize = OpSize.Op[1];
         }
         break;

         case EXTS:
         {
            uint32_t c;
            uint32_t temp4 = 1;

            if (gentype[0] == TOS)
            {
               PiWARN("EXTS with base==TOS is not yet implemented");
               continue; // with next instruction
            }

            // Read the immediate offset (3 bits) / length - 1 (5 bits) from the instruction
            temp3 = READ_PC_BYTE();
            temp = ReadGen(0);
            temp2 = 0;
            temp >>= (temp3 >> 5); // Shift by offset
            temp3 &= 0x1F; // Mask off the lower 5 Bits which are number of bits to extract

            for (c = 0; c <= temp3; c++)
            {
               if (temp & temp4) // Copy the ones
               {
                  temp2 |= temp4;
               }

               temp4 <<= 1;
            }
            temp = temp2;
         }
         break;

         case MOVXiW:
         {
            if (OpSize.Op[0] != sz8)
            {
               PiWARN("MOVXiW forcing first Operand Size");
            }

            OpSize.Op[0] = sz8;
            temp = ReadGen(0);
            SIGN_EXTEND(sz8, temp); // Editor need the useless semicolon
            WriteSize = sz16;
         }
         break;

         case MOVZiW:
         {
            if (OpSize.Op[0] != sz8)
            {
               PiWARN("MOVZiW forcing first Operand Size");
            }

            OpSize.Op[0] = sz8;
            temp = ReadGen(0);
            WriteSize = sz16;
         }
         break;

         case MOVZiD:
         {
            temp = ReadGen(0);
            WriteSize = sz32;
         }
         break;

         case MOVXiD:
         {
            temp = ReadGen(0);
            SIGN_EXTEND(OpSize.Op[0], temp);
            WriteSize = sz32;
         }
         break;

         case MEI:
         {
            temp = ReadGen(0); // src
            temp64.u64 = ReadGen(1); // dst
            temp64.u64 *= temp;
            // Handle the writing to the upper half of dst locally here
            handle_mei_dei_upper_write(temp64.u64);
            // Allow fallthrough write logic to write the lower half of dst
            temp = (uint32_t) temp64.u64;
         }
         break;

         case MUL:
         {
            temp = ReadGen(0);
            temp2 = ReadGen(1);
            temp *= temp2;
         }
         break;

         case DEI:
         {
            int size = OpSize.Op[0] << 3;                      // 8, 16  or 32
            temp = ReadGen(0); // src
            if (temp == 0)
            {
               GOTO_TRAP(DivideByZero);
            }

            temp64.u64 = ReadGen64(1); // dst
            switch (OpSize.Op[0])
            {
               case sz8:
                  temp64.u64 = ((temp64.u64 >> 24) & 0xFF00) | (temp64.u64 & 0xFF);
                  break;

               case sz16:
                  temp64.u64 = ((temp64.u64 >> 16) & 0xFFFF0000) | (temp64.u64 & 0xFFFF);
                  break;
            }
            // PiTRACE("temp = %08x", temp);
            // PiTRACE("temp64.u64 = %016" PRIu64 , temp64.u64);
            temp64.u64 = ((temp64.u64 / temp) << size) | (temp64.u64 % temp);
            //PiTRACE("result = %016" PRIu64 , temp64.u64);
            // Handle the writing to the upper half of dst locally here
            handle_mei_dei_upper_write(temp64.u64);
            // Allow fallthrough write logic to write the lower half of dst
            temp = (uint32_t) temp64.u64;
         }
         break;

         case QUO:
         {
            temp = ReadGen(0);
            temp2 = ReadGen(1);
            if (temp == 0)
            {
               GOTO_TRAP(DivideByZero);
            }

            switch (OpSize.Op[0])
            {
               case sz8:
                  temp = (int8_t) temp2 / (int8_t) temp;
               break;

               case sz16:
                  temp = (int16_t) temp2 / (int16_t) temp;
               break;

               case sz32:
                  temp = (int32_t) temp2 / (int32_t) temp;
               break;
            }
         }
         break;

         case REM:
         {
            temp = ReadGen(0);
            temp2 = ReadGen(1);
            if (temp == 0)
            {
               GOTO_TRAP(DivideByZero);
            }

            switch (OpSize.Op[0])
            {
               case sz8:
                  temp = (int8_t) temp2 % (int8_t) temp;
               break;

               case sz16:
                  temp = (int16_t) temp2 % (int16_t) temp;
               break;

               case sz32:
                  temp = (int32_t) temp2 % (int32_t) temp;
               break;
            }
         }
         break;

         case MOD:
         {
            temp = ReadGen(0);
            temp2 = ReadGen(1);
            if (temp == 0)
            {
               GOTO_TRAP(DivideByZero);
            }

            temp = mod_operator(temp2, temp);
         }
         break;

         case DIV:
         {
            temp = ReadGen(0);
            temp2 = ReadGen(1);
            if (temp == 0)
            {
               GOTO_TRAP(DivideByZero);
            }

            temp = div_operator(temp2, temp);
         }
         break;

         // Format 8

         case EXT:
         {
            uint32_t c;
            int32_t  Offset = r[(opcode >> 11) & 7];
            uint32_t Length = GetDisplacement(&pc);
            uint32_t StartBit;

            if (Length < 1 || Length > 32)
            {
               PiWARN("EXT with length %08"PRIX32" is undefined", Length);
               continue; // with next instruction
            }

            if (gentype[0] == TOS)
            {
               // base is TOS
               //
               // This case is complicated because:
               //
               // 1. We need to avoid modifying the stack pointer.
               //
               // 2. We potentially need to take account of an offset.
               //
               PiWARN("EXT with base==TOS is not yet implemented; offset = %"PRId32, Offset);
               continue; // with next instruction
            }
            else if (gentype[0] == Register)
            {
               // base is a register
               StartBit = ((uint32_t) Offset) & 31;
            }
            else
            {
               // base is memory
               genaddr[0] += OffsetDiv8(Offset);
               StartBit = ((uint32_t) Offset) & 7;
            }

            OpSize.Op[0] = sz32;
            uint32_t Source = ReadGen(0);

            temp = 0;
            for (c = 0; (c < Length) && (c + StartBit < 32); c++)
            {
               if (Source & BIT(c + StartBit))
               {
                  temp |= BIT(c);
               }
            }
         }
         break;

         case CVTP:
         {
            int32_t Offset = r[(opcode >> 11) & 7];
            int32_t Base = ReadAddress(0);

            temp = (Base * 8) + Offset;
            WriteSize = sz32;
         }
         break;

         case INS:
         {
            uint32_t c;
            int32_t  Offset = r[(opcode >> 11) & 7];
            uint32_t Length = GetDisplacement(&pc);
            uint32_t Source = ReadGen(0);
            uint32_t StartBit;

            if (Length < 1 || Length > 32)
            {
               PiWARN("INS with length %08"PRIX32" is undefined", Length);
               continue; // with next instruction
            }

            if (gentype[1] == TOS)
            {
               // base is TOS
               //
               // This case is complicated because:
               //
               // 1. We need to avoid modifying the stack pointer,
               // which might not be an issue as we read then write.
               //
               // 2. We potentially need to take account of an offset. This
               // is harder as our current TOS read/write doesn't allow
               // for an offset. It's also not clear what this means.
               //
               PiWARN("INS with base==TOS is not yet implemented; offset = %"PRId32, Offset);
               continue; // with next instruction
            }
            else if (gentype[1] == Register)
            {
               // base is a register
               StartBit = ((uint32_t) Offset) & 31;
            }
            else
            {
               // base is memory
               genaddr[1] += OffsetDiv8(Offset);
               StartBit = ((uint32_t) Offset) & 7;
            }

            // The field can be upto 32 bits, and is independent of the opcode i bits
            OpSize.Op[1] = sz32;
            temp = ReadGen(1);
            for (c = 0; (c < Length) && (c + StartBit < 32); c++)
            {
               if (Source & BIT(c))
               {
                  temp |= BIT(c + StartBit);
               }
               else
               {
                  temp &= ~(BIT(c + StartBit));
               }
            }
            WriteSize = OpSize.Op[1];
         }
         break;

         case CHECK:
         {
            uint32_t ad = ReadAddress(0);
            temp3 = ReadGen(1);

            // Avoid a "might be uninitialized" warning
            temp2 = 0;
            switch (OpSize.Op[0])
            {
               case sz8:
               {
                  temp = read_x8(ad);
                  temp2 = read_x8(ad + 1);
               }
               break;

               case sz16:
               {
                  temp = read_x16(ad);
                  temp2 = read_x16(ad + 2);
               }
               break;

               case sz32:
               {
                  temp = read_x32(ad);
                  temp2 = read_x32(ad + 4);
               }
               break;
            }
            SIGN_EXTEND(OpSize.Op[0], temp);  // upper bound
            SIGN_EXTEND(OpSize.Op[0], temp2); // lower bound
            SIGN_EXTEND(OpSize.Op[0], temp3); // index

            //PiTRACE("Reg = %u Bounds [%u - %u] Index = %u", 0, temp, temp2, temp3);

            if (((signed int)temp >= (signed int)temp3) && ((signed int)temp3 >= (signed int)temp2))
            {
               r[(opcode >> 11) & 7] = temp3 - temp2;
               F_FLAG = 0;
            }
            else
            {
               F_FLAG = 1;
            }

            continue;
         }
         // No break due to continue

         case INDEX:
         {
            // r0, r1, r2
            // 5, 7, 0x13 (19)
            // accum = accum * (length+1) + index

            temp = r[(opcode >> 11) & 7]; // Accum
            temp2 = ReadGen(0) + 1; // (length+1)
            temp3 = ReadGen(1); // index

            r[(opcode >> 11) & 7] = (temp * temp2) + temp3;
            continue;
         }
         // No break due to continue

         case FFS:
         {
            uint32_t numbits = OpSize.Op[0] << 3;          // number of bits: 8, 16 or 32
            temp2 = ReadGen(0); // base is the variable size operand being scanned
            OpSize.Op[1] = sz8;
            temp = ReadGen(1); // offset is always 8 bits (also the result)
            // find the first set bit, starting at offset
            for (; temp < numbits && !(temp2 & BIT(temp)); temp++)
            {
               continue;                  // No Body!
            }

            if (temp < numbits)
            {
               // a set bit was found, return it in the offset operand
               F_FLAG = 0;
            }
            else
            {
               // no set bit was found, return 0 in the offset operand
               F_FLAG = 1;
               temp = 0;
            }

            WriteSize = sz8;
         }
         break;

         // Format 9
         case MOVif:
         {
            Temp32Type Src;
            Src.u32 = ReadGen(0);
            if (Regs[1].RegType == DoublePrecision)
            {
               temp64.f64 = (double) Src.s32;
            }
            else
            {
               Temp32Type q;
               q.f32 = (float) Src.s32;
               temp = q.u32;
            }
         }
         break;

         case LFSR:
         {
            FSR = ReadGen(0);
            continue;
         }
         // No break due to continue

         case MOVLF:
         {
            Temp32Type q;
            temp64.u64 = ReadGen64(0);
            q.f32 = (float) temp64.f64;
            temp = q.u32;
         }
         break;

         case MOVFL:
         {
            Temp32Type Src;
            Src.u32 = ReadGen(0);
            temp64.f64 = (double) Src.f32;
         }
         break;

         case ROUND:
         {
            if (Regs[0].RegType == DoublePrecision)
            {
               temp64.u64 = ReadGen64(0);
               temp = (int32_t) round(temp64.f64);
            }
            else
            {
               Temp32Type q;
               q.u32 = ReadGen(0);
               temp = (int32_t) roundf(q.f32);
            }
         }
         break;

         case TRUNC:
         {
            if (Regs[0].RegType == DoublePrecision)
            {
               temp64.u64 = ReadGen64(0);
               temp = (int32_t) temp64.f64;
            }
            else
            {
               Temp32Type q;
               q.u32 = ReadGen(0);
               temp = (int32_t) q.f32;
            }
         }
         break;

         case SFSR:
         {
            temp = FSR;
         }
         break;

         case FLOOR:
         {
            if (Regs[0].RegType == DoublePrecision)
            {
               temp64.u64 = ReadGen64(0);
               temp = (int32_t) floor(temp64.f64);
            }
            else
            {
               Temp32Type q;
               q.u32 = ReadGen(0);
               temp = (int32_t) floorf(q.f32);
            }
         }
         break;

         // Format 11
         case ADDf:
         {
            if (Regs[0].RegType == DoublePrecision)
            {
               Temp64Type Src;
               Src.u64     = ReadGen64(0);
               temp64.u64  = ReadGen64(1);

               temp64.f64 += Src.f64;
            }
            else
            {
               Temp32Type Src, Dst;
               Src.u32 = ReadGen(0);
               Dst.u32 = ReadGen(1);

               Dst.f32 += Src.f32;
               temp = Dst.u32;
            }
         }
         break;

         case MOVf:
         {
            if (Regs[0].RegType == DoublePrecision)
            {
               temp64.u64 = ReadGen64(0);
            }
            else
            {
               temp = ReadGen(0);
            }
         }
         break;

         case CMPf:
         {
            L_FLAG = 0;

            if (Regs[0].RegType == DoublePrecision)
            {
               Temp64Type Src;
               Src.u64 = ReadGen64(0);
               temp64.u64 = ReadGen64(1);

               Z_FLAG = TEST(Src.f64 == temp64.f64);
               N_FLAG = TEST(Src.f64 >  temp64.f64);
            }
            else
            {
               Temp32Type Src, Dst;
               Src.u32 = ReadGen(0);
               Dst.u32 = ReadGen(1);

               Z_FLAG = TEST(Src.f32 == Dst.f32);
               N_FLAG = TEST(Src.f32 >  Dst.f32);
            }
            continue;
         }
         // No break due to continue

         case SUBf:
         {
            if (Regs[0].RegType == DoublePrecision)
            {
               Temp64Type Src;
               Src.u64    = ReadGen64(0);
               temp64.u64 = ReadGen64(1);

               temp64.f64 -= Src.f64;
            }
            else
            {
               Temp32Type Src, Dst;
               Src.u32 = ReadGen(0);
               Dst.u32 = ReadGen(1);

               Dst.f32 -= Src.f32;
               temp = Dst.u32;
            }
         }
         break;

         case NEGf:
         {
            if (Regs[0].RegType == DoublePrecision)
            {
               Temp64Type Src;
               Src.u64 = ReadGen64(0);
               temp64.f64 = -Src.f64;
            }
            else
            {
               Temp32Type Src, Dst;
               Src.u32 = ReadGen(0);
               Dst.f32 = -Src.f32;
               temp = Dst.u32;
            }
         }
         break;

         case DIVf:
         {
            if (Regs[0].RegType == DoublePrecision)
            {
               Temp64Type Src;
               Src.u64 = ReadGen64(0);
               temp64.u64 = ReadGen64(1);

               temp64.f64 /= Src.f64;
            }
            else
            {
               Temp32Type Src, Dst;
               Src.u32 = ReadGen(0);
               Dst.u32 = ReadGen(1);

               Dst.f32 /= Src.f32;
               temp = Dst.u32;
            }
         }
         break;

         case MULf:
         {
            if (Regs[0].RegType == DoublePrecision)
            {
               Temp64Type Src;
               Src.u64 = ReadGen64(0);
               temp64.u64 = ReadGen64(1);

               temp64.f64 *= Src.f64;
            }
            else
            {
               Temp32Type Src, Dst;
               Src.u32 = ReadGen(0);
               Dst.u32 = ReadGen(1);

               Dst.f32 *= Src.f32;
               temp = Dst.u32;
            }
         }
         break;

         case ABSf:
         {
            if (Regs[0].RegType == DoublePrecision)
            {
               Temp64Type Src;
               Src.u64 = ReadGen64(0);
               temp64.f64 = fabs(Src.f64);
            }
            else
            {
               Temp32Type Src, Dst;
               Src.u32  = ReadGen(0);
               Dst.f32  = fabsf(Src.f32);
               temp     = Dst.u32;
            }
         }
         break;

         default:
         {
            if (Function < TRAP)
            {
               GOTO_TRAP(UnknownInstruction);
            }

            GOTO_TRAP(UnknownFormat);         // Probably already set but belt and braces here!
         }
         // No break due to goto
      }

      if (WriteSize && (WriteSize <= sz64))
      {
         switch (gentype[WriteIndex])
         {
            case Memory:
            {
               switch (WriteSize)
               {
                  case sz8:   write_x8( genaddr[WriteIndex], temp);  break;
                  case sz16:  write_x16(genaddr[WriteIndex], temp);  break;
                  case sz32:  write_x32(genaddr[WriteIndex], temp);  break;
                  case sz64:  write_x64(genaddr[WriteIndex], temp64.u64);  break;
               }
            }
            break;

            case Register:
            {
               switch (WriteSize)
               {
                  case sz8:   *((uint8_t*)   genreg[WriteIndex]) = temp;  break;
                  case sz16:  *((uint16_t*)  genreg[WriteIndex]) = temp;  break;
                  case sz32:  *((uint32_t*)  genreg[WriteIndex]) = temp;  break;
                  case sz64:  *((uint64_t*)  genreg[WriteIndex]) = temp64.u64;  break;
               }

#ifdef PC_SIMULATION
               if (WriteSize <= sz32)
               {
                  ShowRegisterWrite(Regs[WriteIndex], Truncate(temp, WriteSize));
               }
#endif
            }
            break;

            case TOS:
            {
               if (WriteSize == sz64)
               {
                  PushArbitary(temp64.u64, WriteSize);
               }
               else
               {
                  PushArbitary(temp, WriteSize);
               }
            }
            break;

            case OpImmediate:
            {
               GOTO_TRAP(IllegalWritingImmediate);
            }
         }
      } else {
         PiWARN("Bad write size: %d pc=%08"PRIX32" opcode=%08"PRIX32, WriteSize, startpc, opcode);
      }

#if 0
      switch (Regs[1].RegType)
      {
         case SinglePrecision:
         {
            n32016_ShowRegs(BIT(2));
         }
         break;

         case DoublePrecision:
         {
            n32016_ShowRegs(BIT(3));
         }
      }
#endif
   }
}
