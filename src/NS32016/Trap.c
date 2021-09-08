#include <stdint.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "32016.h"
#include "mem32016.h"
#include "defs.h"
#include "Trap.h"
#include "Profile.h"

uint32_t TrapFlags;

static const char TrapText[TrapCount][40] =
{
   "Break Point Hit",
   "Break Point Trap",
   "Reserved Addressing Mode",
   "Unknown Format",
   "Unknown Instruction",
   "Divide By Zero",
   "Illegal Immediate",
   "Illegal DoubleIndexing",
   "Illegal SpecialReading",
   "Illegal SpecialWriting",
   "Illegal Writing Immediate",
   "Flag Instruction",
   "Privileged Instruction"
};

static void ShowTraps(void)
{
   if (TrapFlags)
   {
      uint32_t Count, Pattern = BIT(0);
      for (Count = 0; Count < TrapCount; Count++, Pattern <<= 1)
      {
         if (TrapFlags & Pattern)
         {
            TrapTRACE("%s", TrapText[Count]);
         }
      }
   }
}

static void Dump(void)
{
   n32016_ShowRegs(0xFF);
   ShowTraps();
   ProfileDump();
}

void n32016_dumpregs(const char* pMessage)
{
   TrapTRACE("%s", pMessage);
   Dump();

#ifdef PC_SIMULATION

#ifdef WIN32
   system("pause");
#endif
   CloseTrace();
   exit(1);
#endif
}

void HandleTrap(void)
{
   n32016_dumpregs("HandleTrap() called");
}
