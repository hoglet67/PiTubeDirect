// B-em v2.2 by Tom Walker
//32016 parasite processor emulation (not working yet)

// 32106 CoProcessor Memory Subsystem
// By Simon R. Ellwood

#include <stdint.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "32016.h"
#include "mem32016.h"

//#ifdef BEEBEM
#include "../tube-ula.h"
#define tubeRead tube_parasite_read
#define tubeWrite tube_parasite_write
//#else
//#include "../bare-metal/tube-lib.h"
//#endif

#ifdef TEST_SUITE
#if TEST_SUITE == 0
#include "test/cpu_test.h"
#define ROM cpu_test
#else
#include "test/fpu_test.h"
#define ROM fpu_test
#endif
#else
#include "pandora/PandoraV2_00.h"
#endif

uint8_t ns32016ram[MEG16];

void init_ram(void)
{
#ifdef TEST_SUITE
   memset(ns32016ram, 0, sizeof(ns32016ram));
   memcpy(ns32016ram, ROM, sizeof(ROM));
#elif defined(PANDORA_BASE)
   memcpy(ns32016ram + PANDORA_BASE, PandoraV2_00, sizeof(PandoraV2_00));
#else
   uint32_t Address;

   for (Address = 0; Address < MEG16; Address += sizeof(PandoraV2_00))
   {
      memcpy(ns32016ram + Address, PandoraV2_00, sizeof(PandoraV2_00));
   }
#endif
}

#if 0
void dump_ram(void)
{
   FILE *f = fopen("32016.dmp", "wb");
   if (f)
   {
      fwrite(ns32016ram, RAM_SIZE, 1, f);
      fclose(f);
   }
}
#endif

// Tube Access
// FFFFF0 - R1 status
// FFFFF2 - R1 data
// FFFFF4 - R2 status
// FFFFF6 - R2 data
// FFFFF8 - R3 status
// FFFFFA - R3 data
// FFFFFC - R4 status
// FFFFFE - R4 data

uint8_t read_x8(uint32_t addr)
{
   //addr &= MEM_MASK;

   if (addr < IO_BASE)
   {
      return ns32016ram[addr];
   }

   if ((addr & 0x01) == 0)
   {
      return tubeRead(addr >> 1);
   }

   //PiTRACE("Bad read_x8 @ %06"PRIX32"\n", addr);

   return 0;
}

uint16_t read_x16(uint32_t addr)
{
   //addr &= MEM_MASK;

#ifdef NS_FAST_RAM
   if (addr < IO_BASE)
   {
      return *((uint16_t*) (ns32016ram + addr));
   }
#endif

   return read_x8(addr) | (read_x8(addr + 1) << 8);
}

uint32_t read_x32(uint32_t addr)
{
   //addr &= MEM_MASK;

#ifdef NS_FAST_RAM
   if (addr < IO_BASE)
   {
      return *((uint32_t*) (ns32016ram + addr));
   }
#endif

   return read_x8(addr) | (read_x8(addr + 1) << 8) | (read_x8(addr + 2) << 16) | (read_x8(addr + 3) << 24);
}

uint64_t read_x64(uint32_t addr)
{
   // ARM doesn't support unalizged 64-bit loads, so the following
   // results in a Data Abort exception:
   // return *((uint64_t*) (ns32016ram + addr))
   return (((uint64_t) read_x32(addr + 4)) << 32) + read_x32(addr);
}

uint32_t read_n(uint32_t addr, uint32_t Size)
{
   if (Size <= sizeof(uint64_t))
   {
      if ((addr + Size) <= IO_BASE)
      {
         uint32_t Result = 0;
         memcpy(&Result, ns32016ram + addr, Size);
         return Result;
      }
   }

   PiTRACE("Bad Read @ %06" PRIu32 "\n", addr);
   return 0;
}

