#include <stdint.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include "defs.h"
#include "32016.h"
#include "mem32016.h"
#include "Trap.h"
#include "Decode.h"
#include "NSDis.h"

#define HEX24 "x'%06" PRIX32
#define HEX32 "x'%" PRIX32

#define MAX_INSTR_SIZE 8

// #define ADD_ASCII

static inline uint32_t read_x32_internal(uint32_t addr) {
   return read_x8(addr) | (read_x8(addr + 1) << 8) | (read_x8(addr + 2) << 16) | (read_x8(addr + 3) << 24);
}

static const char LPRLookUp[16][20] =
{
   "UPSR",
   "DCR",
   "BPC",
   "DSR",
   "CAR",
   "{0101}",
   "{0110}",
   "{0111}",
   "FP",
   "SP",
   "SB",
   "USP",
   "CFG",
   "PSR",
   "INTBASE",
   "MOD"
};

static char *str_buf;
static size_t str_bufsize;

static void StringInit(char *buf, size_t bufsize) {
   str_buf = buf;
   str_bufsize = bufsize;
}


static void StringAppend(const char *fmt, ...) {
   int len;
   va_list argptr;
   va_start(argptr, fmt);
   len = vsnprintf(str_buf, str_bufsize, fmt, argptr);
   str_buf += len;
   str_bufsize -= len;
   va_end(argptr);
}


#if 0
static void PostfixLookup(uint8_t Postfix)
{
   char PostFixLk[] = "BWTD";

   if (Postfix)
   {
      Postfix--;
      StringAppend("%c", PostFixLk[Postfix & 3]);
   }
}
#endif


static void AddStringFlags(uint32_t opcode)
{
   if (opcode & (BIT(Backwards) | BIT(UntilMatch) | BIT(WhileMatch)))
   {
      StringAppend("[");
      if (opcode & BIT(Backwards))
      {
         StringAppend("B");
      }

      uint32_t Options = (opcode >> 17) & 3;
      if (Options == 1) // While match
      {
         StringAppend("W");
      }
      else if (Options == 3)
      {
         StringAppend("U");
      }

      StringAppend("]");
   }
}

static void AddCfgFLags(uint32_t opcode)
{
   StringAppend("[");
   if (opcode & BIT(15))   StringAppend("I");
   if (opcode & BIT(16))   StringAppend("F");
   if (opcode & BIT(17))   StringAppend("M");
   if (opcode & BIT(18))   StringAppend("C");
   StringAppend("]");
}

static const char InstuctionText[InstructionCount][8] =
{
   // FORMAT 0
   "BEQ", "BNE", "BCS", "BCC", "BH", "BLS", "BGT", "BLE", "BFS", "BFC", "BLO", "BHS", "BLT", "BGE", "BR", "BN",

   // FORMAT 1
   "BSR", "RET", "CXP", "RXP", "RETT", "RETI", "SAVE", "RESTORE", "ENTER", "EXIT", "NOP", "WAIT", "DIA", "FLAG", "SVC", "BPT",

   // FORMAT 2
   "ADDQ", "CMPQ", "SPR", "Scond", "ACB", "MOVQ", "LPR", "TRAP", "TRAP", "TRAP", "TRAP", "TRAP", "TRAP", "TRAP", "TRAP", "TRAP",

   // FORMAT 3
   "CXPD", "TRAP", "BICPSR", "TRAP", "JUMP", "TRAP", "BISPSR", "TRAP", "TRAP", "TRAP", "ADJSP", "TRAP", "JSR", "TRAP", "CASE", "TRAP",

   // FORMAT 4
   "ADD", "CMP", "BIC", "TRAP", "ADDC", "MOV", "OR", "TRAP", "SUB", "ADDR", "AND", "TRAP", "SUBC", "TBIT", "XOR", "TRAP",

   // FORMAT 5
   "MOVS", "CMPS", "SETCFG", "SKPS", "TRAP", "TRAP", "TRAP", "TRAP", "TRAP", "TRAP", "TRAP", "TRAP", "TRAP", "TRAP", "TRAP", "TRAP",

   // FORMAT 6
   "ROT", "ASH", "CBIT", "CBITI", "TRAP", "LSH", "SBIT", "SBITI", "NEG", "NOT", "TRAP", "SUBP", "ABS", "COM", "IBIT", "ADDP",

   // FORMAT 7
   "MOVM", "CMPM", "INSS", "EXTS", "MOVX", "MOVZ", "MOVZ", "MOVX", "MUL", "MEI", "Trap", "DEI", "QUO", "REM", "MOD", "DIV", "TRAP"

   // FORMAT 8
   "EXT", "CVTP", "INS", "CHECK", "INDEX", "FFS", "MOVUS", "MOVSU", "TRAP", "TRAP", "TRAP", "TRAP", "TRAP", "TRAP", "TRAP", "TRAP",

   // FORMAT 9
   "MOV", "LFSR", "MOVLF", "MOVFL", "ROUND", "TRUNC", "SFSR", "FLOOR", "TRAP", "TRAP", "TRAP", "TRAP", "TRAP", "TRAP", "TRAP", "TRAP",

   // FORMAT 10
   "TRAP", "TRAP", "TRAP", "TRAP", "TRAP", "TRAP", "TRAP", "TRAP", "TRAP", "TRAP", "TRAP", "TRAP", "TRAP", "TRAP", "TRAP", "TRAP",

   // FORMAT 11
   "ADDf", "MOVf", "CMPf", "TRAP", "SUBf", "NEGf", "TRAP", "TRAP", "DIVf", "TRAP", "TRAP", "TRAP", "MULf", "ABSf", "TRAP", "TRAP",

   // FORMAT 12
   "TRAP", "TRAP", "POLY", "DOT", "SCALB", "LOGB", "TRAP", "TRAP", "TRAP", "TRAP", "TRAP", "TRAP", "TRAP", "TRAP", "TRAP", "TRAP",

   // FORMAT 13
   "TRAP", "TRAP", "TRAP", "TRAP", "TRAP", "TRAP", "TRAP", "TRAP", "TRAP", "TRAP", "TRAP", "TRAP", "TRAP", "TRAP", "TRAP", "TRAP",

   // FORMAT 14
   "RDVAL", "WRVAL", "LMR", "SMR", "TRAP", "TRAP", "TRAP", "TRAP", "TRAP", "CINV", "TRAP", "TRAP", "TRAP", "TRAP", "TRAP", "TRAP",

   // Illegal
   "TRAP"
};

