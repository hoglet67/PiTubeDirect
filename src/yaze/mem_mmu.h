/* Header file for the Memory and the Memory Management Unit (MMU).
   Module MEM_MMU.[hc] Copyright (C) 1998/1999 by Andreas Gerlich (agl)

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
#ifndef MEM_MMU_H
#define MEM_MMU_H

#include <stdint.h>

typedef uint8_t	BYTE;

typedef uint16_t WORD;

/* FASTREG needs to be at least 16 bits wide and efficient for the
   host architecture */
typedef uint_fast16_t	FASTREG;

/* FASTWORK needs to be wider than 16 bits and efficient for the host
   architecture */
typedef uint_fast32_t	FASTWORK;

/*-------------------------------- definitions for memory space --------*/

#define Z80MEMSIZE 64		/* logical Addressspace of the Z80 */

#ifndef MEMSIZE			/* if MEMSIZE are not given */
  #define MEMSIZE Z80MEMSIZE	/* default without MMU: MEMSIZE = Z80MEMSIZE */
#endif

extern BYTE ram[MEMSIZE*1024];	/* RAM which is present */

/*---------------------------------- definitions for MMU tables --------*/

/* Some important macros. They are the interface between an access from
   the simz80-/yaze-Modules and the method of the memory access: */

extern unsigned char copro_z80_read_mem(unsigned int);
extern void copro_z80_write_mem(unsigned int, unsigned char);

#define GetBYTE(a)	     ((uint8_t)copro_z80_read_mem(a))
#define GetBYTE_pp(a)	 ((uint8_t)copro_z80_read_mem( (a++) ) )
#define GetBYTE_mm(a)	 ((uint8_t)copro_z80_read_mem( (a--) ) )
#define mm_GetBYTE(a)	 copro_z80_read_mem( (--(a)) )
#define PutBYTE(a, v)	 copro_z80_write_mem(a, (uint8_t)(v))
#define PutBYTE_pp(a,v)	 copro_z80_write_mem( (a++) , (uint8_t)(v))
#define PutBYTE_mm(a,v)	 copro_z80_write_mem( (a--) , (uint8_t)(v))
#define mm_PutBYTE(a,v)	 copro_z80_write_mem( (--(a)) , (uint8_t)(v))
#define GetWORD(a)	    ((uint16_t)(copro_z80_read_mem(a) | ( copro_z80_read_mem( (a) + 1) << 8) ))
#define PutWORD(a, v)    { copro_z80_write_mem( (a), (BYTE)(v & 0xFF) ); copro_z80_write_mem( ((a)+1),(BYTE) ((v)>>8 )); }

#endif