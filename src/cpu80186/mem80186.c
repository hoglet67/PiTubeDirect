/*
  Fake86: A portable, open-source 8086 PC emulator.
  Copyright (C)2010-2012 Mike Chambers

  This program is free software; you can redistribute it and/or
  modify it under the terms of the GNU General Public License
  as published by the Free Software Foundation; either version 2
  of the License, or (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
*/

// Soft 80186 Second Processor
// Copyright (C)2015-2016 Simon R. Ellwood BEng (FordP)
//#include "config.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "cpu80186.h"
#include "mem80186.h"
#include "Client86_v1_01.h"
#include "../tube-client.h"
#include "../tube.h"

#ifdef INCLUDE_DEBUGGER
#include "../cpu_debug.h"
#include "cpu80186_debug.h"
#endif

// RAM Address limit
static uint32_t RAM_LIMIT;

#ifdef DECLARE_RAM
uint8_t* RAM;//[ONE_MEG];
#else
uint8_t* RAM = (uint8_t*) m186_RamBase;
#endif

// In a 512KB system, the memory is organized as follows:
// 00000-3FFFF - Lower RAM
// 40000-7FFFF - Upper RAM
// 80000-BFFFF - Upper RAM (alias)
// C0000-FFFFF - ROM
//
// We allow RAM_LIMIT to be any multiple of 64K, so have a few more cases.
//
//  1 -> 64KB    no upper bank
//  2 -> 128KB   no upper bank
//  3 -> 192KB   no upper bank
//  4 -> 256KB   no upper bank
//  5 -> 320KB   80000->BFFFF remapped to 40000->7FFFF
//  6 -> 384KB   80000->BFFFF remapped to 40000->7FFFF
//  7 -> 448KB   80000->BFFFF remapped to 40000->7FFFF
//  8 -> 512KB   80000->BFFFF remapped to 40000->7FFFF
//  9 -> 576KB   90000->BFFFF remapped to 50000->7FFFF
// 10 -> 640KB   A0000->BFFFF remapped to 60000->7FFFF
// 11 -> 704KB   B0000->BFFFF remapped to 70000->7FFFF
// 12 -> 768KB   no remapping
// 13 -> 832KB   no remapping
// 14 -> 896KB   no remapping
// 15 -> 960KB   no remapping

static inline uint32_t map_address(uint32_t addr) {
   // Implement the Upper RAM alias
   if (addr > 0x80000 && addr <= 0xBFFFF && addr >= RAM_LIMIT) {
      addr ^= 0xC0000;
   }
   return addr;
}

void write86(uint32_t addr32, uint8_t value)
{
   addr32 &= 0xFFFFFF;
#ifdef INCLUDE_DEBUGGER
   if (cpu80186_debug_enabled) {
      debug_memwrite(&cpu80186_cpu_debug, addr32, value, 1);
   }
#endif
   uint32_t addr = map_address(addr32);
   if (addr < RAM_LIMIT) {
#if USE_MEMORY_POINTER
      RAM[addr] = value;
#else
      *(unsigned char *)(addr) = value;
#endif
   }
}

void writew86(uint32_t addr32, uint16_t value)
{
   write86(addr32, (uint8_t) value);
   write86(addr32 + 1, (uint8_t)(value >> 8));
}

uint8_t read86(uint32_t addr32)
{
   addr32 &= 0xFFFFFF;
   uint32_t addr = map_address(addr32);
#if USE_MEMORY_POINTER
   uint8_t value = (RAM[addr]);
#else
   uint8_t value = *(unsigned char *)(addr) ;
#endif
#ifdef INCLUDE_DEBUGGER
   if (cpu80186_debug_enabled) {
      debug_memread(&cpu80186_cpu_debug, addr32, value, 1);
   }
#endif
   return value;
}

uint16_t readw86(uint32_t addr32)
{
   return ((uint16_t) read86(addr32) | (uint16_t)(read86(addr32 + 1) << 8));
}

void Cleari80186Ram(void)
{
   // Always allocate 1MB of space
   RAM = copro_mem_reset(ONE_MEG);
   if (copro_memory_size > 0) {
      RAM_LIMIT = copro_memory_size  & ~((64*1024)-1);
      if (RAM_LIMIT < 0x10000) {
         // Minimum is 64KB
         RAM_LIMIT = 0x10000;
      }
      if (RAM_LIMIT > 0xF0000) {
         // Maximum is 960KB
         RAM_LIMIT = 0xF0000;
      }
   } else {
      // Default is 896KB
      RAM_LIMIT = 0xE0000;
   }
}

// The ARM must call RomCopy before starting the Processor
void RomCopy(void)
{
   uint32_t Base;

   for (Base = MirrorBase; Base < ONE_MEG; Base += sizeof(Client86_v1_01))
   {
      memcpy((void*)&RAM[Base], (void*)Client86_v1_01, sizeof(Client86_v1_01));
   }
}