void write_x8(uint32_t addr, uint8_t val)
{ 
  //addr &= MEM_MASK;

#ifdef TRACE_WRITEs
   PiTRACE(" @%06"PRIX32" = %02"PRIX8"\n", addr, val);
#endif

   if (addr <= (RAM_SIZE - sizeof(uint8_t)))
   {
      ns32016ram[addr] = val;
      return;
   }

   if ((addr >= IO_BASE) && ((addr & 0x01) == 0))
   {
      tubeWrite(addr >> 1, val);
      return;
   }

   if (addr == 0xF90000)
   {
#ifdef PANDORA_ROM_PAGE_OUT
      PiTRACE("Pandora ROM no longer occupying the entire memory space!\n")
      memset(ns32016ram, 0, RAM_SIZE);
#else
      PiTRACE("Pandora ROM writes to 0xF90000\n");
#endif

      return;
   }

   PiTRACE("Writing outside of RAM @%06"PRIX32" %02"PRIX8"\n", addr, val);
}

void write_x16(uint32_t addr, uint16_t val)
{
#ifdef TRACE_WRITEs
   PiTRACE(" @%06"PRIX32" = %04"PRIX16"\n", addr, val);
#endif

#ifdef NS_FAST_RAM
   if (addr <= (RAM_SIZE - sizeof(uint16_t)))
   {
      *((uint16_t*) (ns32016ram + addr)) = val;
      return;
   }
#endif

   write_x8(addr++, val & 0xFF);
   write_x8(addr, val >> 8);
}

void write_x32(uint32_t addr, uint32_t val)
{
#ifdef TRACE_WRITEs
   PiTRACE(" @%06"PRIX32" = %08"PRIX32"\n", addr, val);
#endif

#ifdef NS_FAST_RAM
   if (addr <= (RAM_SIZE - sizeof(uint32_t)))
   {
      *((uint32_t*) (ns32016ram + addr)) = val;
      return;
   }
#endif

   write_x8(addr++, val);
   write_x8(addr++, (val >> 8));
   write_x8(addr++, (val >> 16));
   write_x8(addr, (val >> 24));
}

void write_x64(uint32_t addr, uint64_t val)
{
#ifdef TRACE_WRITEs
   PiTRACE(" @%06"PRIX32" = %016"PRIX64"\n", addr, val);
#endif

#ifdef NS_FAST_RAM
   if (addr <= (RAM_SIZE - sizeof(uint64_t)))
   {
      *((uint64_t*) (ns32016ram + addr)) = val;
      return;
   }
#endif

   write_x8(addr++, (uint8_t) val);
   write_x8(addr++, (uint8_t) (val >> 8));
   write_x8(addr++, (uint8_t) (val >> 16));
   write_x8(addr++, (uint8_t) (val >> 24));
   write_x8(addr++, (uint8_t) (val >> 32));
   write_x8(addr++, (uint8_t) (val >> 40));
   write_x8(addr++, (uint8_t) (val >> 48));
   write_x8(addr,   (uint8_t) (val >> 56));
}

void write_Arbitary(uint32_t addr, void* pData, uint32_t Size)
{
#ifdef TRACE_WRITEs
   uint32_t Index;
   register uint8_t* pV = (uint8_t*) pData;

   PiTRACE("?@%06"PRIX32" =", addr);

   for (Index = 0; Index < Size; Index++)
   {
      PiTRACE("%02"PRIX8, pV[Index]);
   }
   PiTRACE("\n");
#endif

   //addr &= MEM_MASK;

#ifdef NS_FAST_RAM
   if ((addr + Size) <= RAM_SIZE)
   {
      memcpy(ns32016ram + addr, pData, Size);
      return;
   }
#endif

   register uint8_t* pValue = (uint8_t*) pData;
   while (Size--)
   {
      write_x8(addr++, *pValue++);
   }
}

#if 0
uint32_t LoadBinary(const char *pFileName, uint32_t Location)
{
   FILE* pFile = fopen(pFileName, "rb");
   uint32_t End = 0;

   if (pFile)
   {
      long FileSize;

      fseek(pFile, 0, SEEK_END);
      FileSize = ftell(pFile);
      rewind(pFile);

      if ((Location + FileSize) < MEG16)
      { 
         End = fread(ns32016ram + Location, sizeof(uint8_t), FileSize, pFile) + Location;
      }

      fclose(pFile);
   }

   return End;
}
#endif