static void GetOperandText(uint32_t Start, uint32_t* pPC, RegLKU Pattern, uint32_t c, OperandSizeType *OperandSize)
{
   const char RegLetter[] = "RFD*****";

  if (Pattern.OpType < 8)
  {
      StringAppend("%c%0" PRId32, RegLetter[Pattern.RegType], Pattern.OpType);
   }
   else if (Pattern.Whole < 16)
   {
      int32_t d = GetDisplacement(pPC);
      StringAppend("%0" PRId32 "(R%u)", d, (Pattern.Whole & 7));
   }
   else
   {
      switch (Pattern.Whole & 0x1F)
      {
         case FrameRelative:
         {
            int32_t d1 = GetDisplacement(pPC);
            int32_t d2 = GetDisplacement(pPC);
            StringAppend("%" PRId32 "(%" PRId32 "(FP))", d2, d1);
         }
         break;

         case StackRelative:
         {
            int32_t d1 = GetDisplacement(pPC);
            int32_t d2 = GetDisplacement(pPC);
            StringAppend("%" PRId32 "(%" PRId32 "(SP))", d2, d1);
         }
         break;

         case StaticRelative:
         {
            int32_t d1 = GetDisplacement(pPC);
            int32_t d2 = GetDisplacement(pPC);
            StringAppend("%" PRId32 "(%" PRId32 "(SB))", d2 , d1);
         }
         break;

         case IllegalOperand:
         {
            StringAppend("(reserved)");
         }
         break;

         case Immediate:
         {
            int32_t Value;
            MultiReg temp3;

            temp3.u32 = SWAP32(read_x32_internal(*pPC));
            if (OperandSize->Op[c] == sz8)
               Value = temp3.u8;
            else if (OperandSize->Op[c] == sz16)
               Value = temp3.u16;
            else
               Value = temp3.u32;

            (*pPC) += OperandSize->Op[c];
            StringAppend(HEX32, Value);
         }
         break;

         case Absolute:
         {
            int32_t d = GetDisplacement(pPC);
            StringAppend("@" HEX32, d);
         }
         break;

         case External:
         {
            int32_t d1 = GetDisplacement(pPC);
            int32_t d2 = GetDisplacement(pPC);
            StringAppend("EXT(" HEX32 ")+" HEX32, d1, d2);
         }
         break;

         case TopOfStack:
         {
            StringAppend("TOS");
         }
         break;

         case FpRelative:
         {
            int32_t d = GetDisplacement(pPC);
            StringAppend("%" PRId32 "(FP)", d);
         }
         break;

         case SpRelative:
         {
            int32_t d = GetDisplacement(pPC);
            StringAppend("%" PRId32 "(SP)", d);
         }
         break;

         case SbRelative:
         {
            int32_t d = GetDisplacement(pPC);
            StringAppend("%" PRId32 "(SB)", d);
         }
         break;

         case PcRelative:
         {
            int32_t d = GetDisplacement(pPC);
#if 1
            StringAppend("* + %" PRId32, d);

#else
            StringAppend("&06%" PRIX32 "[PC]", Start + d);
#endif
         }
         break;

         case EaPlusRn:
         case EaPlus2Rn:
         case EaPlus4Rn:
         case EaPlus8Rn:
         {
            const char SizeLookup[] = "BWDQ";
            RegLKU NewPattern;
            NewPattern.Whole = Pattern.Whole >> 11;
            GetOperandText(Start, pPC, NewPattern, c, OperandSize);   // Recurse
            StringAppend("[R%" PRId16 ":%c]", ((Pattern.Whole >> 8) & 3), SizeLookup[Pattern.Whole & 3]);
         }
         break;
      }
   }
}

