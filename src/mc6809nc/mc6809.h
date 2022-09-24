/*
 * Copyright 2001 by Arto Salmi and Joze Fabcic
 * Copyright 2006 by Brian Dominy <brian@oddchange.com>
 *
 * This file is part of GCC6809.
 *
 * GCC6809 is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * GCC6809 is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with GCC6809; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */


#ifndef M6809_H
#define M6809_H

#include <stdio.h>
//#include <stdlib.h>
//#include <string.h>
#include <stdint.h>

typedef uint16_t target_addr_t;

//#include "machine.h"

#define E_FLAG 0x80u
#define F_FLAG 0x40u
#define H_FLAG 0x20u
#define I_FLAG 0x10u
#define N_FLAG 0x08u
#define Z_FLAG 0x04u
#define V_FLAG 0x02u
#define C_FLAG 0x01u

extern int debug_enabled;
extern int need_flush;
extern unsigned long total;
extern int dump_cycles_on_success;
extern const char *prog_name;

long get_elapsed_realtime (void);

extern uint8_t copro_mc6809nc_read(uint16_t addr);
extern void copro_mc6809nc_write(uint16_t addr, uint8_t data);

/* Primitive read/write macros */
#define read8(addr)        copro_mc6809nc_read (addr)
#define write8(addr,val)   do {  copro_mc6809nc_write(addr, val); } while (0)

/* 16-bit versions */
#define read16(addr)       ((uint16_t) (copro_mc6809nc_read (addr + 1) + (copro_mc6809nc_read (addr) << 8)))
#define write16(addr,val)  do { write8(addr+1, val & 0xFF); write8(addr, (val >> 8) & 0xFF); } while (0)

/* Fetch macros */

#define abs_read16(addr)   ((abs_read8(addr) << 8) | abs_read8(addr+1))

#define fetch8()           abs_read8 (pc++)
#define fetch16()          (pc += 2, abs_read16(pc-2))

/* 6809.c */
extern int mc6809nc_execute (int tube_cycles);
extern void mc6809nc_reset (void);


extern void mc6809nc_request_irq (unsigned int source);
extern void mc6809nc_release_irq (unsigned int source);
extern void mc6809nc_request_firq (unsigned int source);
extern void mc6809nc_release_firq (unsigned int source);

extern uint8_t get_a  (void);
extern uint8_t get_b  (void);
extern uint8_t get_cc (void);
extern uint8_t get_dp (void);
extern uint16_t get_x  (void);
extern uint16_t get_y  (void);
extern uint16_t get_s  (void);
extern uint16_t get_u  (void);
extern uint16_t get_pc (void);
extern uint16_t get_d  (void);
extern uint8_t get_flags (void);
extern void set_a  (unsigned);
extern void set_b  (unsigned);
extern void set_cc (unsigned);
extern void set_dp (unsigned);
extern void set_x  (unsigned);
extern void set_y  (unsigned);
extern void set_s  (unsigned);
extern void set_u  (unsigned);
extern void set_pc (unsigned);
extern void set_d  (unsigned);

#endif /* M6809_H */
