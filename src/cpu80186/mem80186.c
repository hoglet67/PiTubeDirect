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

#ifdef DECLARE_RAM
uint8_t RAM[ONE_MEG];
#else
uint8_t* RAM = (uint8_t*) m186_RamBase;
#endif

void write86(uint32_t addr32, uint8_t value)
{
  addr32 &= 0xFFFFFF;
  if (addr32 < 0xF0000) {
    RAM[addr32] = value;
  }
}

void writew86(uint32_t addr32, uint16_t value)
{
  write86(addr32, (uint8_t) value);
  write86(addr32 + 1, (uint8_t)(value >> 8));
}

uint8_t read86(uint32_t addr32)
{
  return (RAM[addr32 & 0xFFFFF]);
}

uint16_t readw86(uint32_t addr32)
{
  return ((uint16_t) read86(addr32) | (uint16_t)(read86(addr32 + 1) << 8));
}

void Cleari80186Ram(void)
{
  memset(RAM, 0, ONE_MEG);
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
