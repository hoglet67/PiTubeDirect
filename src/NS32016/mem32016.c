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

#ifdef INCLUDE_DEBUGGER
#include "debug.h"
#include "../cpu_debug.h"
#endif

#ifdef BEM

#include "../tube.h"
static uint8_t ns32016ram[MEG16];

#else

#include "../tube-client.h"
#include "../tube-ula.h"
static uint8_t * ns32016ram;

#endif

#ifdef TEST_SUITE
#if TEST_SUITE == 0
#include "test/cpu_test.h"
#define ROM cpu_test
#else
#include "test/fpu_test.h"
#define ROM fpu_test
#endif
#else
// #include "pandora/PandoraV0_61.h"
// #define PANDORA_VERSION PandoraV0_61
// #include "pandora/PandoraV1_00.h"
// #define PANDORA_VERSION PandoraV1_00
#include "pandora/PandoraV2_00.h"
#define PANDORA_VERSION PandoraV2_00
#endif

void init_ram(void)
{
#ifndef BEM
   ns32016ram = copro_mem_reset(MEG16);
#endif
#ifdef TEST_SUITE
   memcpy(ns32016ram, ROM, sizeof(ROM));
#elif defined(PANDORA_BASE)
   memcpy(ns32016ram + PANDORA_BASE, PANDORA_VERSION, sizeof(PANDORA_VERSION));
#else
   uint32_t Address;

   for (Address = 0; Address < MEG16; Address += sizeof(PANDORA_VERSION))
   {
      memcpy(ns32016ram + Address, PANDORA_VERSION, sizeof(PANDORA_VERSION));
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
#ifdef INCLUDE_DEBUGGER
{
   uint8_t val = read_x8_internal(addr);   
   if (n32016_debug_enabled)
   {
      debug_memread(&n32016_cpu_debug, addr, val, 1);
   }
   return val;
}
uint8_t read_x8_internal(uint32_t addr)
#endif
{
   addr &= 0xFFFFFF;

   if (addr < IO_BASE)
   {
#ifdef USE_MEMORY_POINTER
      return ns32016ram[addr];
#else
      return *(unsigned char *)(addr);
#endif
   }

   if ((addr & 0xFFFFF1) == 0xFFFFF0)
   {
      return tube_parasite_read(addr >> 1);
   }

   PiWARN("Bad Read @ %06" PRIX32 "\n", addr);

   return 0;
}

uint16_t read_x16(uint32_t addr)
{
   addr &= 0xFFFFFF;

#ifdef NS_FAST_RAM
   if (addr < IO_BASE)
   {
      uint16_t val;
#ifdef USE_MEMORY_POINTER
      val = *((uint16_t*) (ns32016ram + addr));
#else
      val = *((uint16_t*) ( addr));
#endif
#ifdef INCLUDE_DEBUGGER
      if (n32016_debug_enabled)
      {
         debug_memread(&n32016_cpu_debug, addr, val, 2);
      }
#endif
      return val;
   }
#endif

   return read_x8(addr) | (read_x8(addr + 1) << 8);
}

uint32_t read_x32(uint32_t addr)
{
   addr &= 0xFFFFFF;

#ifdef NS_FAST_RAM
   if (addr < IO_BASE)
   {
      uint32_t val;
#ifdef USE_MEMORY_POINTER
      val = *((uint32_t*) (ns32016ram + addr));
#else
      val = *((uint32_t*) (addr));
#endif
#ifdef INCLUDE_DEBUGGER
      if (n32016_debug_enabled)
      {
         debug_memread(&n32016_cpu_debug, addr, val, 3);
      }
#endif
      return val;
   }
#endif

   return read_x8(addr) | (read_x8(addr + 1) << 8) | (read_x8(addr + 2) << 16) | (read_x8(addr + 3) << 24);
}

uint64_t read_x64(uint32_t addr)
{
   addr &= 0xFFFFFF;
   // ARM doesn't support unaligned 64-bit loads, so the following
   // results in a Data Abort exception:
   // return *((uint64_t*) (ns32016ram + addr))
   return (((uint64_t) read_x32(addr + 4)) << 32) + read_x32(addr);
}

// As this function returns uint32_t it *should* only be used for size 1, 2 or 4
uint32_t read_n(uint32_t addr, uint32_t Size)
{
   addr &= 0xFFFFFF;
   switch (Size)
   {
   case sz8:
      return read_x8(addr);
   case sz16:
      return read_x16(addr);
   case sz32:
      return read_x32(addr);
   default:
      PiWARN("Bad read_n() size @ %06" PRIX32 " size %" PRIX32 "\n", addr, Size);
      return 0;
   }
}

void write_x8(uint32_t addr, uint8_t val)
#ifdef INCLUDE_DEBUGGER
{
   if (n32016_debug_enabled)
   {
      debug_memwrite(&n32016_cpu_debug, addr, val, 1);
   }
   write_x8_internal(addr, val);   
}
void write_x8_internal(uint32_t addr, uint8_t val)
#endif
{
   addr &= 0xFFFFFF;

   if (addr <= (RAM_SIZE - sizeof(uint8_t)))
   {
#ifdef USE_MEMORY_POINTER
      ns32016ram[addr] = val;
#else
      *(unsigned char *)(addr) = val;
#endif
      return;
   }

   if ((addr & 0xFFFFF1) == 0xFFFFF0)
   {
      tube_parasite_write(addr >> 1, val);
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

   // Silently ignore writing one word beyond end of RAM
   // as Pandora RAM test does this
   if (addr >= RAM_SIZE + 4) {
      PiWARN("Writing outside of RAM @%06"PRIX32" %02"PRIX8"\n", addr, val);
   }
}

void write_x16(uint32_t addr, uint16_t val)
{
   addr &= 0xFFFFFF;

#ifdef NS_FAST_RAM
   if (addr <= (RAM_SIZE - sizeof(uint16_t)))
   {
#ifdef INCLUDE_DEBUGGER
      if (n32016_debug_enabled)
      {
         debug_memwrite(&n32016_cpu_debug, addr, val, 2);
      }
#endif
#ifdef USE_MEMORY_POINTER
      *((uint16_t*) (ns32016ram + addr)) = val;
#else
      *((uint16_t*) (addr)) = val;
#endif
      return;
   }
#endif

   write_x8(addr++, val & 0xFF);
   write_x8(addr, val >> 8);
}

void write_x32(uint32_t addr, uint32_t val)
{
   addr &= 0xFFFFFF;

#ifdef NS_FAST_RAM
   if (addr <= (RAM_SIZE - sizeof(uint32_t)))
   {
#ifdef INCLUDE_DEBUGGER
      if (n32016_debug_enabled)
      {
         debug_memwrite(&n32016_cpu_debug, addr, val, 4);
      }
#endif
#ifdef USE_MEMORY_POINTER
      *((uint32_t*) (ns32016ram + addr)) = val;
#else
      *((uint32_t*) (addr)) = val;
#endif
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
   addr &= 0xFFFFFF;

#ifdef NS_FAST_RAM
   if (addr <= (RAM_SIZE - sizeof(uint64_t)))
   {
      // ARM doesn't support unaligned 64-bit stores, so the following
      // results in a Data Abort exception:
      // *((uint64_t*) (ns32016ram + addr)) = val;
      write_x32(addr, (uint32_t) val);
      write_x32(addr + 4, (uint32_t) (val >> 32));
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
   addr &= 0xFFFFFF;

#ifdef NS_FAST_RAM
#ifdef INCLUDE_DEBUGGER
   if ((addr + Size) <= RAM_SIZE && !n32016_debug_enabled) 
#else
   if ((addr + Size) <= RAM_SIZE) 
#endif
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
