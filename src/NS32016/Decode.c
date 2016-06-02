#include <stdint.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "32016.h"
#include "Profile.h"
#include "Trap.h"
#include "defs.h"
#include "Decode.h"

uint8_t FunctionLookup[256];

const uint8_t FormatSizes[FormatCount + 1] =
{
   1, 1, 2, 2, 2, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 1
};

#define FUNC(FORMAT, OFFSET) (((FORMAT) << 4) + (OFFSET)) 

uint8_t GetFunction(uint8_t FirstByte)
{
   switch (FirstByte & 0x0F)
   {
      case 0x0A:
         return FUNC(Format0, FirstByte >> 4);
      case 0x02:
         return FUNC(Format1, FirstByte >> 4);

      case 0x0C:
      case 0x0D:
      case 0x0F:
      {
         if ((FirstByte & 0x70) != 0x70)
         {
            return FUNC(Format2, (FirstByte >> 4) & 0x07);
         }

         return FUNC(Format3, 0);
      }
      // No break due to return
   }

   if ((FirstByte & 0x03) != 0x02)
   {
      return FUNC(Format4, (FirstByte >> 2) & 0x0F);
   }

   switch (FirstByte)
   {
      case 0x0E:
         return FUNC(Format5, 0); // String instruction
      case 0x4E:
         return FUNC(Format6, 0);
      case 0xCE:
         return FUNC(Format7, 0);
      CASE4(0x2E):
         return FUNC(Format8, 0);
      case 0x3E:
         return FUNC(Format9, 0);
      case 0x7E:
         return FUNC(Format10, 0);
      case 0xBE:
         return FUNC(Format11, 0);
      case 0xFE:
         return FUNC(Format12, 0);
      case 0x9E:
         return FUNC(Format13, 0);
      case 0x1E:
         return FUNC(Format14, 0);
   }

   return FormatBad;
}

void n32016_build_matrix()
{
   uint32_t Index;

   for (Index = 0; Index < 256; Index++)
   {
      FunctionLookup[Index] = GetFunction(Index);
   }
}

void BreakPoint(uint32_t pc, uint32_t opcode)
{
#if 1
#ifndef TEST_SUITE
   // Exec address of Bas32
   if (pc == 0x000200)
   {
      printf("Entering Bas32\n");
      ProfileInit();
   }
   // Exec address of Panos
   if (pc == 0x000400)
   {
      printf("Entering Panos\n");
      ProfileInit();
   }
   // Address of SVC &11 (OS_EXIT)
   if (pc == 0xF007BB)
   {
      n32016_dumpregs("Retuning to Pandora");
      ProfileInit();
   }
#endif
#endif
}