static void RegLookUp(uint32_t Start, uint32_t* pPC, OperandSizeType *OperandSize)
{
   // printf("RegLookUp(%06" PRIX32 ", %06" PRIX32 ")\n", pc, (*pPC));
   uint32_t Index;

   for (Index = 0; Index < 2; Index++)
   {
      if (Regs[Index].Whole < 0xFFFF)
      {
         if (Index == 1)
         {
            if (Regs[0].Whole < 0xFFFF)
            {
               StringAppend(",");
            }
         }

         GetOperandText(Start, pPC, Regs[Index], Index, OperandSize);
      }
   }
}

static void ShowRegs(uint8_t Pattern, uint8_t Reverse)
{
   uint32_t Count;
   uint32_t First = 1;

   StringAppend("[");

   for (Count = 0; Count < 8; Count++)
   {
      if (Pattern & BIT(Count))
      {
         if (First == 0)
         {
            StringAppend(",");
         }

         if (Reverse)
         {
            StringAppend("R%" PRIu32, Count ^ 7);
         }
         else
         {
            StringAppend("R%" PRIu32, Count);
         }
         First = 0;
      }
   }

   StringAppend("]");
}

static const char PostFixLk[] = "BWTD";
static const char PostFltLk[] = "123F5678";
static const char EightSpaces[] = "        ";

static void AddInstructionText(uint32_t Function, uint32_t opcode, uint32_t OperandSize)
{
   if (Function < InstructionCount)
   {
      char Str[80];

      switch (Function)
      {
         case Scond:
         {
            uint32_t Condition = ((opcode >> 7) & 0x0F);
            sprintf(Str, "S%s", &InstuctionText[Condition][1]);             // Offset by 1 to lose the 'B'
         }
         break;

         case SFSR:
         case LFSR:
         {
            OperandSize = 0;
         }
         // Fall Through

         default:
         {
            sprintf(Str, "%s", InstuctionText[Function]);
         }
      }

      if ((opcode & 0x80FF) == 0x800E)
      {
         OperandSize = Translating;
      }

      if (OperandSize)
      {
         OperandSize--;
         uint32_t Format = Function >> 4;

         if (Format >= 9)
         {
            sprintf(Str + strlen(Str), "%c", PostFltLk[OperandSize & 3]);                 // Offset by 1 to loose the 'B'
         }
         else
         {
            sprintf(Str + strlen(Str), "%c", PostFixLk[OperandSize & 3]);                 // Offset by 1 to loose the 'B'
         }
      }

      switch (Function)
      {
         case MOVXiW:
         case MOVZiW:
         {
            sprintf(Str + strlen(Str), "W");
         }
         break;

         case MOVZiD:
         case MOVXiD:
         {
            sprintf(Str + strlen(Str), "D");
         }
         break;
      }

      size_t Len = strlen(Str);
      if (Len < (sizeof(EightSpaces) - 1))
      {
         StringAppend("%s%s", Str, &EightSpaces[Len]);
      }
   }
}

#ifdef ADD_ASCII
static void AddASCII(uint32_t opcode, uint32_t Format)
{
   if (Format < sizeof(FormatSizes))
   {
      uint32_t Len = FormatSizes[Format];
      uint32_t Count;

      for (Count = 0; Count < 4; Count++)
      {
         if (Count < Len)
         {
            uint8_t Data = opcode & 0xFF;
            StringAppend("%c", (Data < 0x20) ? '.' : Data);
            opcode >>= 8;
         }
         else
         {
            StringAppend(" ");
         }
      }
   }
}
#endif

