/* Header file for the instruction set simulator.
   Copyright (C) 1995  Frank D. Cringle.
   Modifications for MMU and CP/M 3.1 Copyright (C) 2000/2003 by Andreas Gerlich


This file is part of yaze-ag - yet another Z80 emulator by ag.

Yaze-ag is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free
Software Foundation; either version 2 of the License, or (at your
option) any later version.

This program is distributed in the hope that it will be useful, but
WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA. */

/* SEE limits and BYTE-, WORD- and FASTREG - definitions im MEM_MMU.h */

/* two sets of accumulator / flags */
/*
extern WORD af[2];
extern int af_sel;
*/


//extern int regs_sel;
/*
extern WORD ir;
extern WORD ix;
extern WORD iy;
extern WORD sp;
extern WORD pc;
extern WORD IFF;
*/
/* see definitions for memory in mem_mmu.h */

// For FASTWORK
#include "mem_mmu.h"

#ifdef INCLUDE_DEBUGGER
#include "../cpu_debug.h"
extern int simz80_debug_enabled;
extern cpu_debug_t simz80_cpu_debug;
#endif

extern FASTWORK simz80(FASTREG PC);

#define FLAG_C	1
#define FLAG_N	2
#define FLAG_P	4
#define FLAG_H	16
#define FLAG_Z	64
#define FLAG_S	128

#define SETFLAG(f,c)	AF = (c) ? AF | FLAG_ ## f : (AF & (uint32_t)~FLAG_ ## f)
#define TSTFLAG(f)	((AF & FLAG_ ## f)?1:0)

#define ldig(x)		((x) & 0xf)
#define hdig(x)		(((x)>>4)&0xf)
#define lreg(x)		((x)&0xff)
#define hreg(x)		(((x)>>8)&0xff)

#define Setlreg(x, v)	x = (uint16_t)(((x)&0xff00u) | ((v)&0xffu))
#define Sethreg(x, v)	x = (uint16_t)(((x)&0xffu) | (((v)&0xffu) << 8))

/* SEE functions for manipulating of memory in mem_mmu.h
      line RAM, GetBYTE, GetWORD, PutBYTE, PutWORD, ....
*/

#ifndef BIOS
extern uint8_t copro_z80_read_io(unsigned int);
extern void copro_z80_write_io(unsigned int, unsigned char);
#define Input(port) copro_z80_read_io(port)
#define Output(port, value) copro_z80_write_io(port,value)
#else
/* Define these as macros or functions if you really want to simulate I/O */
#define Input(port)	0
#define Output(port, value)
#endif

// Some extra functions for handling RST, IRQ and NMI

extern void simz80_reset();

extern void simz80_NMI();

extern void simz80_IRQ();

extern int simz80_is_IRQ_enabled();

extern FASTWORK simz80_execute(int tube_cycles);
