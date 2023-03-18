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

/* cpu.c: functions to emulate the 8086/V20 CPU in software. the heart of Fake86. */

// Soft 80186 Second Processor
// Copyright (C)2015-2016 Simon R. Ellwood BEng (FordP)

//#include "config.h"
#include <stdint.h>
#include <stdio.h>
#include "../copro-80186.h"
#include "iop80186.h"
#include "mem80186.h"

#ifdef INCLUDE_DEBUGGER
#include "../cpu_debug.h"
#include "cpu80186_debug.h"
#endif

#define TUBE_ACCESS(ADDRESS)	(((ADDRESS) & 0xFFF1) == 0x0080)
#define TUBE_CONVERT(PORT) 	(((PORT) >> 1) & 0x0007)



static uint32_t x86sa,x86ss,x86src;
static uint32_t x86da,x86ds,x86dst;
static uint32_t x86ena;
static uint16_t x86imask=0;


// I/O locations
// =============
// &80 Tube R1 Status
// &82 Tube R1 Data
// &84 Tube R2 Status
// &86 Tube R2 Data
// &88 Tube R3 Status
// &8A Tube R3 Data
// &8C Tube R4 Status
// &8E Tube R4 Data

void x86_dma()
{
    if (!(x86ena & 2)) {
        return;
    }        
    if (x86src < 0x100) {
        write86(x86dst, portin((uint16_t) x86src));
        x86dst++;
    } else {
        portout((uint16_t) x86dst, read86(x86src));
        x86src++;
    }
}

void portout(uint16_t portnum, uint8_t value)
{
#ifdef INCLUDE_DEBUGGER
   if (cpu80186_debug_enabled) {
      debug_iowrite(&cpu80186_cpu_debug, portnum, value, 1);
   }
#endif
    switch (portnum)
    {
        case 0xFF28: x86imask = value; return;
        case 0xFFC0: x86sa  = (x86sa  & 0xFF00) |  value;       x86src = x86sa + ((x86ss & 0xF) << 16); return;
        case 0xFFC1: x86sa  = (x86sa  & 0x00FF) | (value << 8); x86src = x86sa + ((x86ss & 0xF) << 16); return;
        case 0xFFC2: x86ss  = (x86ss  & 0xFF00) |  value;       x86src = x86sa + ((x86ss & 0xF) << 16); return;
        case 0xFFC3: x86ss  = (x86ss  & 0x00FF) | (value << 8); x86src = x86sa + ((x86ss & 0xF) << 16); return;
        case 0xFFC4: x86da  = (x86da  & 0xFF00) |  value;       x86dst = x86da + ((x86ds & 0xF) << 16); return;
        case 0xFFC5: x86da  = (x86da  & 0x00FF) | (value << 8); x86dst = x86da + ((x86ds & 0xF) << 16); return;
        case 0xFFC6: x86ds  = (x86ds  & 0xFF00) |  value;       x86dst = x86da + ((x86ds & 0xF) << 16); return;
        case 0xFFC7: x86ds  = (x86ds  & 0x00FF) | (value << 8); x86dst = x86da + ((x86ds & 0xF) << 16); return;
        case 0xFFCA: x86ena = (x86ena & 0xFF00) |  value;                                               return;
        case 0xFFCB: x86ena = (x86ena & 0x00FF) | (value << 8);                                         return;
    }
	if (TUBE_ACCESS(portnum)) {
		copro_80186_tube_write(TUBE_CONVERT(portnum), value);
	}
}

uint8_t portin(uint16_t portnum)
{
   uint8_t value;
	if (TUBE_ACCESS(portnum)) {
		value = (uint8_t) copro_80186_tube_read(TUBE_CONVERT(portnum));
	} else {
      value = 0xFF;
   }
#ifdef INCLUDE_DEBUGGER
   if (cpu80186_debug_enabled) {
      debug_ioread(&cpu80186_cpu_debug, portnum, value, 1);
   }
#endif
	return value;
}

uint16_t portin16(uint16_t portnum)
{
	return (uint16_t)( portin(portnum) | (uint16_t) (portin(portnum + 1) << 8));
}

void portout16(uint16_t portnum, uint16_t value)
{
	portout(portnum, (uint8_t) value);
	portout(portnum + 1, (uint8_t) (value >> 8));
}