void n32016_show_instruction(uint32_t StartPc, uint32_t* pPC, uint32_t opcode, uint32_t Function, OperandSizeType *OperandSize)
{
   int i;
   static uint32_t old_pc = 0xFFFFFFFF;

   if (StartPc < (IO_BASE - 64))                     // The code will not work near the IO Space as it will have side effects
   {
      if (StartPc == old_pc)
      {
         switch (Function)
         {
            case MOVS:
            case WAIT:                                             // Wait for interrupt then continue execution
            case DIA:                                              // Wait for interrupt and in theory never resume execution (stack manipulation would get round this)
            case CMPS:
            case SKPS:
            {
               return;                                            // This is just another iteration of an interruptable instructions
            }
            // No break due to return
         }
      }

      old_pc = StartPc;

      StringAppend("&%06" PRIX32 " ", StartPc);
      StringAppend("[");
      for (i = 0; i < MAX_INSTR_SIZE; i++) {
         StringAppend("%02x", read_x8_internal(StartPc + i));
      }
      StringAppend("] ");
      uint32_t Format = Function >> 4;
      StringAppend("F%01" PRIu32 " ", Format);
#ifdef ADD_ASCII
      AddASCII(opcode, Format);
#endif
      if (Function < InstructionCount)
      {
         AddInstructionText(Function, opcode, OperandSize->Op[0]);

         switch (Function)
         {
            case ADDQ:
            case CMPQ:
            case ACB:
            case MOVQ:
            {
               int32_t Value = (opcode >> 7) & 0xF;
               NIBBLE_EXTEND(Value);
               StringAppend("%" PRId32 ",", Value);
            }
            break;

            case LPR:
            case SPR:
            {
               int32_t Value = (opcode >> 7) & 0xF;
               StringAppend("%s", LPRLookUp[Value]);
               if (Function == LPR)
               {
                  StringAppend(",");
               }
            }
            break;
         }

         RegLookUp(StartPc, pPC, OperandSize);

         if ((Function <= BN) || (Function == BSR))
         {
            int32_t d = GetDisplacement(pPC);
            StringAppend("&%06"PRIX32" ", StartPc + d);
         }

         switch (Function)
         {
            case SAVE:
            {
               ShowRegs(read_x8_internal((*pPC)++), 0);    //Access directly we do not want tube reads!
            }
            break;

            case RESTORE:
            {
               ShowRegs(read_x8_internal((*pPC)++), 1);    //Access directly we do not want tube reads!
            }
            break;

            case EXIT:
            {
               ShowRegs(read_x8_internal((*pPC)++), 1);    //Access directly we do not want tube reads!
            }
            break;

            case ENTER:
            {
               ShowRegs(read_x8_internal((*pPC)++), 0);    //Access directly we do not want tube reads!
               int32_t d = GetDisplacement(pPC);
               StringAppend(" " HEX32 "", d);
            }
            break;

            case RET:
            case CXP:
            case RXP:
            {
               int32_t d = GetDisplacement(pPC);
               StringAppend(" " HEX32 "", d);
            }
            break;

            case ACB:
            {
               int32_t d = GetDisplacement(pPC);
               StringAppend("PC x+'%" PRId32 "", d);
            }
            break;

            case MOVM:
            case CMPM:
            {
               int32_t d = GetDisplacement(pPC);
               StringAppend(",%" PRId32, (d / OperandSize->Op[0]) + 1);
            }
            break;

            case EXT:
            case INS:
            {
               int32_t d = GetDisplacement(pPC);
               StringAppend(",%" PRId32, d);
            }
            break;

            case MOVS:
            case CMPS:
            case SKPS:
            {
               AddStringFlags(opcode);
            }
            break;

            case SETCFG:
            {
               AddCfgFLags(opcode);
            }
            break;

            case INSS:
            case EXTS:
            {
               uint8_t Value = read_x8_internal((*pPC)++);
               StringAppend(",%" PRIu32 ",%" PRIu32,  Value >> 5, ((Value & 0x1F) + 1));
            }
            break;
         }

      }

      return;
   }

   //PiTRACE("PC is :%08"PRIX32" ?????\n", *pPC);
}

