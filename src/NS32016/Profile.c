#include <stdint.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "32016.h"
#include "Decode.h"
#include "mem32016.h"
#include "defs.h"
//#include "Trap.h"
#include "NDis.h"
#include "Profile.h"

#ifdef PROFILING

#define NUM_OPERAND_TYPES 80

uint32_t Frequencies[InstructionCount][NUM_OPERAND_TYPES][NUM_OPERAND_TYPES];


const char operandStrings[NUM_OPERAND_TYPES][20] =
{
   "--none--"        ,  //  0
   "--error--"       ,  //  1
   "RN"              ,  //  2
   "disp(RN)"        ,  //  3
   "disp2(disp1(FP))",  //  4
   "disp2(disp1(SP))",  //  5
   "disp2(disp1(SB))",  //  6
   "--illegal--"     ,  //  7
   "value"           ,  //  8
   "@disp"           ,  //  9
   "EXT(disp1)+disp2",  // 10
   "TOS"             ,  // 11
   "disp(FP)"        ,  // 12
   "disp(SP)"        ,  // 13
   "disp(SB)"        ,  // 14
   "*+disp"          ,  // 15
};

void ProfileInit(void)
{
   memset(Frequencies, 0, sizeof(Frequencies));
}

uint16_t processOperand(uint16_t operand)
{
   // Mask off bits 15..8 which carry the base mode in indexed modes
   if (operand == 0xFFFF)
   {
      return 0; // --none--
   }
   else if (operand >= 0 && operand <= 7)
   {
      return 2; // RN
   }
   else if (operand >= 8 && operand <= 15)
   {
      return 3; // disp(RN)
   }
   else if (operand >= 16 && operand <= 27)
   {
      return operand - 12; // everything else
   }
   else if ((operand & 0xff) >= 28 && (operand & 0xff) <= 31)
   {
      return 16 + ((operand & 0xff) - 27) * 16 + processOperand(operand >> 11) ; // scaled indexed
   }
   else
   {
      return 1; // --error--
   }
}

void ProfileAdd(uint32_t Function, uint16_t Regs0, uint16_t Regs1)
{
   if (Function < InstructionCount)
   {
      Regs0 = processOperand(Regs0);
      Regs1 = processOperand(Regs1);
      Frequencies[Function][Regs0][Regs1]++;
   }
}

char *operandText(uint16_t Reg)
{
   static char result[80];
   static char mode[4] = "BWDQ";
   if (Reg < 16)
   {
      sprintf(result, "%s", operandStrings[Reg]);
   }
   else
   {
      sprintf(result, "%s[Rn:%c]", operandStrings[Reg & 15], mode[(Reg >> 4) - 1]);
   }
   return result;
}

void ProfileDump(void)
{
   uint32_t Function;
   uint16_t Regs0;
   uint16_t Regs1;
   uint32_t total;

   printf("\"Function\", \"Name\", \"Operand 0\", \"Operand 1\", \"Frequency\"\n");

   for (Function = 0; Function < InstructionCount; Function++)
   {

      // Skip the TRAP instruction as it's not real
      if (strcmp(InstuctionText[Function], "TRAP") == 0)
         continue;

      total = 0;
      for (Regs0 = 0; Regs0 < NUM_OPERAND_TYPES; Regs0++)
         for (Regs1 = 0; Regs1 < NUM_OPERAND_TYPES; Regs1++)
            if (Frequencies[Function][Regs0][Regs1])
               total++;
      if (total) {
         // Break down of addressing modes
         for (Regs0 = 0; Regs0 < NUM_OPERAND_TYPES; Regs0++)
            for (Regs1 = 0; Regs1 < NUM_OPERAND_TYPES; Regs1++)
               if (Frequencies[Function][Regs0][Regs1])
               {
                  printf("%02" PRIX32 ", ", Function);
                  printf("%-8s, ", InstuctionText[Function]);
                  printf("%-24s, ", operandText(Regs1));
                  printf("%-24s, ", operandText(Regs0));
                  printf("%9" PRIu32 "\n", Frequencies[Function][Regs0][Regs1]);
               }
         printf("\n");
      }
   }
}
#endif