void ShowRegisterWrite(RegLKU RegIn, uint32_t Value)
{
   if (RegIn.OpType < 8)
   {
#ifdef SHOW_REG_WRITES
      if (RegIn.RegType == Integer)
      {
         PiTRACE(" R%u = %"PRIX32"\n", RegIn.OpType, Value);
      }
#endif

#ifdef TEST_SUITE
      if (RegIn.OpType == 7)
      {
         PiTRACE("*** TEST = %u\n", Value);

#if 0
         if (Value == 137)
         {
            PiTRACE("*** BREAKPOINT\n");
         }
#endif
      }
#endif
   }
}

static void getgen(int gen, int c, uint32_t* pPC)
{
   gen &= 0x1F;
   Regs[c].Whole = gen;

   if (gen >= EaPlusRn)
   {
      Regs[c].Whole |= read_x8_internal((*pPC)++) << 8;
      (*pPC)++;

      if ((Regs[c].Whole & 0xF800) == (Immediate << 11))
      {
         SET_TRAP(IllegalImmediate);
      }

      if ((Regs[c].Whole & 0xF800) >= (EaPlusRn << 11))
      {
         SET_TRAP(IllegalDoubleIndexing);
      }
   }
}

#define SET_OPERAND_SIZE(in) OperandSize.Whole = OpSizeLookup[(in) & 0x03]

static void Decode(uint32_t* pPC)
{
   uint32_t StartPc = *pPC;
   uint32_t opcode = read_x32_internal(*pPC);
   uint32_t Function = FunctionLookup[opcode & 0xFF];
   uint32_t Format = Function >> 4;
   OperandSizeType OperandSize;


   Regs[0].Whole =
   Regs[1].Whole = 0xFFFF;

   if (Format < (FormatCount + 1))
   {
      *pPC += FormatSizes[Format];                                        // Add the basic number of bytes for a particular instruction
   }

   OperandSize.Whole = 0;
   switch (Format)
   {
      case Format0:
      case Format1:
      {
         // Nothing here
      }
      break;

      case Format2:
      {
         SET_OPERAND_SIZE(opcode);
         getgen(opcode >> 11, 0, pPC);
      }
      break;

      case Format3:
      {
         Function += ((opcode >> 7) & 0x0F);
         SET_OPERAND_SIZE(opcode);
         getgen(opcode >> 11, 0, pPC);
      }
      break;

      case Format4:
      {
         SET_OPERAND_SIZE(opcode);
         getgen(opcode >> 11, 0, pPC);
         getgen(opcode >> 6, 1, pPC);
      }
      break;

      case Format5:
      {
         Function += ((opcode >> 10) & 0x0F);
         SET_OPERAND_SIZE(opcode >> 8);
         if (opcode & BIT(Translation))
         {
            SET_OPERAND_SIZE(0);         // 8 Bit
         }
      }
      break;

      case Format6:
      {
         Function += ((opcode >> 10) & 0x0F);
         SET_OPERAND_SIZE(opcode >> 8);

         // Ordering important here, as getgen uses Operand Size
         switch (Function)
         {
            case ROT:
            case ASH:
            case LSH:
            {
               OperandSize.Op[0] = sz8;
            }
            break;
         }

         getgen(opcode >> 19, 0, pPC);
         getgen(opcode >> 14, 1, pPC);
      }
      break;

      case Format7:
      {
         Function += ((opcode >> 10) & 0x0F);
         SET_OPERAND_SIZE(opcode >> 8);
         getgen(opcode >> 19, 0, pPC);
         getgen(opcode >> 14, 1, pPC);
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

         SET_OPERAND_SIZE(opcode >> 8);

         if (Function == CVTP)
         {
            SET_OPERAND_SIZE(3);               // 32 Bit
         }

         getgen(opcode >> 19, 0, pPC);
         getgen(opcode >> 14, 1, pPC);
      }
      break;

      case Format9:
      {
         Function += ((opcode >> 11) & 0x07);
      }
      break;

      case Format11:
      {
         Function += ((opcode >> 10) & 0x0F);
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

   n32016_show_instruction(StartPc, pPC, opcode, Function, &OperandSize);
}


uint32_t n32016_disassemble(uint32_t addr, char *buf, size_t bufsize)
{
   int i;
   uint32_t old = addr;
   int len;
   StringInit(buf, bufsize);
   Decode(&addr);
   len = addr - old;
   // Nuke the op bytes that are part of next instruction
   for (i = 9 + len * 2; i < 9 + MAX_INSTR_SIZE * 2 && i < bufsize - 1; i++) {
      buf[i] = ' ';
   }
   //ShowTraps();
   //CLEAR_TRAP();
   return addr;
}
