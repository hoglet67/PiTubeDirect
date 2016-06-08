/* Z80 instruction set simulator.
   Copyright (C) 1995  Frank D. Cringle.

This file is part of yaze - yet another Z80 emulator.

Yaze is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free
Software Foundation; either version 2 of the License, or (at your
option) any later version.

This program is distributed in the hope that it will be useful, but
WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA. */

/* This file was generated from simz80.pl
   with the following choice of options */
char *perl_params =
    "combine=0,"
    "optab=0,"
    "cb_inline=1,"
    "dfd_inline=1,"
    "ed_inline=1";

#include "mem_mmu.h"
#include "simz80.h"

static const unsigned char partab[256] = {
	4,0,0,4,0,4,4,0,0,4,4,0,4,0,0,4,
	0,4,4,0,4,0,0,4,4,0,0,4,0,4,4,0,
	0,4,4,0,4,0,0,4,4,0,0,4,0,4,4,0,
	4,0,0,4,0,4,4,0,0,4,4,0,4,0,0,4,
	0,4,4,0,4,0,0,4,4,0,0,4,0,4,4,0,
	4,0,0,4,0,4,4,0,0,4,4,0,4,0,0,4,
	4,0,0,4,0,4,4,0,0,4,4,0,4,0,0,4,
	0,4,4,0,4,0,0,4,4,0,0,4,0,4,4,0,
	0,4,4,0,4,0,0,4,4,0,0,4,0,4,4,0,
	4,0,0,4,0,4,4,0,0,4,4,0,4,0,0,4,
	4,0,0,4,0,4,4,0,0,4,4,0,4,0,0,4,
	0,4,4,0,4,0,0,4,4,0,0,4,0,4,4,0,
	4,0,0,4,0,4,4,0,0,4,4,0,4,0,0,4,
	0,4,4,0,4,0,0,4,4,0,0,4,0,4,4,0,
	0,4,4,0,4,0,0,4,4,0,0,4,0,4,4,0,
	4,0,0,4,0,4,4,0,0,4,4,0,4,0,0,4,
};

#define parity(x)	partab[(x)&0xff]

#ifdef DEBUG
volatile int stopsim;
#endif

#define POP(x)	do {							\
	FASTREG y = RAM_pp(SP);						\
	x = y + (RAM_pp(SP) << 8);					\
} while (0)

#define PUSH(x) do {							\
	mm_RAM(SP) = (x) >> 8;						\
	mm_RAM(SP) = x;							\
} while (0)

#define JPC(cond) PC = cond ? GetWORD(PC) : PC+2

#define CALLC(cond) {							\
    if (cond) {								\
	FASTREG adrr = GetWORD(PC);					\
	PUSH(PC+2);							\
	PC = adrr;							\
    }									\
    else								\
	PC += 2;							\
}

/* load Z80 registers into (we hope) host registers */
#define LOAD_STATE()							\
    PC = pc;								\
    AF = af[af_sel];							\
    BC = regs[regs_sel].bc;						\
    DE = regs[regs_sel].de;						\
    HL = regs[regs_sel].hl;						\
    IX = ix;								\
    IY = iy;								\
    SP = sp

/* load Z80 registers into (we hope) host registers */
#define DECLARE_STATE()							\
    FASTREG PC = pc;							\
    FASTREG AF = af[af_sel];						\
    FASTREG BC = regs[regs_sel].bc;					\
    FASTREG DE = regs[regs_sel].de;					\
    FASTREG HL = regs[regs_sel].hl;					\
    FASTREG IX = ix;							\
    FASTREG IY = iy;							\
    FASTREG SP = sp

/* save Z80 registers back into memory */
#define SAVE_STATE()							\
    pc = PC;								\
    af[af_sel] = AF;							\
    regs[regs_sel].bc = BC;						\
    regs[regs_sel].de = DE;						\
    regs[regs_sel].hl = HL;						\
    ix = IX;								\
    iy = IY;								\
    sp = SP

FASTWORK
simz80(FASTREG PC)
{
    FASTREG AF = af[af_sel];
    FASTREG BC = regs[regs_sel].bc;
    FASTREG DE = regs[regs_sel].de;
    FASTREG HL = regs[regs_sel].hl;
    FASTREG SP = sp;
    FASTREG IX = ix;
    FASTREG IY = iy;
    FASTWORK temp, acu, sum, cbits;
    FASTWORK op, adr;
#ifdef MMU
    FASTREG tmp2;
#endif

#ifdef DEBUG
    while (!stopsim) {
#else
    while (1) {
#endif
    switch(RAM_pp(PC)) {
	case 0x00:			/* NOP */
		break;
	case 0x01:			/* LD BC,nnnn */
		BC = GetWORD(PC);
		PC += 2;
		break;
	case 0x02:			/* LD (BC),A */
		PutBYTE(BC, hreg(AF));
		break;
	case 0x03:			/* INC BC */
		++BC;
		break;
	case 0x04:			/* INC B */
		BC += 0x100;
		temp = hreg(BC);
		AF = (AF & ~0xfe) | (temp & 0xa8) |
			(((temp & 0xff) == 0) << 6) |
			(((temp & 0xf) == 0) << 4) |
			((temp == 0x80) << 2);
		break;
	case 0x05:			/* DEC B */
		BC -= 0x100;
		temp = hreg(BC);
		AF = (AF & ~0xfe) | (temp & 0xa8) |
			(((temp & 0xff) == 0) << 6) |
			(((temp & 0xf) == 0xf) << 4) |
			((temp == 0x7f) << 2) | 2;
		break;
	case 0x06:			/* LD B,nn */
		Sethreg(BC, GetBYTE_pp(PC));
		break;
	case 0x07:			/* RLCA */
		AF = ((AF >> 7) & 0x0128) | ((AF << 1) & ~0x1ff) |
			(AF & 0xc4) | ((AF >> 15) & 1);
		break;
	case 0x08:			/* EX AF,AF' */
		af[af_sel] = AF;
		af_sel = 1 - af_sel;
		AF = af[af_sel];
		break;
	case 0x09:			/* ADD HL,BC */
		HL &= 0xffff;
		BC &= 0xffff;
		sum = HL + BC;
		cbits = (HL ^ BC ^ sum) >> 8;
		HL = sum;
		AF = (AF & ~0x3b) | ((sum >> 8) & 0x28) |
			(cbits & 0x10) | ((cbits >> 8) & 1);
		break;
	case 0x0A:			/* LD A,(BC) */
		Sethreg(AF, GetBYTE(BC));
		break;
	case 0x0B:			/* DEC BC */
		--BC;
		break;
	case 0x0C:			/* INC C */
		temp = lreg(BC)+1;
		Setlreg(BC, temp);
		AF = (AF & ~0xfe) | (temp & 0xa8) |
			(((temp & 0xff) == 0) << 6) |
			(((temp & 0xf) == 0) << 4) |
			((temp == 0x80) << 2);
		break;
	case 0x0D:			/* DEC C */
		temp = lreg(BC)-1;
		Setlreg(BC, temp);
		AF = (AF & ~0xfe) | (temp & 0xa8) |
			(((temp & 0xff) == 0) << 6) |
			(((temp & 0xf) == 0xf) << 4) |
			((temp == 0x7f) << 2) | 2;
		break;
	case 0x0E:			/* LD C,nn */
		Setlreg(BC, GetBYTE_pp(PC));
		break;
	case 0x0F:			/* RRCA */
		temp = hreg(AF);
		sum = temp >> 1;
		AF = ((temp & 1) << 15) | (sum << 8) |
			(sum & 0x28) | (AF & 0xc4) | (temp & 1);
		break;
	case 0x10:			/* DJNZ dd */
		PC += ((BC -= 0x100) & 0xff00) ? (signed char) GetBYTE(PC) + 1 : 1;
		break;
	case 0x11:			/* LD DE,nnnn */
		DE = GetWORD(PC);
		PC += 2;
		break;
	case 0x12:			/* LD (DE),A */
		PutBYTE(DE, hreg(AF));
		break;
	case 0x13:			/* INC DE */
		++DE;
		break;
	case 0x14:			/* INC D */
		DE += 0x100;
		temp = hreg(DE);
		AF = (AF & ~0xfe) | (temp & 0xa8) |
			(((temp & 0xff) == 0) << 6) |
			(((temp & 0xf) == 0) << 4) |
			((temp == 0x80) << 2);
		break;
	case 0x15:			/* DEC D */
		DE -= 0x100;
		temp = hreg(DE);
		AF = (AF & ~0xfe) | (temp & 0xa8) |
			(((temp & 0xff) == 0) << 6) |
			(((temp & 0xf) == 0xf) << 4) |
			((temp == 0x7f) << 2) | 2;
		break;
	case 0x16:			/* LD D,nn */
		Sethreg(DE, GetBYTE_pp(PC));
		break;
	case 0x17:			/* RLA */
		AF = ((AF << 8) & 0x0100) | ((AF >> 7) & 0x28) | ((AF << 1) & ~0x01ff) |
			(AF & 0xc4) | ((AF >> 15) & 1);
		break;
	case 0x18:			/* JR dd */
		PC += (1) ? (signed char) GetBYTE(PC) + 1 : 1;
		break;
	case 0x19:			/* ADD HL,DE */
		HL &= 0xffff;
		DE &= 0xffff;
		sum = HL + DE;
		cbits = (HL ^ DE ^ sum) >> 8;
		HL = sum;
		AF = (AF & ~0x3b) | ((sum >> 8) & 0x28) |
			(cbits & 0x10) | ((cbits >> 8) & 1);
		break;
	case 0x1A:			/* LD A,(DE) */
		Sethreg(AF, GetBYTE(DE));
		break;
	case 0x1B:			/* DEC DE */
		--DE;
		break;
	case 0x1C:			/* INC E */
		temp = lreg(DE)+1;
		Setlreg(DE, temp);
		AF = (AF & ~0xfe) | (temp & 0xa8) |
			(((temp & 0xff) == 0) << 6) |
			(((temp & 0xf) == 0) << 4) |
			((temp == 0x80) << 2);
		break;
	case 0x1D:			/* DEC E */
		temp = lreg(DE)-1;
		Setlreg(DE, temp);
		AF = (AF & ~0xfe) | (temp & 0xa8) |
			(((temp & 0xff) == 0) << 6) |
			(((temp & 0xf) == 0xf) << 4) |
			((temp == 0x7f) << 2) | 2;
		break;
	case 0x1E:			/* LD E,nn */
		Setlreg(DE, GetBYTE_pp(PC));
		break;
	case 0x1F:			/* RRA */
		temp = hreg(AF);
		sum = temp >> 1;
		AF = ((AF & 1) << 15) | (sum << 8) |
			(sum & 0x28) | (AF & 0xc4) | (temp & 1);
		break;
	case 0x20:			/* JR NZ,dd */
		PC += (!TSTFLAG(Z)) ? (signed char) GetBYTE(PC) + 1 : 1;
		break;
	case 0x21:			/* LD HL,nnnn */
		HL = GetWORD(PC);
		PC += 2;
		break;
	case 0x22:			/* LD (nnnn),HL */
		temp = GetWORD(PC);
		PutWORD(temp, HL);
		PC += 2;
		break;
	case 0x23:			/* INC HL */
		++HL;
		break;
	case 0x24:			/* INC H */
		HL += 0x100;
		temp = hreg(HL);
		AF = (AF & ~0xfe) | (temp & 0xa8) |
			(((temp & 0xff) == 0) << 6) |
			(((temp & 0xf) == 0) << 4) |
			((temp == 0x80) << 2);
		break;
	case 0x25:			/* DEC H */
		HL -= 0x100;
		temp = hreg(HL);
		AF = (AF & ~0xfe) | (temp & 0xa8) |
			(((temp & 0xff) == 0) << 6) |
			(((temp & 0xf) == 0xf) << 4) |
			((temp == 0x7f) << 2) | 2;
		break;
	case 0x26:			/* LD H,nn */
		Sethreg(HL, GetBYTE_pp(PC));
		break;
	case 0x27:			/* DAA */
		acu = hreg(AF);
		temp = ldig(acu);
		cbits = TSTFLAG(C);
		if (TSTFLAG(N)) {	/* last operation was a subtract */
			int hd = cbits || acu > 0x99;
			if (TSTFLAG(H) || (temp > 9)) { /* adjust low digit */
				if (temp > 5)
					SETFLAG(H, 0);
				acu -= 6;
				acu &= 0xff;
			}
			if (hd)		/* adjust high digit */
				acu -= 0x160;
		}
		else {			/* last operation was an add */
			if (TSTFLAG(H) || (temp > 9)) { /* adjust low digit */
				SETFLAG(H, (temp > 9));
				acu += 6;
			}
			if (cbits || ((acu & 0x1f0) > 0x90)) /* adjust high digit */
				acu += 0x60;
		}
		cbits |= (acu >> 8) & 1;
		acu &= 0xff;
		AF = (acu << 8) | (acu & 0xa8) | ((acu == 0) << 6) |
			(AF & 0x12) | partab[acu] | cbits;
		break;
	case 0x28:			/* JR Z,dd */
		PC += (TSTFLAG(Z)) ? (signed char) GetBYTE(PC) + 1 : 1;
		break;
	case 0x29:			/* ADD HL,HL */
		HL &= 0xffff;
		sum = HL + HL;
		cbits = (HL ^ HL ^ sum) >> 8;
		HL = sum;
		AF = (AF & ~0x3b) | ((sum >> 8) & 0x28) |
			(cbits & 0x10) | ((cbits >> 8) & 1);
		break;
	case 0x2A:			/* LD HL,(nnnn) */
		temp = GetWORD(PC);
		HL = GetWORD(temp);
		PC += 2;
		break;
	case 0x2B:			/* DEC HL */
		--HL;
		break;
	case 0x2C:			/* INC L */
		temp = lreg(HL)+1;
		Setlreg(HL, temp);
		AF = (AF & ~0xfe) | (temp & 0xa8) |
			(((temp & 0xff) == 0) << 6) |
			(((temp & 0xf) == 0) << 4) |
			((temp == 0x80) << 2);
		break;
	case 0x2D:			/* DEC L */
		temp = lreg(HL)-1;
		Setlreg(HL, temp);
		AF = (AF & ~0xfe) | (temp & 0xa8) |
			(((temp & 0xff) == 0) << 6) |
			(((temp & 0xf) == 0xf) << 4) |
			((temp == 0x7f) << 2) | 2;
		break;
	case 0x2E:			/* LD L,nn */
		Setlreg(HL, GetBYTE_pp(PC));
		break;
	case 0x2F:			/* CPL */
		AF = (~AF & ~0xff) | (AF & 0xc5) | ((~AF >> 8) & 0x28) | 0x12;
		break;
	case 0x30:			/* JR NC,dd */
		PC += (!TSTFLAG(C)) ? (signed char) GetBYTE(PC) + 1 : 1;
		break;
	case 0x31:			/* LD SP,nnnn */
		SP = GetWORD(PC);
		PC += 2;
		break;
	case 0x32:			/* LD (nnnn),A */
		temp = GetWORD(PC);
		PutBYTE(temp, hreg(AF));
		PC += 2;
		break;
	case 0x33:			/* INC SP */
		++SP;
		break;
	case 0x34:			/* INC (HL) */
		temp = GetBYTE(HL)+1;
		PutBYTE(HL, temp);
		AF = (AF & ~0xfe) | (temp & 0xa8) |
			(((temp & 0xff) == 0) << 6) |
			(((temp & 0xf) == 0) << 4) |
			((temp == 0x80) << 2);
		break;
	case 0x35:			/* DEC (HL) */
		temp = GetBYTE(HL)-1;
		PutBYTE(HL, temp);
		AF = (AF & ~0xfe) | (temp & 0xa8) |
			(((temp & 0xff) == 0) << 6) |
			(((temp & 0xf) == 0xf) << 4) |
			((temp == 0x7f) << 2) | 2;
		break;
	case 0x36:			/* LD (HL),nn */
		PutBYTE(HL, GetBYTE_pp(PC));
		break;
	case 0x37:			/* SCF */
		AF = (AF&~0x3b)|((AF>>8)&0x28)|1;
		break;
	case 0x38:			/* JR C,dd */
		PC += (TSTFLAG(C)) ? (signed char) GetBYTE(PC) + 1 : 1;
		break;
	case 0x39:			/* ADD HL,SP */
		HL &= 0xffff;
		SP &= 0xffff;
		sum = HL + SP;
		cbits = (HL ^ SP ^ sum) >> 8;
		HL = sum;
		AF = (AF & ~0x3b) | ((sum >> 8) & 0x28) |
			(cbits & 0x10) | ((cbits >> 8) & 1);
		break;
	case 0x3A:			/* LD A,(nnnn) */
		temp = GetWORD(PC);
		Sethreg(AF, GetBYTE(temp));
		PC += 2;
		break;
	case 0x3B:			/* DEC SP */
		--SP;
		break;
	case 0x3C:			/* INC A */
		AF += 0x100;
		temp = hreg(AF);
		AF = (AF & ~0xfe) | (temp & 0xa8) |
			(((temp & 0xff) == 0) << 6) |
			(((temp & 0xf) == 0) << 4) |
			((temp == 0x80) << 2);
		break;
	case 0x3D:			/* DEC A */
		AF -= 0x100;
		temp = hreg(AF);
		AF = (AF & ~0xfe) | (temp & 0xa8) |
			(((temp & 0xff) == 0) << 6) |
			(((temp & 0xf) == 0xf) << 4) |
			((temp == 0x7f) << 2) | 2;
		break;
	case 0x3E:			/* LD A,nn */
		Sethreg(AF, GetBYTE_pp(PC));
		break;
	case 0x3F:			/* CCF */
		AF = (AF&~0x3b)|((AF>>8)&0x28)|((AF&1)<<4)|(~AF&1);
		break;
	case 0x40:			/* LD B,B */
		/* nop */
		break;
	case 0x41:			/* LD B,C */
		BC = (BC & 255) | ((BC & 255) << 8);
		break;
	case 0x42:			/* LD B,D */
		BC = (BC & 255) | (DE & ~255);
		break;
	case 0x43:			/* LD B,E */
		BC = (BC & 255) | ((DE & 255) << 8);
		break;
	case 0x44:			/* LD B,H */
		BC = (BC & 255) | (HL & ~255);
		break;
	case 0x45:			/* LD B,L */
		BC = (BC & 255) | ((HL & 255) << 8);
		break;
	case 0x46:			/* LD B,(HL) */
		Sethreg(BC, GetBYTE(HL));
		break;
	case 0x47:			/* LD B,A */
		BC = (BC & 255) | (AF & ~255);
		break;
	case 0x48:			/* LD C,B */
		BC = (BC & ~255) | ((BC >> 8) & 255);
		break;
	case 0x49:			/* LD C,C */
		/* nop */
		break;
	case 0x4A:			/* LD C,D */
		BC = (BC & ~255) | ((DE >> 8) & 255);
		break;
	case 0x4B:			/* LD C,E */
		BC = (BC & ~255) | (DE & 255);
		break;
	case 0x4C:			/* LD C,H */
		BC = (BC & ~255) | ((HL >> 8) & 255);
		break;
	case 0x4D:			/* LD C,L */
		BC = (BC & ~255) | (HL & 255);
		break;
	case 0x4E:			/* LD C,(HL) */
		Setlreg(BC, GetBYTE(HL));
		break;
	case 0x4F:			/* LD C,A */
		BC = (BC & ~255) | ((AF >> 8) & 255);
		break;
	case 0x50:			/* LD D,B */
		DE = (DE & 255) | (BC & ~255);
		break;
	case 0x51:			/* LD D,C */
		DE = (DE & 255) | ((BC & 255) << 8);
		break;
	case 0x52:			/* LD D,D */
		/* nop */
		break;
	case 0x53:			/* LD D,E */
		DE = (DE & 255) | ((DE & 255) << 8);
		break;
	case 0x54:			/* LD D,H */
		DE = (DE & 255) | (HL & ~255);
		break;
	case 0x55:			/* LD D,L */
		DE = (DE & 255) | ((HL & 255) << 8);
		break;
	case 0x56:			/* LD D,(HL) */
		Sethreg(DE, GetBYTE(HL));
		break;
	case 0x57:			/* LD D,A */
		DE = (DE & 255) | (AF & ~255);
		break;
	case 0x58:			/* LD E,B */
		DE = (DE & ~255) | ((BC >> 8) & 255);
		break;
	case 0x59:			/* LD E,C */
		DE = (DE & ~255) | (BC & 255);
		break;
	case 0x5A:			/* LD E,D */
		DE = (DE & ~255) | ((DE >> 8) & 255);
		break;
	case 0x5B:			/* LD E,E */
		/* nop */
		break;
	case 0x5C:			/* LD E,H */
		DE = (DE & ~255) | ((HL >> 8) & 255);
		break;
	case 0x5D:			/* LD E,L */
		DE = (DE & ~255) | (HL & 255);
		break;
	case 0x5E:			/* LD E,(HL) */
		Setlreg(DE, GetBYTE(HL));
		break;
	case 0x5F:			/* LD E,A */
		DE = (DE & ~255) | ((AF >> 8) & 255);
		break;
	case 0x60:			/* LD H,B */
		HL = (HL & 255) | (BC & ~255);
		break;
	case 0x61:			/* LD H,C */
		HL = (HL & 255) | ((BC & 255) << 8);
		break;
	case 0x62:			/* LD H,D */
		HL = (HL & 255) | (DE & ~255);
		break;
	case 0x63:			/* LD H,E */
		HL = (HL & 255) | ((DE & 255) << 8);
		break;
	case 0x64:			/* LD H,H */
		/* nop */
		break;
	case 0x65:			/* LD H,L */
		HL = (HL & 255) | ((HL & 255) << 8);
		break;
	case 0x66:			/* LD H,(HL) */
		Sethreg(HL, GetBYTE(HL));
		break;
	case 0x67:			/* LD H,A */
		HL = (HL & 255) | (AF & ~255);
		break;
	case 0x68:			/* LD L,B */
		HL = (HL & ~255) | ((BC >> 8) & 255);
		break;
	case 0x69:			/* LD L,C */
		HL = (HL & ~255) | (BC & 255);
		break;
	case 0x6A:			/* LD L,D */
		HL = (HL & ~255) | ((DE >> 8) & 255);
		break;
	case 0x6B:			/* LD L,E */
		HL = (HL & ~255) | (DE & 255);
		break;
	case 0x6C:			/* LD L,H */
		HL = (HL & ~255) | ((HL >> 8) & 255);
		break;
	case 0x6D:			/* LD L,L */
		/* nop */
		break;
	case 0x6E:			/* LD L,(HL) */
		Setlreg(HL, GetBYTE(HL));
		break;
	case 0x6F:			/* LD L,A */
		HL = (HL & ~255) | ((AF >> 8) & 255);
		break;
	case 0x70:			/* LD (HL),B */
		PutBYTE(HL, hreg(BC));
		break;
	case 0x71:			/* LD (HL),C */
		PutBYTE(HL, lreg(BC));
		break;
	case 0x72:			/* LD (HL),D */
		PutBYTE(HL, hreg(DE));
		break;
	case 0x73:			/* LD (HL),E */
		PutBYTE(HL, lreg(DE));
		break;
	case 0x74:			/* LD (HL),H */
		PutBYTE(HL, hreg(HL));
		break;
	case 0x75:			/* LD (HL),L */
		PutBYTE(HL, lreg(HL));
		break;
	case 0x76:			/* HALT */
		SAVE_STATE();
		return PC&0xffff;
	case 0x77:			/* LD (HL),A */
		PutBYTE(HL, hreg(AF));
		break;
	case 0x78:			/* LD A,B */
		AF = (AF & 255) | (BC & ~255);
		break;
	case 0x79:			/* LD A,C */
		AF = (AF & 255) | ((BC & 255) << 8);
		break;
	case 0x7A:			/* LD A,D */
		AF = (AF & 255) | (DE & ~255);
		break;
	case 0x7B:			/* LD A,E */
		AF = (AF & 255) | ((DE & 255) << 8);
		break;
	case 0x7C:			/* LD A,H */
		AF = (AF & 255) | (HL & ~255);
		break;
	case 0x7D:			/* LD A,L */
		AF = (AF & 255) | ((HL & 255) << 8);
		break;
	case 0x7E:			/* LD A,(HL) */
		Sethreg(AF, GetBYTE(HL));
		break;
	case 0x7F:			/* LD A,A */
		/* nop */
		break;
	case 0x80:			/* ADD A,B */
		temp = hreg(BC);
		acu = hreg(AF);
		sum = acu + temp;
		cbits = acu ^ temp ^ sum;
		AF = ((sum & 0xff) << 8) | (sum & 0xa8) |
			(((sum & 0xff) == 0) << 6) | (cbits & 0x10) |
			(((cbits >> 6) ^ (cbits >> 5)) & 4) |
			((cbits >> 8) & 1);
		break;
	case 0x81:			/* ADD A,C */
		temp = lreg(BC);
		acu = hreg(AF);
		sum = acu + temp;
		cbits = acu ^ temp ^ sum;
		AF = ((sum & 0xff) << 8) | (sum & 0xa8) |
			(((sum & 0xff) == 0) << 6) | (cbits & 0x10) |
			(((cbits >> 6) ^ (cbits >> 5)) & 4) |
			((cbits >> 8) & 1);
		break;
	case 0x82:			/* ADD A,D */
		temp = hreg(DE);
		acu = hreg(AF);
		sum = acu + temp;
		cbits = acu ^ temp ^ sum;
		AF = ((sum & 0xff) << 8) | (sum & 0xa8) |
			(((sum & 0xff) == 0) << 6) | (cbits & 0x10) |
			(((cbits >> 6) ^ (cbits >> 5)) & 4) |
			((cbits >> 8) & 1);
		break;
	case 0x83:			/* ADD A,E */
		temp = lreg(DE);
		acu = hreg(AF);
		sum = acu + temp;
		cbits = acu ^ temp ^ sum;
		AF = ((sum & 0xff) << 8) | (sum & 0xa8) |
			(((sum & 0xff) == 0) << 6) | (cbits & 0x10) |
			(((cbits >> 6) ^ (cbits >> 5)) & 4) |
			((cbits >> 8) & 1);
		break;
	case 0x84:			/* ADD A,H */
		temp = hreg(HL);
		acu = hreg(AF);
		sum = acu + temp;
		cbits = acu ^ temp ^ sum;
		AF = ((sum & 0xff) << 8) | (sum & 0xa8) |
			(((sum & 0xff) == 0) << 6) | (cbits & 0x10) |
			(((cbits >> 6) ^ (cbits >> 5)) & 4) |
			((cbits >> 8) & 1);
		break;
	case 0x85:			/* ADD A,L */
		temp = lreg(HL);
		acu = hreg(AF);
		sum = acu + temp;
		cbits = acu ^ temp ^ sum;
		AF = ((sum & 0xff) << 8) | (sum & 0xa8) |
			(((sum & 0xff) == 0) << 6) | (cbits & 0x10) |
			(((cbits >> 6) ^ (cbits >> 5)) & 4) |
			((cbits >> 8) & 1);
		break;
	case 0x86:			/* ADD A,(HL) */
		temp = GetBYTE(HL);
		acu = hreg(AF);
		sum = acu + temp;
		cbits = acu ^ temp ^ sum;
		AF = ((sum & 0xff) << 8) | (sum & 0xa8) |
			(((sum & 0xff) == 0) << 6) | (cbits & 0x10) |
			(((cbits >> 6) ^ (cbits >> 5)) & 4) |
			((cbits >> 8) & 1);
		break;
	case 0x87:			/* ADD A,A */
		temp = hreg(AF);
		acu = hreg(AF);
		sum = acu + temp;
		cbits = acu ^ temp ^ sum;
		AF = ((sum & 0xff) << 8) | (sum & 0xa8) |
			(((sum & 0xff) == 0) << 6) | (cbits & 0x10) |
			(((cbits >> 6) ^ (cbits >> 5)) & 4) |
			((cbits >> 8) & 1);
		break;
	case 0x88:			/* ADC A,B */
		temp = hreg(BC);
		acu = hreg(AF);
		sum = acu + temp + TSTFLAG(C);
		cbits = acu ^ temp ^ sum;
		AF = ((sum & 0xff) << 8) | (sum & 0xa8) |
			(((sum & 0xff) == 0) << 6) | (cbits & 0x10) |
			(((cbits >> 6) ^ (cbits >> 5)) & 4) |
			((cbits >> 8) & 1);
		break;
	case 0x89:			/* ADC A,C */
		temp = lreg(BC);
		acu = hreg(AF);
		sum = acu + temp + TSTFLAG(C);
		cbits = acu ^ temp ^ sum;
		AF = ((sum & 0xff) << 8) | (sum & 0xa8) |
			(((sum & 0xff) == 0) << 6) | (cbits & 0x10) |
			(((cbits >> 6) ^ (cbits >> 5)) & 4) |
			((cbits >> 8) & 1);
		break;
	case 0x8A:			/* ADC A,D */
		temp = hreg(DE);
		acu = hreg(AF);
		sum = acu + temp + TSTFLAG(C);
		cbits = acu ^ temp ^ sum;
		AF = ((sum & 0xff) << 8) | (sum & 0xa8) |
			(((sum & 0xff) == 0) << 6) | (cbits & 0x10) |
			(((cbits >> 6) ^ (cbits >> 5)) & 4) |
			((cbits >> 8) & 1);
		break;
	case 0x8B:			/* ADC A,E */
		temp = lreg(DE);
		acu = hreg(AF);
		sum = acu + temp + TSTFLAG(C);
		cbits = acu ^ temp ^ sum;
		AF = ((sum & 0xff) << 8) | (sum & 0xa8) |
			(((sum & 0xff) == 0) << 6) | (cbits & 0x10) |
			(((cbits >> 6) ^ (cbits >> 5)) & 4) |
			((cbits >> 8) & 1);
		break;
	case 0x8C:			/* ADC A,H */
		temp = hreg(HL);
		acu = hreg(AF);
		sum = acu + temp + TSTFLAG(C);
		cbits = acu ^ temp ^ sum;
		AF = ((sum & 0xff) << 8) | (sum & 0xa8) |
			(((sum & 0xff) == 0) << 6) | (cbits & 0x10) |
			(((cbits >> 6) ^ (cbits >> 5)) & 4) |
			((cbits >> 8) & 1);
		break;
	case 0x8D:			/* ADC A,L */
		temp = lreg(HL);
		acu = hreg(AF);
		sum = acu + temp + TSTFLAG(C);
		cbits = acu ^ temp ^ sum;
		AF = ((sum & 0xff) << 8) | (sum & 0xa8) |
			(((sum & 0xff) == 0) << 6) | (cbits & 0x10) |
			(((cbits >> 6) ^ (cbits >> 5)) & 4) |
			((cbits >> 8) & 1);
		break;
	case 0x8E:			/* ADC A,(HL) */
		temp = GetBYTE(HL);
		acu = hreg(AF);
		sum = acu + temp + TSTFLAG(C);
		cbits = acu ^ temp ^ sum;
		AF = ((sum & 0xff) << 8) | (sum & 0xa8) |
			(((sum & 0xff) == 0) << 6) | (cbits & 0x10) |
			(((cbits >> 6) ^ (cbits >> 5)) & 4) |
			((cbits >> 8) & 1);
		break;
	case 0x8F:			/* ADC A,A */
		temp = hreg(AF);
		acu = hreg(AF);
		sum = acu + temp + TSTFLAG(C);
		cbits = acu ^ temp ^ sum;
		AF = ((sum & 0xff) << 8) | (sum & 0xa8) |
			(((sum & 0xff) == 0) << 6) | (cbits & 0x10) |
			(((cbits >> 6) ^ (cbits >> 5)) & 4) |
			((cbits >> 8) & 1);
		break;
	case 0x90:			/* SUB B */
		temp = hreg(BC);
		acu = hreg(AF);
		sum = acu - temp;
		cbits = acu ^ temp ^ sum;
		AF = ((sum & 0xff) << 8) | (sum & 0xa8) |
			(((sum & 0xff) == 0) << 6) | (cbits & 0x10) |
			(((cbits >> 6) ^ (cbits >> 5)) & 4) | 2 |
			((cbits >> 8) & 1);
		break;
	case 0x91:			/* SUB C */
		temp = lreg(BC);
		acu = hreg(AF);
		sum = acu - temp;
		cbits = acu ^ temp ^ sum;
		AF = ((sum & 0xff) << 8) | (sum & 0xa8) |
			(((sum & 0xff) == 0) << 6) | (cbits & 0x10) |
			(((cbits >> 6) ^ (cbits >> 5)) & 4) | 2 |
			((cbits >> 8) & 1);
		break;
	case 0x92:			/* SUB D */
		temp = hreg(DE);
		acu = hreg(AF);
		sum = acu - temp;
		cbits = acu ^ temp ^ sum;
		AF = ((sum & 0xff) << 8) | (sum & 0xa8) |
			(((sum & 0xff) == 0) << 6) | (cbits & 0x10) |
			(((cbits >> 6) ^ (cbits >> 5)) & 4) | 2 |
			((cbits >> 8) & 1);
		break;
	case 0x93:			/* SUB E */
		temp = lreg(DE);
		acu = hreg(AF);
		sum = acu - temp;
		cbits = acu ^ temp ^ sum;
		AF = ((sum & 0xff) << 8) | (sum & 0xa8) |
			(((sum & 0xff) == 0) << 6) | (cbits & 0x10) |
			(((cbits >> 6) ^ (cbits >> 5)) & 4) | 2 |
			((cbits >> 8) & 1);
		break;
	case 0x94:			/* SUB H */
		temp = hreg(HL);
		acu = hreg(AF);
		sum = acu - temp;
		cbits = acu ^ temp ^ sum;
		AF = ((sum & 0xff) << 8) | (sum & 0xa8) |
			(((sum & 0xff) == 0) << 6) | (cbits & 0x10) |
			(((cbits >> 6) ^ (cbits >> 5)) & 4) | 2 |
			((cbits >> 8) & 1);
		break;
	case 0x95:			/* SUB L */
		temp = lreg(HL);
		acu = hreg(AF);
		sum = acu - temp;
		cbits = acu ^ temp ^ sum;
		AF = ((sum & 0xff) << 8) | (sum & 0xa8) |
			(((sum & 0xff) == 0) << 6) | (cbits & 0x10) |
			(((cbits >> 6) ^ (cbits >> 5)) & 4) | 2 |
			((cbits >> 8) & 1);
		break;
	case 0x96:			/* SUB (HL) */
		temp = GetBYTE(HL);
		acu = hreg(AF);
		sum = acu - temp;
		cbits = acu ^ temp ^ sum;
		AF = ((sum & 0xff) << 8) | (sum & 0xa8) |
			(((sum & 0xff) == 0) << 6) | (cbits & 0x10) |
			(((cbits >> 6) ^ (cbits >> 5)) & 4) | 2 |
			((cbits >> 8) & 1);
		break;
	case 0x97:			/* SUB A */
		temp = hreg(AF);
		acu = hreg(AF);
		sum = acu - temp;
		cbits = acu ^ temp ^ sum;
		AF = ((sum & 0xff) << 8) | (sum & 0xa8) |
			(((sum & 0xff) == 0) << 6) | (cbits & 0x10) |
			(((cbits >> 6) ^ (cbits >> 5)) & 4) | 2 |
			((cbits >> 8) & 1);
		break;
	case 0x98:			/* SBC A,B */
		temp = hreg(BC);
		acu = hreg(AF);
		sum = acu - temp - TSTFLAG(C);
		cbits = acu ^ temp ^ sum;
		AF = ((sum & 0xff) << 8) | (sum & 0xa8) |
			(((sum & 0xff) == 0) << 6) | (cbits & 0x10) |
			(((cbits >> 6) ^ (cbits >> 5)) & 4) | 2 |
			((cbits >> 8) & 1);
		break;
	case 0x99:			/* SBC A,C */
		temp = lreg(BC);
		acu = hreg(AF);
		sum = acu - temp - TSTFLAG(C);
		cbits = acu ^ temp ^ sum;
		AF = ((sum & 0xff) << 8) | (sum & 0xa8) |
			(((sum & 0xff) == 0) << 6) | (cbits & 0x10) |
			(((cbits >> 6) ^ (cbits >> 5)) & 4) | 2 |
			((cbits >> 8) & 1);
		break;
	case 0x9A:			/* SBC A,D */
		temp = hreg(DE);
		acu = hreg(AF);
		sum = acu - temp - TSTFLAG(C);
		cbits = acu ^ temp ^ sum;
		AF = ((sum & 0xff) << 8) | (sum & 0xa8) |
			(((sum & 0xff) == 0) << 6) | (cbits & 0x10) |
			(((cbits >> 6) ^ (cbits >> 5)) & 4) | 2 |
			((cbits >> 8) & 1);
		break;
	case 0x9B:			/* SBC A,E */
		temp = lreg(DE);
		acu = hreg(AF);
		sum = acu - temp - TSTFLAG(C);
		cbits = acu ^ temp ^ sum;
		AF = ((sum & 0xff) << 8) | (sum & 0xa8) |
			(((sum & 0xff) == 0) << 6) | (cbits & 0x10) |
			(((cbits >> 6) ^ (cbits >> 5)) & 4) | 2 |
			((cbits >> 8) & 1);
		break;
	case 0x9C:			/* SBC A,H */
		temp = hreg(HL);
		acu = hreg(AF);
		sum = acu - temp - TSTFLAG(C);
		cbits = acu ^ temp ^ sum;
		AF = ((sum & 0xff) << 8) | (sum & 0xa8) |
			(((sum & 0xff) == 0) << 6) | (cbits & 0x10) |
			(((cbits >> 6) ^ (cbits >> 5)) & 4) | 2 |
			((cbits >> 8) & 1);
		break;
	case 0x9D:			/* SBC A,L */
		temp = lreg(HL);
		acu = hreg(AF);
		sum = acu - temp - TSTFLAG(C);
		cbits = acu ^ temp ^ sum;
		AF = ((sum & 0xff) << 8) | (sum & 0xa8) |
			(((sum & 0xff) == 0) << 6) | (cbits & 0x10) |
			(((cbits >> 6) ^ (cbits >> 5)) & 4) | 2 |
			((cbits >> 8) & 1);
		break;
	case 0x9E:			/* SBC A,(HL) */
		temp = GetBYTE(HL);
		acu = hreg(AF);
		sum = acu - temp - TSTFLAG(C);
		cbits = acu ^ temp ^ sum;
		AF = ((sum & 0xff) << 8) | (sum & 0xa8) |
			(((sum & 0xff) == 0) << 6) | (cbits & 0x10) |
			(((cbits >> 6) ^ (cbits >> 5)) & 4) | 2 |
			((cbits >> 8) & 1);
		break;
	case 0x9F:			/* SBC A,A */
		temp = hreg(AF);
		acu = hreg(AF);
		sum = acu - temp - TSTFLAG(C);
		cbits = acu ^ temp ^ sum;
		AF = ((sum & 0xff) << 8) | (sum & 0xa8) |
			(((sum & 0xff) == 0) << 6) | (cbits & 0x10) |
			(((cbits >> 6) ^ (cbits >> 5)) & 4) | 2 |
			((cbits >> 8) & 1);
		break;
	case 0xA0:			/* AND B */
		sum = ((AF & (BC)) >> 8) & 0xff;
		AF = (sum << 8) | (sum & 0xa8) |
			((sum == 0) << 6) | 0x10 | partab[sum];
		break;
	case 0xA1:			/* AND C */
		sum = ((AF >> 8) & BC) & 0xff;
		AF = (sum << 8) | (sum & 0xa8) | 0x10 |
			((sum == 0) << 6) | partab[sum];
		break;
	case 0xA2:			/* AND D */
		sum = ((AF & (DE)) >> 8) & 0xff;
		AF = (sum << 8) | (sum & 0xa8) |
			((sum == 0) << 6) | 0x10 | partab[sum];
		break;
	case 0xA3:			/* AND E */
		sum = ((AF >> 8) & DE) & 0xff;
		AF = (sum << 8) | (sum & 0xa8) | 0x10 |
			((sum == 0) << 6) | partab[sum];
		break;
	case 0xA4:			/* AND H */
		sum = ((AF & (HL)) >> 8) & 0xff;
		AF = (sum << 8) | (sum & 0xa8) |
			((sum == 0) << 6) | 0x10 | partab[sum];
		break;
	case 0xA5:			/* AND L */
		sum = ((AF >> 8) & HL) & 0xff;
		AF = (sum << 8) | (sum & 0xa8) | 0x10 |
			((sum == 0) << 6) | partab[sum];
		break;
	case 0xA6:			/* AND (HL) */
		sum = ((AF >> 8) & GetBYTE(HL)) & 0xff;
		AF = (sum << 8) | (sum & 0xa8) | 0x10 |
			((sum == 0) << 6) | partab[sum];
		break;
	case 0xA7:			/* AND A */
		sum = ((AF & (AF)) >> 8) & 0xff;
		AF = (sum << 8) | (sum & 0xa8) |
			((sum == 0) << 6) | 0x10 | partab[sum];
		break;
	case 0xA8:			/* XOR B */
		sum = ((AF ^ (BC)) >> 8) & 0xff;
		AF = (sum << 8) | (sum & 0xa8) | ((sum == 0) << 6) | partab[sum];
		break;
	case 0xA9:			/* XOR C */
		sum = ((AF >> 8) ^ BC) & 0xff;
		AF = (sum << 8) | (sum & 0xa8) | ((sum == 0) << 6) | partab[sum];
		break;
	case 0xAA:			/* XOR D */
		sum = ((AF ^ (DE)) >> 8) & 0xff;
		AF = (sum << 8) | (sum & 0xa8) | ((sum == 0) << 6) | partab[sum];
		break;
	case 0xAB:			/* XOR E */
		sum = ((AF >> 8) ^ DE) & 0xff;
		AF = (sum << 8) | (sum & 0xa8) | ((sum == 0) << 6) | partab[sum];
		break;
	case 0xAC:			/* XOR H */
		sum = ((AF ^ (HL)) >> 8) & 0xff;
		AF = (sum << 8) | (sum & 0xa8) | ((sum == 0) << 6) | partab[sum];
		break;
	case 0xAD:			/* XOR L */
		sum = ((AF >> 8) ^ HL) & 0xff;
		AF = (sum << 8) | (sum & 0xa8) | ((sum == 0) << 6) | partab[sum];
		break;
	case 0xAE:			/* XOR (HL) */
		sum = ((AF >> 8) ^ GetBYTE(HL)) & 0xff;
		AF = (sum << 8) | (sum & 0xa8) | ((sum == 0) << 6) | partab[sum];
		break;
	case 0xAF:			/* XOR A */
		sum = ((AF ^ (AF)) >> 8) & 0xff;
		AF = (sum << 8) | (sum & 0xa8) | ((sum == 0) << 6) | partab[sum];
		break;
	case 0xB0:			/* OR B */
		sum = ((AF | (BC)) >> 8) & 0xff;
		AF = (sum << 8) | (sum & 0xa8) | ((sum == 0) << 6) | partab[sum];
		break;
	case 0xB1:			/* OR C */
		sum = ((AF >> 8) | BC) & 0xff;
		AF = (sum << 8) | (sum & 0xa8) | ((sum == 0) << 6) | partab[sum];
		break;
	case 0xB2:			/* OR D */
		sum = ((AF | (DE)) >> 8) & 0xff;
		AF = (sum << 8) | (sum & 0xa8) | ((sum == 0) << 6) | partab[sum];
		break;
	case 0xB3:			/* OR E */
		sum = ((AF >> 8) | DE) & 0xff;
		AF = (sum << 8) | (sum & 0xa8) | ((sum == 0) << 6) | partab[sum];
		break;
	case 0xB4:			/* OR H */
		sum = ((AF | (HL)) >> 8) & 0xff;
		AF = (sum << 8) | (sum & 0xa8) | ((sum == 0) << 6) | partab[sum];
		break;
	case 0xB5:			/* OR L */
		sum = ((AF >> 8) | HL) & 0xff;
		AF = (sum << 8) | (sum & 0xa8) | ((sum == 0) << 6) | partab[sum];
		break;
	case 0xB6:			/* OR (HL) */
		sum = ((AF >> 8) | GetBYTE(HL)) & 0xff;
		AF = (sum << 8) | (sum & 0xa8) | ((sum == 0) << 6) | partab[sum];
		break;
	case 0xB7:			/* OR A */
		sum = ((AF | (AF)) >> 8) & 0xff;
		AF = (sum << 8) | (sum & 0xa8) | ((sum == 0) << 6) | partab[sum];
		break;
	case 0xB8:			/* CP B */
		temp = hreg(BC);
		AF = (AF & ~0x28) | (temp & 0x28);
		acu = hreg(AF);
		sum = acu - temp;
		cbits = acu ^ temp ^ sum;
		AF = (AF & ~0xff) | (sum & 0x80) |
			(((sum & 0xff) == 0) << 6) | (temp & 0x28) |
			(((cbits >> 6) ^ (cbits >> 5)) & 4) | 2 |
			(cbits & 0x10) | ((cbits >> 8) & 1);
		break;
	case 0xB9:			/* CP C */
		temp = lreg(BC);
		AF = (AF & ~0x28) | (temp & 0x28);
		acu = hreg(AF);
		sum = acu - temp;
		cbits = acu ^ temp ^ sum;
		AF = (AF & ~0xff) | (sum & 0x80) |
			(((sum & 0xff) == 0) << 6) | (temp & 0x28) |
			(((cbits >> 6) ^ (cbits >> 5)) & 4) | 2 |
			(cbits & 0x10) | ((cbits >> 8) & 1);
		break;
	case 0xBA:			/* CP D */
		temp = hreg(DE);
		AF = (AF & ~0x28) | (temp & 0x28);
		acu = hreg(AF);
		sum = acu - temp;
		cbits = acu ^ temp ^ sum;
		AF = (AF & ~0xff) | (sum & 0x80) |
			(((sum & 0xff) == 0) << 6) | (temp & 0x28) |
			(((cbits >> 6) ^ (cbits >> 5)) & 4) | 2 |
			(cbits & 0x10) | ((cbits >> 8) & 1);
		break;
	case 0xBB:			/* CP E */
		temp = lreg(DE);
		AF = (AF & ~0x28) | (temp & 0x28);
		acu = hreg(AF);
		sum = acu - temp;
		cbits = acu ^ temp ^ sum;
		AF = (AF & ~0xff) | (sum & 0x80) |
			(((sum & 0xff) == 0) << 6) | (temp & 0x28) |
			(((cbits >> 6) ^ (cbits >> 5)) & 4) | 2 |
			(cbits & 0x10) | ((cbits >> 8) & 1);
		break;
	case 0xBC:			/* CP H */
		temp = hreg(HL);
		AF = (AF & ~0x28) | (temp & 0x28);
		acu = hreg(AF);
		sum = acu - temp;
		cbits = acu ^ temp ^ sum;
		AF = (AF & ~0xff) | (sum & 0x80) |
			(((sum & 0xff) == 0) << 6) | (temp & 0x28) |
			(((cbits >> 6) ^ (cbits >> 5)) & 4) | 2 |
			(cbits & 0x10) | ((cbits >> 8) & 1);
		break;
	case 0xBD:			/* CP L */
		temp = lreg(HL);
		AF = (AF & ~0x28) | (temp & 0x28);
		acu = hreg(AF);
		sum = acu - temp;
		cbits = acu ^ temp ^ sum;
		AF = (AF & ~0xff) | (sum & 0x80) |
			(((sum & 0xff) == 0) << 6) | (temp & 0x28) |
			(((cbits >> 6) ^ (cbits >> 5)) & 4) | 2 |
			(cbits & 0x10) | ((cbits >> 8) & 1);
		break;
	case 0xBE:			/* CP (HL) */
		temp = GetBYTE(HL);
		AF = (AF & ~0x28) | (temp & 0x28);
		acu = hreg(AF);
		sum = acu - temp;
		cbits = acu ^ temp ^ sum;
		AF = (AF & ~0xff) | (sum & 0x80) |
			(((sum & 0xff) == 0) << 6) | (temp & 0x28) |
			(((cbits >> 6) ^ (cbits >> 5)) & 4) | 2 |
			(cbits & 0x10) | ((cbits >> 8) & 1);
		break;
	case 0xBF:			/* CP A */
		temp = hreg(AF);
		AF = (AF & ~0x28) | (temp & 0x28);
		acu = hreg(AF);
		sum = acu - temp;
		cbits = acu ^ temp ^ sum;
		AF = (AF & ~0xff) | (sum & 0x80) |
			(((sum & 0xff) == 0) << 6) | (temp & 0x28) |
			(((cbits >> 6) ^ (cbits >> 5)) & 4) | 2 |
			(cbits & 0x10) | ((cbits >> 8) & 1);
		break;
	case 0xC0:			/* RET NZ */
		if (!TSTFLAG(Z)) POP(PC);
		break;
	case 0xC1:			/* POP BC */
		POP(BC);
		break;
	case 0xC2:			/* JP NZ,nnnn */
		JPC(!TSTFLAG(Z));
		break;
	case 0xC3:			/* JP nnnn */
		JPC(1);
		break;
	case 0xC4:			/* CALL NZ,nnnn */
		CALLC(!TSTFLAG(Z));
		break;
	case 0xC5:			/* PUSH BC */
		PUSH(BC);
		break;
	case 0xC6:			/* ADD A,nn */
		temp = GetBYTE_pp(PC);
		acu = hreg(AF);
		sum = acu + temp;
		cbits = acu ^ temp ^ sum;
		AF = ((sum & 0xff) << 8) | (sum & 0xa8) |
			(((sum & 0xff) == 0) << 6) | (cbits & 0x10) |
			(((cbits >> 6) ^ (cbits >> 5)) & 4) |
			((cbits >> 8) & 1);
		break;
	case 0xC7:			/* RST 0 */
		PUSH(PC); PC = 0;
		break;
	case 0xC8:			/* RET Z */
		if (TSTFLAG(Z)) POP(PC);
		break;
	case 0xC9:			/* RET */
		POP(PC);
		break;
	case 0xCA:			/* JP Z,nnnn */
		JPC(TSTFLAG(Z));
		break;
	case 0xCB:			/* CB prefix */
		adr = HL;
		switch ((op = GetBYTE(PC)) & 7) {
		    /*
		     * Use default to supress compiler warning: "warning:
		     * 'acu' may be used uninitialized in this function"
		     */
		default:
		case 0: ++PC; acu = hreg(BC); break;
		case 1: ++PC; acu = lreg(BC); break;
		case 2: ++PC; acu = hreg(DE); break;
		case 3: ++PC; acu = lreg(DE); break;
		case 4: ++PC; acu = hreg(HL); break;
		case 5: ++PC; acu = lreg(HL); break;
		case 6: ++PC; acu = GetBYTE(adr);  break;
		case 7: ++PC; acu = hreg(AF); break;
		}
		switch (op & 0xc0) {
		    /*
		     * Use default to supress compiler warning: "warning:
		     * 'temp' may be used uninitialized in this function"
		     */
		default:
		case 0x00:		/* shift/rotate */
			switch (op & 0x38) {
			/*
			 * Use default to supress compiler warning: "warning:
			 * 'temp' may be used uninitialized in this function"
			 */
			default:
			case 0x00:	/* RLC */
				temp = (acu << 1) | (acu >> 7);
				cbits = temp & 1;
				goto cbshflg1;
			case 0x08:	/* RRC */
				temp = (acu >> 1) | (acu << 7);
				cbits = temp & 0x80;
				goto cbshflg1;
			case 0x10:	/* RL */
				temp = (acu << 1) | TSTFLAG(C);
				cbits = acu & 0x80;
				goto cbshflg1;
			case 0x18:	/* RR */
				temp = (acu >> 1) | (TSTFLAG(C) << 7);
				cbits = acu & 1;
				goto cbshflg1;
			case 0x20:	/* SLA */
				temp = acu << 1;
				cbits = acu & 0x80;
				goto cbshflg1;
			case 0x28:	/* SRA */
				temp = (acu >> 1) | (acu & 0x80);
				cbits = acu & 1;
				goto cbshflg1;
			case 0x30:	/* SLIA */
				temp = (acu << 1) | 1;
				cbits = acu & 0x80;
				goto cbshflg1;
			case 0x38:	/* SRL */
				temp = acu >> 1;
				cbits = acu & 1;
			cbshflg1:
				AF = (AF & ~0xff) | (temp & 0xa8) |
					(((temp & 0xff) == 0) << 6) |
					parity(temp) | !!cbits;
			}
			break;
		case 0x40:		/* BIT */
			if (acu & (1 << ((op >> 3) & 7)))
				AF = (AF & ~0xfe) | 0x10 |
				(((op & 0x38) == 0x38) << 7);
			else
				AF = (AF & ~0xfe) | 0x54;
			if ((op&7) != 6)
				AF |= (acu & 0x28);
			temp = acu;
			break;
		case 0x80:		/* RES */
			temp = acu & ~(1 << ((op >> 3) & 7));
			break;
		case 0xc0:		/* SET */
			temp = acu | (1 << ((op >> 3) & 7));
			break;
		}
		switch (op & 7) {
		case 0: Sethreg(BC, temp); break;
		case 1: Setlreg(BC, temp); break;
		case 2: Sethreg(DE, temp); break;
		case 3: Setlreg(DE, temp); break;
		case 4: Sethreg(HL, temp); break;
		case 5: Setlreg(HL, temp); break;
		case 6: PutBYTE(adr, temp);  break;
		case 7: Sethreg(AF, temp); break;
		}
		break;
	case 0xCC:			/* CALL Z,nnnn */
		CALLC(TSTFLAG(Z));
		break;
	case 0xCD:			/* CALL nnnn */
		CALLC(1);
		break;
	case 0xCE:			/* ADC A,nn */
		temp = GetBYTE_pp(PC);
		acu = hreg(AF);
		sum = acu + temp + TSTFLAG(C);
		cbits = acu ^ temp ^ sum;
		AF = ((sum & 0xff) << 8) | (sum & 0xa8) |
			(((sum & 0xff) == 0) << 6) | (cbits & 0x10) |
			(((cbits >> 6) ^ (cbits >> 5)) & 4) |
			((cbits >> 8) & 1);
		break;
	case 0xCF:			/* RST 8 */
		PUSH(PC); PC = 8;
		break;
	case 0xD0:			/* RET NC */
		if (!TSTFLAG(C)) POP(PC);
		break;
	case 0xD1:			/* POP DE */
		POP(DE);
		break;
	case 0xD2:			/* JP NC,nnnn */
		JPC(!TSTFLAG(C));
		break;
	case 0xD3:			/* OUT (nn),A */
		Output(GetBYTE_pp(PC), hreg(AF));
		break;
	case 0xD4:			/* CALL NC,nnnn */
		CALLC(!TSTFLAG(C));
		break;
	case 0xD5:			/* PUSH DE */
		PUSH(DE);
		break;
	case 0xD6:			/* SUB nn */
		temp = GetBYTE_pp(PC);
		acu = hreg(AF);
		sum = acu - temp;
		cbits = acu ^ temp ^ sum;
		AF = ((sum & 0xff) << 8) | (sum & 0xa8) |
			(((sum & 0xff) == 0) << 6) | (cbits & 0x10) |
			(((cbits >> 6) ^ (cbits >> 5)) & 4) | 2 |
			((cbits >> 8) & 1);
		break;
	case 0xD7:			/* RST 10H */
		PUSH(PC); PC = 0x10;
		break;
	case 0xD8:			/* RET C */
		if (TSTFLAG(C)) POP(PC);
		break;
	case 0xD9:			/* EXX */
		regs[regs_sel].bc = BC;
		regs[regs_sel].de = DE;
		regs[regs_sel].hl = HL;
		regs_sel = 1 - regs_sel;
		BC = regs[regs_sel].bc;
		DE = regs[regs_sel].de;
		HL = regs[regs_sel].hl;
		break;
	case 0xDA:			/* JP C,nnnn */
		JPC(TSTFLAG(C));
		break;
	case 0xDB:			/* IN A,(nn) */
		Sethreg(AF, Input(GetBYTE_pp(PC)));
		break;
	case 0xDC:			/* CALL C,nnnn */
		CALLC(TSTFLAG(C));
		break;
	case 0xDD:			/* DD prefix */
		switch (op = GetBYTE_pp(PC)) {
		case 0x09:			/* ADD IX,BC */
			IX &= 0xffff;
			BC &= 0xffff;
			sum = IX + BC;
			cbits = (IX ^ BC ^ sum) >> 8;
			IX = sum;
			AF = (AF & ~0x3b) | ((sum >> 8) & 0x28) |
				(cbits & 0x10) | ((cbits >> 8) & 1);
			break;
		case 0x19:			/* ADD IX,DE */
			IX &= 0xffff;
			DE &= 0xffff;
			sum = IX + DE;
			cbits = (IX ^ DE ^ sum) >> 8;
			IX = sum;
			AF = (AF & ~0x3b) | ((sum >> 8) & 0x28) |
				(cbits & 0x10) | ((cbits >> 8) & 1);
			break;
		case 0x21:			/* LD IX,nnnn */
			IX = GetWORD(PC);
			PC += 2;
			break;
		case 0x22:			/* LD (nnnn),IX */
			temp = GetWORD(PC);
			PutWORD(temp, IX);
			PC += 2;
			break;
		case 0x23:			/* INC IX */
			++IX;
			break;
		case 0x24:			/* INC IXH */
			IX += 0x100;
			temp = hreg(IX);
			AF = (AF & ~0xfe) | (temp & 0xa8) |
				(((temp & 0xff) == 0) << 6) |
				(((temp & 0xf) == 0) << 4) |
				((temp == 0x80) << 2);
			break;
		case 0x25:			/* DEC IXH */
			IX -= 0x100;
			temp = hreg(IX);
			AF = (AF & ~0xfe) | (temp & 0xa8) |
				(((temp & 0xff) == 0) << 6) |
				(((temp & 0xf) == 0xf) << 4) |
				((temp == 0x7f) << 2) | 2;
			break;
		case 0x26:			/* LD IXH,nn */
			Sethreg(IX, GetBYTE_pp(PC));
			break;
		case 0x29:			/* ADD IX,IX */
			IX &= 0xffff;
			sum = IX + IX;
			cbits = (IX ^ IX ^ sum) >> 8;
			IX = sum;
			AF = (AF & ~0x3b) | ((sum >> 8) & 0x28) |
				(cbits & 0x10) | ((cbits >> 8) & 1);
			break;
		case 0x2A:			/* LD IX,(nnnn) */
			temp = GetWORD(PC);
			IX = GetWORD(temp);
			PC += 2;
			break;
		case 0x2B:			/* DEC IX */
			--IX;
			break;
		case 0x2C:			/* INC IXL */
			temp = lreg(IX)+1;
			Setlreg(IX, temp);
			AF = (AF & ~0xfe) | (temp & 0xa8) |
				(((temp & 0xff) == 0) << 6) |
				(((temp & 0xf) == 0) << 4) |
				((temp == 0x80) << 2);
			break;
		case 0x2D:			/* DEC IXL */
			temp = lreg(IX)-1;
			Setlreg(IX, temp);
			AF = (AF & ~0xfe) | (temp & 0xa8) |
				(((temp & 0xff) == 0) << 6) |
				(((temp & 0xf) == 0xf) << 4) |
				((temp == 0x7f) << 2) | 2;
			break;
		case 0x2E:			/* LD IXL,nn */
			Setlreg(IX, GetBYTE_pp(PC));
			break;
		case 0x34:			/* INC (IX+dd) */
			adr = IX + (signed char) GetBYTE_pp(PC);
			temp = GetBYTE(adr)+1;
			PutBYTE(adr, temp);
			AF = (AF & ~0xfe) | (temp & 0xa8) |
				(((temp & 0xff) == 0) << 6) |
				(((temp & 0xf) == 0) << 4) |
				((temp == 0x80) << 2);
			break;
		case 0x35:			/* DEC (IX+dd) */
			adr = IX + (signed char) GetBYTE_pp(PC);
			temp = GetBYTE(adr)-1;
			PutBYTE(adr, temp);
			AF = (AF & ~0xfe) | (temp & 0xa8) |
				(((temp & 0xff) == 0) << 6) |
				(((temp & 0xf) == 0xf) << 4) |
				((temp == 0x7f) << 2) | 2;
			break;
		case 0x36:			/* LD (IX+dd),nn */
			adr = IX + (signed char) GetBYTE_pp(PC);
			PutBYTE(adr, GetBYTE_pp(PC));
			break;
		case 0x39:			/* ADD IX,SP */
			IX &= 0xffff;
			SP &= 0xffff;
			sum = IX + SP;
			cbits = (IX ^ SP ^ sum) >> 8;
			IX = sum;
			AF = (AF & ~0x3b) | ((sum >> 8) & 0x28) |
				(cbits & 0x10) | ((cbits >> 8) & 1);
			break;
		case 0x44:			/* LD B,IXH */
			Sethreg(BC, hreg(IX));
			break;
		case 0x45:			/* LD B,IXL */
			Sethreg(BC, lreg(IX));
			break;
		case 0x46:			/* LD B,(IX+dd) */
			adr = IX + (signed char) GetBYTE_pp(PC);
			Sethreg(BC, GetBYTE(adr));
			break;
		case 0x4C:			/* LD C,IXH */
			Setlreg(BC, hreg(IX));
			break;
		case 0x4D:			/* LD C,IXL */
			Setlreg(BC, lreg(IX));
			break;
		case 0x4E:			/* LD C,(IX+dd) */
			adr = IX + (signed char) GetBYTE_pp(PC);
			Setlreg(BC, GetBYTE(adr));
			break;
		case 0x54:			/* LD D,IXH */
			Sethreg(DE, hreg(IX));
			break;
		case 0x55:			/* LD D,IXL */
			Sethreg(DE, lreg(IX));
			break;
		case 0x56:			/* LD D,(IX+dd) */
			adr = IX + (signed char) GetBYTE_pp(PC);
			Sethreg(DE, GetBYTE(adr));
			break;
		case 0x5C:			/* LD E,H */
			Setlreg(DE, hreg(IX));
			break;
		case 0x5D:			/* LD E,L */
			Setlreg(DE, lreg(IX));
			break;
		case 0x5E:			/* LD E,(IX+dd) */
			adr = IX + (signed char) GetBYTE_pp(PC);
			Setlreg(DE, GetBYTE(adr));
			break;
		case 0x60:			/* LD IXH,B */
			Sethreg(IX, hreg(BC));
			break;
		case 0x61:			/* LD IXH,C */
			Sethreg(IX, lreg(BC));
			break;
		case 0x62:			/* LD IXH,D */
			Sethreg(IX, hreg(DE));
			break;
		case 0x63:			/* LD IXH,E */
			Sethreg(IX, lreg(DE));
			break;
		case 0x64:			/* LD IXH,IXH */
			/* nop */
			break;
		case 0x65:			/* LD IXH,IXL */
			Sethreg(IX, lreg(IX));
			break;
		case 0x66:			/* LD H,(IX+dd) */
			adr = IX + (signed char) GetBYTE_pp(PC);
			Sethreg(HL, GetBYTE(adr));
			break;
		case 0x67:			/* LD IXH,A */
			Sethreg(IX, hreg(AF));
			break;
		case 0x68:			/* LD IXL,B */
			Setlreg(IX, hreg(BC));
			break;
		case 0x69:			/* LD IXL,C */
			Setlreg(IX, lreg(BC));
			break;
		case 0x6A:			/* LD IXL,D */
			Setlreg(IX, hreg(DE));
			break;
		case 0x6B:			/* LD IXL,E */
			Setlreg(IX, lreg(DE));
			break;
		case 0x6C:			/* LD IXL,IXH */
			Setlreg(IX, hreg(IX));
			break;
		case 0x6D:			/* LD IXL,IXL */
			/* nop */
			break;
		case 0x6E:			/* LD L,(IX+dd) */
			adr = IX + (signed char) GetBYTE_pp(PC);
			Setlreg(HL, GetBYTE(adr));
			break;
		case 0x6F:			/* LD IXL,A */
			Setlreg(IX, hreg(AF));
			break;
		case 0x70:			/* LD (IX+dd),B */
			adr = IX + (signed char) GetBYTE_pp(PC);
			PutBYTE(adr, hreg(BC));
			break;
		case 0x71:			/* LD (IX+dd),C */
			adr = IX + (signed char) GetBYTE_pp(PC);
			PutBYTE(adr, lreg(BC));
			break;
		case 0x72:			/* LD (IX+dd),D */
			adr = IX + (signed char) GetBYTE_pp(PC);
			PutBYTE(adr, hreg(DE));
			break;
		case 0x73:			/* LD (IX+dd),E */
			adr = IX + (signed char) GetBYTE_pp(PC);
			PutBYTE(adr, lreg(DE));
			break;
		case 0x74:			/* LD (IX+dd),H */
			adr = IX + (signed char) GetBYTE_pp(PC);
			PutBYTE(adr, hreg(HL));
			break;
		case 0x75:			/* LD (IX+dd),L */
			adr = IX + (signed char) GetBYTE_pp(PC);
			PutBYTE(adr, lreg(HL));
			break;
		case 0x77:			/* LD (IX+dd),A */
			adr = IX + (signed char) GetBYTE_pp(PC);
			PutBYTE(adr, hreg(AF));
			break;
		case 0x7C:			/* LD A,IXH */
			Sethreg(AF, hreg(IX));
			break;
		case 0x7D:			/* LD A,IXL */
			Sethreg(AF, lreg(IX));
			break;
		case 0x7E:			/* LD A,(IX+dd) */
			adr = IX + (signed char) GetBYTE_pp(PC);
			Sethreg(AF, GetBYTE(adr));
			break;
		case 0x84:			/* ADD A,IXH */
			temp = hreg(IX);
			acu = hreg(AF);
			sum = acu + temp;
			cbits = acu ^ temp ^ sum;
			AF = ((sum & 0xff) << 8) | (sum & 0xa8) |
				(((sum & 0xff) == 0) << 6) | (cbits & 0x10) |
				(((cbits >> 6) ^ (cbits >> 5)) & 4) |
				((cbits >> 8) & 1);
			break;
		case 0x85:			/* ADD A,IXL */
			temp = lreg(IX);
			acu = hreg(AF);
			sum = acu + temp;
			cbits = acu ^ temp ^ sum;
			AF = ((sum & 0xff) << 8) | (sum & 0xa8) |
				(((sum & 0xff) == 0) << 6) | (cbits & 0x10) |
				(((cbits >> 6) ^ (cbits >> 5)) & 4) |
				((cbits >> 8) & 1);
			break;
		case 0x86:			/* ADD A,(IX+dd) */
			adr = IX + (signed char) GetBYTE_pp(PC);
			temp = GetBYTE(adr);
			acu = hreg(AF);
			sum = acu + temp;
			cbits = acu ^ temp ^ sum;
			AF = ((sum & 0xff) << 8) | (sum & 0xa8) |
				(((sum & 0xff) == 0) << 6) | (cbits & 0x10) |
				(((cbits >> 6) ^ (cbits >> 5)) & 4) |
				((cbits >> 8) & 1);
			break;
		case 0x8C:			/* ADC A,IXH */
			temp = hreg(IX);
			acu = hreg(AF);
			sum = acu + temp + TSTFLAG(C);
			cbits = acu ^ temp ^ sum;
			AF = ((sum & 0xff) << 8) | (sum & 0xa8) |
				(((sum & 0xff) == 0) << 6) | (cbits & 0x10) |
				(((cbits >> 6) ^ (cbits >> 5)) & 4) |
				((cbits >> 8) & 1);
			break;
		case 0x8D:			/* ADC A,IXL */
			temp = lreg(IX);
			acu = hreg(AF);
			sum = acu + temp + TSTFLAG(C);
			cbits = acu ^ temp ^ sum;
			AF = ((sum & 0xff) << 8) | (sum & 0xa8) |
				(((sum & 0xff) == 0) << 6) | (cbits & 0x10) |
				(((cbits >> 6) ^ (cbits >> 5)) & 4) |
				((cbits >> 8) & 1);
			break;
		case 0x8E:			/* ADC A,(IX+dd) */
			adr = IX + (signed char) GetBYTE_pp(PC);
			temp = GetBYTE(adr);
			acu = hreg(AF);
			sum = acu + temp + TSTFLAG(C);
			cbits = acu ^ temp ^ sum;
			AF = ((sum & 0xff) << 8) | (sum & 0xa8) |
				(((sum & 0xff) == 0) << 6) | (cbits & 0x10) |
				(((cbits >> 6) ^ (cbits >> 5)) & 4) |
				((cbits >> 8) & 1);
			break;
		case 0x94:			/* SUB IXH */
			temp = hreg(IX);
			acu = hreg(AF);
			sum = acu - temp;
			cbits = acu ^ temp ^ sum;
			AF = ((sum & 0xff) << 8) | (sum & 0xa8) |
				(((sum & 0xff) == 0) << 6) | (cbits & 0x10) |
				(((cbits >> 6) ^ (cbits >> 5)) & 4) | 2 |
				((cbits >> 8) & 1);
			break;
		case 0x95:			/* SUB IXL */
			temp = lreg(IX);
			acu = hreg(AF);
			sum = acu - temp;
			cbits = acu ^ temp ^ sum;
			AF = ((sum & 0xff) << 8) | (sum & 0xa8) |
				(((sum & 0xff) == 0) << 6) | (cbits & 0x10) |
				(((cbits >> 6) ^ (cbits >> 5)) & 4) | 2 |
				((cbits >> 8) & 1);
			break;
		case 0x96:			/* SUB (IX+dd) */
			adr = IX + (signed char) GetBYTE_pp(PC);
			temp = GetBYTE(adr);
			acu = hreg(AF);
			sum = acu - temp;
			cbits = acu ^ temp ^ sum;
			AF = ((sum & 0xff) << 8) | (sum & 0xa8) |
				(((sum & 0xff) == 0) << 6) | (cbits & 0x10) |
				(((cbits >> 6) ^ (cbits >> 5)) & 4) | 2 |
				((cbits >> 8) & 1);
			break;
		case 0x9C:			/* SBC A,IXH */
			temp = hreg(IX);
			acu = hreg(AF);
			sum = acu - temp - TSTFLAG(C);
			cbits = acu ^ temp ^ sum;
			AF = ((sum & 0xff) << 8) | (sum & 0xa8) |
				(((sum & 0xff) == 0) << 6) | (cbits & 0x10) |
				(((cbits >> 6) ^ (cbits >> 5)) & 4) | 2 |
				((cbits >> 8) & 1);
			break;
		case 0x9D:			/* SBC A,IXL */
			temp = lreg(IX);
			acu = hreg(AF);
			sum = acu - temp - TSTFLAG(C);
			cbits = acu ^ temp ^ sum;
			AF = ((sum & 0xff) << 8) | (sum & 0xa8) |
				(((sum & 0xff) == 0) << 6) | (cbits & 0x10) |
				(((cbits >> 6) ^ (cbits >> 5)) & 4) | 2 |
				((cbits >> 8) & 1);
			break;
		case 0x9E:			/* SBC A,(IX+dd) */
			adr = IX + (signed char) GetBYTE_pp(PC);
			temp = GetBYTE(adr);
			acu = hreg(AF);
			sum = acu - temp - TSTFLAG(C);
			cbits = acu ^ temp ^ sum;
			AF = ((sum & 0xff) << 8) | (sum & 0xa8) |
				(((sum & 0xff) == 0) << 6) | (cbits & 0x10) |
				(((cbits >> 6) ^ (cbits >> 5)) & 4) | 2 |
				((cbits >> 8) & 1);
			break;
		case 0xA4:			/* AND IXH */
			sum = ((AF & (IX)) >> 8) & 0xff;
			AF = (sum << 8) | (sum & 0xa8) |
				((sum == 0) << 6) | 0x10 | partab[sum];
			break;
		case 0xA5:			/* AND IXL */
			sum = ((AF >> 8) & IX) & 0xff;
			AF = (sum << 8) | (sum & 0xa8) | 0x10 |
				((sum == 0) << 6) | partab[sum];
			break;
		case 0xA6:			/* AND (IX+dd) */
			adr = IX + (signed char) GetBYTE_pp(PC);
			sum = ((AF >> 8) & GetBYTE(adr)) & 0xff;
			AF = (sum << 8) | (sum & 0xa8) | 0x10 |
				((sum == 0) << 6) | partab[sum];
			break;
		case 0xAC:			/* XOR IXH */
			sum = ((AF ^ (IX)) >> 8) & 0xff;
			AF = (sum << 8) | (sum & 0xa8) | ((sum == 0) << 6) | partab[sum];
			break;
		case 0xAD:			/* XOR IXL */
			sum = ((AF >> 8) ^ IX) & 0xff;
			AF = (sum << 8) | (sum & 0xa8) | ((sum == 0) << 6) | partab[sum];
			break;
		case 0xAE:			/* XOR (IX+dd) */
			adr = IX + (signed char) GetBYTE_pp(PC);
			sum = ((AF >> 8) ^ GetBYTE(adr)) & 0xff;
			AF = (sum << 8) | (sum & 0xa8) | ((sum == 0) << 6) | partab[sum];
			break;
		case 0xB4:			/* OR IXH */
			sum = ((AF | (IX)) >> 8) & 0xff;
			AF = (sum << 8) | (sum & 0xa8) | ((sum == 0) << 6) | partab[sum];
			break;
		case 0xB5:			/* OR IXL */
			sum = ((AF >> 8) | IX) & 0xff;
			AF = (sum << 8) | (sum & 0xa8) | ((sum == 0) << 6) | partab[sum];
			break;
		case 0xB6:			/* OR (IX+dd) */
			adr = IX + (signed char) GetBYTE_pp(PC);
			sum = ((AF >> 8) | GetBYTE(adr)) & 0xff;
			AF = (sum << 8) | (sum & 0xa8) | ((sum == 0) << 6) | partab[sum];
			break;
		case 0xBC:			/* CP IXH */
			temp = hreg(IX);
			AF = (AF & ~0x28) | (temp & 0x28);
			acu = hreg(AF);
			sum = acu - temp;
			cbits = acu ^ temp ^ sum;
			AF = (AF & ~0xff) | (sum & 0x80) |
				(((sum & 0xff) == 0) << 6) | (temp & 0x28) |
				(((cbits >> 6) ^ (cbits >> 5)) & 4) | 2 |
				(cbits & 0x10) | ((cbits >> 8) & 1);
			break;
		case 0xBD:			/* CP IXL */
			temp = lreg(IX);
			AF = (AF & ~0x28) | (temp & 0x28);
			acu = hreg(AF);
			sum = acu - temp;
			cbits = acu ^ temp ^ sum;
			AF = (AF & ~0xff) | (sum & 0x80) |
				(((sum & 0xff) == 0) << 6) | (temp & 0x28) |
				(((cbits >> 6) ^ (cbits >> 5)) & 4) | 2 |
				(cbits & 0x10) | ((cbits >> 8) & 1);
			break;
		case 0xBE:			/* CP (IX+dd) */
			adr = IX + (signed char) GetBYTE_pp(PC);
			temp = GetBYTE(adr);
			AF = (AF & ~0x28) | (temp & 0x28);
			acu = hreg(AF);
			sum = acu - temp;
			cbits = acu ^ temp ^ sum;
			AF = (AF & ~0xff) | (sum & 0x80) |
				(((sum & 0xff) == 0) << 6) | (temp & 0x28) |
				(((cbits >> 6) ^ (cbits >> 5)) & 4) | 2 |
				(cbits & 0x10) | ((cbits >> 8) & 1);
			break;
		case 0xCB:			/* CB prefix */
			adr = IX + (signed char) GetBYTE_pp(PC);
			switch ((op = GetBYTE(PC)) & 7) {
			    /*
			     * default: supresses compiler warning: "warning:
			     * 'acu' may be used uninitialized in this function"
			     */
			default:
			case 0: ++PC; acu = hreg(BC); break;
			case 1: ++PC; acu = lreg(BC); break;
			case 2: ++PC; acu = hreg(DE); break;
			case 3: ++PC; acu = lreg(DE); break;
			case 4: ++PC; acu = hreg(HL); break;
			case 5: ++PC; acu = lreg(HL); break;
			case 6: ++PC; acu = GetBYTE(adr);  break;
			case 7: ++PC; acu = hreg(AF); break;
			}
			switch (op & 0xc0) {
			/*
			 * Use default to supress compiler warning: "warning:
			 * 'temp' may be used uninitialized in this function"
			 */
			default:
			case 0x00:		/* shift/rotate */
				switch (op & 0x38) {
				/*
				 * Use default: to supress compiler warning
				 * about 'temp' being used uninitialized
				 */
				default:
				case 0x00:	/* RLC */
					temp = (acu << 1) | (acu >> 7);
					cbits = temp & 1;
					goto cbshflg2;
				case 0x08:	/* RRC */
					temp = (acu >> 1) | (acu << 7);
					cbits = temp & 0x80;
					goto cbshflg2;
				case 0x10:	/* RL */
					temp = (acu << 1) | TSTFLAG(C);
					cbits = acu & 0x80;
					goto cbshflg2;
				case 0x18:	/* RR */
					temp = (acu >> 1) | (TSTFLAG(C) << 7);
					cbits = acu & 1;
					goto cbshflg2;
				case 0x20:	/* SLA */
					temp = acu << 1;
					cbits = acu & 0x80;
					goto cbshflg2;
				case 0x28:	/* SRA */
					temp = (acu >> 1) | (acu & 0x80);
					cbits = acu & 1;
					goto cbshflg2;
				case 0x30:	/* SLIA */
					temp = (acu << 1) | 1;
					cbits = acu & 0x80;
					goto cbshflg2;
				case 0x38:	/* SRL */
					temp = acu >> 1;
					cbits = acu & 1;
				cbshflg2:
					AF = (AF & ~0xff) | (temp & 0xa8) |
						(((temp & 0xff) == 0) << 6) |
						parity(temp) | !!cbits;
				}
				break;
			case 0x40:		/* BIT */
				if (acu & (1 << ((op >> 3) & 7)))
					AF = (AF & ~0xfe) | 0x10 |
					(((op & 0x38) == 0x38) << 7);
				else
					AF = (AF & ~0xfe) | 0x54;
				if ((op&7) != 6)
					AF |= (acu & 0x28);
				temp = acu;
				break;
			case 0x80:		/* RES */
				temp = acu & ~(1 << ((op >> 3) & 7));
				break;
			case 0xc0:		/* SET */
				temp = acu | (1 << ((op >> 3) & 7));
				break;
			}
			switch (op & 7) {
			case 0: Sethreg(BC, temp); break;
			case 1: Setlreg(BC, temp); break;
			case 2: Sethreg(DE, temp); break;
			case 3: Setlreg(DE, temp); break;
			case 4: Sethreg(HL, temp); break;
			case 5: Setlreg(HL, temp); break;
			case 6: PutBYTE(adr, temp);  break;
			case 7: Sethreg(AF, temp); break;
			}
			break;
		case 0xE1:			/* POP IX */
			POP(IX);
			break;
		case 0xE3:			/* EX (SP),IX */
			temp = IX; POP(IX); PUSH(temp);
			break;
		case 0xE5:			/* PUSH IX */
			PUSH(IX);
			break;
		case 0xE9:			/* JP (IX) */
			PC = IX;
			break;
		case 0xF9:			/* LD SP,IX */
			SP = IX;
			break;
		default: PC--;		/* ignore DD */
		}
		break;
	case 0xDE:			/* SBC A,nn */
		temp = GetBYTE_pp(PC);
		acu = hreg(AF);
		sum = acu - temp - TSTFLAG(C);
		cbits = acu ^ temp ^ sum;
		AF = ((sum & 0xff) << 8) | (sum & 0xa8) |
			(((sum & 0xff) == 0) << 6) | (cbits & 0x10) |
			(((cbits >> 6) ^ (cbits >> 5)) & 4) | 2 |
			((cbits >> 8) & 1);
		break;
	case 0xDF:			/* RST 18H */
		PUSH(PC); PC = 0x18;
		break;
	case 0xE0:			/* RET PO */
		if (!TSTFLAG(P)) POP(PC);
		break;
	case 0xE1:			/* POP HL */
		POP(HL);
		break;
	case 0xE2:			/* JP PO,nnnn */
		JPC(!TSTFLAG(P));
		break;
	case 0xE3:			/* EX (SP),HL */
		temp = HL; POP(HL); PUSH(temp);
		break;
	case 0xE4:			/* CALL PO,nnnn */
		CALLC(!TSTFLAG(P));
		break;
	case 0xE5:			/* PUSH HL */
		PUSH(HL);
		break;
	case 0xE6:			/* AND nn */
		sum = ((AF >> 8) & GetBYTE_pp(PC)) & 0xff;
		AF = (sum << 8) | (sum & 0xa8) | 0x10 |
			((sum == 0) << 6) | partab[sum];
		break;
	case 0xE7:			/* RST 20H */
		PUSH(PC); PC = 0x20;
		break;
	case 0xE8:			/* RET PE */
		if (TSTFLAG(P)) POP(PC);
		break;
	case 0xE9:			/* JP (HL) */
		PC = HL;
		break;
	case 0xEA:			/* JP PE,nnnn */
		JPC(TSTFLAG(P));
		break;
	case 0xEB:			/* EX DE,HL */
		temp = HL; HL = DE; DE = temp;
		break;
	case 0xEC:			/* CALL PE,nnnn */
		CALLC(TSTFLAG(P));
		break;
	case 0xED:			/* ED prefix */
		switch (op = GetBYTE_pp(PC)) {
		case 0x40:			/* IN B,(C) */
			temp = Input(lreg(BC));
			Sethreg(BC, temp);
			AF = (AF & ~0xfe) | (temp & 0xa8) |
				(((temp & 0xff) == 0) << 6) |
				parity(temp);
			break;
		case 0x41:			/* OUT (C),B */
			Output(lreg(BC), BC);
			break;
		case 0x42:			/* SBC HL,BC */
			HL &= 0xffff;
			BC &= 0xffff;
			sum = HL - BC - TSTFLAG(C);
			cbits = (HL ^ BC ^ sum) >> 8;
			HL = sum;
			AF = (AF & ~0xff) | ((sum >> 8) & 0xa8) |
				(((sum & 0xffff) == 0) << 6) |
				(((cbits >> 6) ^ (cbits >> 5)) & 4) |
				(cbits & 0x10) | 2 | ((cbits >> 8) & 1);
			break;
		case 0x43:			/* LD (nnnn),BC */
			temp = GetWORD(PC);
			PutWORD(temp, BC);
			PC += 2;
			break;
		case 0x44:			/* NEG */
			temp = hreg(AF);
			AF = (-(AF & 0xff00) & 0xff00);
			AF |= ((AF >> 8) & 0xa8) | (((AF & 0xff00) == 0) << 6) |
				(((temp & 0x0f) != 0) << 4) | ((temp == 0x80) << 2) |
				2 | (temp != 0);
			break;
		case 0x45:			/* RETN */
			IFF |= IFF >> 1;
			POP(PC);
			break;
		case 0x46:			/* IM 0 */
			/* interrupt mode 0 */
			break;
		case 0x47:			/* LD I,A */
			ir = (ir & 255) | (AF & ~255);
			break;
		case 0x48:			/* IN C,(C) */
			temp = Input(lreg(BC));
			Setlreg(BC, temp);
			AF = (AF & ~0xfe) | (temp & 0xa8) |
				(((temp & 0xff) == 0) << 6) |
				parity(temp);
			break;
		case 0x49:			/* OUT (C),C */
			Output(lreg(BC), BC);
			break;
		case 0x4A:			/* ADC HL,BC */
			HL &= 0xffff;
			BC &= 0xffff;
			sum = HL + BC + TSTFLAG(C);
			cbits = (HL ^ BC ^ sum) >> 8;
			HL = sum;
			AF = (AF & ~0xff) | ((sum >> 8) & 0xa8) |
				(((sum & 0xffff) == 0) << 6) |
				(((cbits >> 6) ^ (cbits >> 5)) & 4) |
				(cbits & 0x10) | ((cbits >> 8) & 1);
			break;
		case 0x4B:			/* LD BC,(nnnn) */
			temp = GetWORD(PC);
			BC = GetWORD(temp);
			PC += 2;
			break;
		case 0x4D:			/* RETI */
			IFF |= IFF >> 1;
			POP(PC);
			break;
		case 0x4F:			/* LD R,A */
			ir = (ir & ~255) | ((AF >> 8) & 255);
			break;
		case 0x50:			/* IN D,(C) */
			temp = Input(lreg(BC));
			Sethreg(DE, temp);
			AF = (AF & ~0xfe) | (temp & 0xa8) |
				(((temp & 0xff) == 0) << 6) |
				parity(temp);
			break;
		case 0x51:			/* OUT (C),D */
			Output(lreg(BC), DE);
			break;
		case 0x52:			/* SBC HL,DE */
			HL &= 0xffff;
			DE &= 0xffff;
			sum = HL - DE - TSTFLAG(C);
			cbits = (HL ^ DE ^ sum) >> 8;
			HL = sum;
			AF = (AF & ~0xff) | ((sum >> 8) & 0xa8) |
				(((sum & 0xffff) == 0) << 6) |
				(((cbits >> 6) ^ (cbits >> 5)) & 4) |
				(cbits & 0x10) | 2 | ((cbits >> 8) & 1);
			break;
		case 0x53:			/* LD (nnnn),DE */
			temp = GetWORD(PC);
			PutWORD(temp, DE);
			PC += 2;
			break;
		case 0x56:			/* IM 1 */
			/* interrupt mode 1 */
			break;
		case 0x57:			/* LD A,I */
			AF = (AF & 0x29) | (ir & ~255) | ((ir >> 8) & 0x80) | (((ir & ~255) == 0) << 6) | ((IFF & 2) << 1);
			break;
		case 0x58:			/* IN E,(C) */
			temp = Input(lreg(BC));
			Setlreg(DE, temp);
			AF = (AF & ~0xfe) | (temp & 0xa8) |
				(((temp & 0xff) == 0) << 6) |
				parity(temp);
			break;
		case 0x59:			/* OUT (C),E */
			Output(lreg(BC), DE);
			break;
		case 0x5A:			/* ADC HL,DE */
			HL &= 0xffff;
			DE &= 0xffff;
			sum = HL + DE + TSTFLAG(C);
			cbits = (HL ^ DE ^ sum) >> 8;
			HL = sum;
			AF = (AF & ~0xff) | ((sum >> 8) & 0xa8) |
				(((sum & 0xffff) == 0) << 6) |
				(((cbits >> 6) ^ (cbits >> 5)) & 4) |
				(cbits & 0x10) | ((cbits >> 8) & 1);
			break;
		case 0x5B:			/* LD DE,(nnnn) */
			temp = GetWORD(PC);
			DE = GetWORD(temp);
			PC += 2;
			break;
		case 0x5E:			/* IM 2 */
			/* interrupt mode 2 */
			break;
		case 0x5F:			/* LD A,R */
			AF = (AF & 0x29) | ((ir & 255) << 8) | (ir & 0x80) | (((ir & 255) == 0) << 6) | ((IFF & 2) << 1);
			break;
		case 0x60:			/* IN H,(C) */
			temp = Input(lreg(BC));
			Sethreg(HL, temp);
			AF = (AF & ~0xfe) | (temp & 0xa8) |
				(((temp & 0xff) == 0) << 6) |
				parity(temp);
			break;
		case 0x61:			/* OUT (C),H */
			Output(lreg(BC), HL);
			break;
		case 0x62:			/* SBC HL,HL */
			HL &= 0xffff;
			sum = HL - HL - TSTFLAG(C);
			cbits = (HL ^ HL ^ sum) >> 8;
			HL = sum;
			AF = (AF & ~0xff) | ((sum >> 8) & 0xa8) |
				(((sum & 0xffff) == 0) << 6) |
				(((cbits >> 6) ^ (cbits >> 5)) & 4) |
				(cbits & 0x10) | 2 | ((cbits >> 8) & 1);
			break;
		case 0x63:			/* LD (nnnn),HL */
			temp = GetWORD(PC);
			PutWORD(temp, HL);
			PC += 2;
			break;
		case 0x67:			/* RRD */
			temp = GetBYTE(HL);
			acu = hreg(AF);
			PutBYTE(HL, hdig(temp) | (ldig(acu) << 4));
			acu = (acu & 0xf0) | ldig(temp);
			AF = (acu << 8) | (acu & 0xa8) | (((acu & 0xff) == 0) << 6) |
				partab[acu] | (AF & 1);
			break;
		case 0x68:			/* IN L,(C) */
			temp = Input(lreg(BC));
			Setlreg(HL, temp);
			AF = (AF & ~0xfe) | (temp & 0xa8) |
				(((temp & 0xff) == 0) << 6) |
				parity(temp);
			break;
		case 0x69:			/* OUT (C),L */
			Output(lreg(BC), HL);
			break;
		case 0x6A:			/* ADC HL,HL */
			HL &= 0xffff;
			sum = HL + HL + TSTFLAG(C);
			cbits = (HL ^ HL ^ sum) >> 8;
			HL = sum;
			AF = (AF & ~0xff) | ((sum >> 8) & 0xa8) |
				(((sum & 0xffff) == 0) << 6) |
				(((cbits >> 6) ^ (cbits >> 5)) & 4) |
				(cbits & 0x10) | ((cbits >> 8) & 1);
			break;
		case 0x6B:			/* LD HL,(nnnn) */
			temp = GetWORD(PC);
			HL = GetWORD(temp);
			PC += 2;
			break;
		case 0x6F:			/* RLD */
			temp = GetBYTE(HL);
			acu = hreg(AF);
			PutBYTE(HL, (ldig(temp) << 4) | ldig(acu));
			acu = (acu & 0xf0) | hdig(temp);
			AF = (acu << 8) | (acu & 0xa8) | (((acu & 0xff) == 0) << 6) |
				partab[acu] | (AF & 1);
			break;
		case 0x70:			/* IN (C) */
			temp = Input(lreg(BC));
			Setlreg(temp, temp);
			AF = (AF & ~0xfe) | (temp & 0xa8) |
				(((temp & 0xff) == 0) << 6) |
				parity(temp);
			break;
		case 0x71:			/* OUT (C),0 */
			Output(lreg(BC), 0);
			break;
		case 0x72:			/* SBC HL,SP */
			HL &= 0xffff;
			SP &= 0xffff;
			sum = HL - SP - TSTFLAG(C);
			cbits = (HL ^ SP ^ sum) >> 8;
			HL = sum;
			AF = (AF & ~0xff) | ((sum >> 8) & 0xa8) |
				(((sum & 0xffff) == 0) << 6) |
				(((cbits >> 6) ^ (cbits >> 5)) & 4) |
				(cbits & 0x10) | 2 | ((cbits >> 8) & 1);
			break;
		case 0x73:			/* LD (nnnn),SP */
			temp = GetWORD(PC);
			PutWORD(temp, SP);
			PC += 2;
			break;
		case 0x78:			/* IN A,(C) */
			temp = Input(lreg(BC));
			Sethreg(AF, temp);
			AF = (AF & ~0xfe) | (temp & 0xa8) |
				(((temp & 0xff) == 0) << 6) |
				parity(temp);
			break;
		case 0x79:			/* OUT (C),A */
			Output(lreg(BC), AF);
			break;
		case 0x7A:			/* ADC HL,SP */
			HL &= 0xffff;
			SP &= 0xffff;
			sum = HL + SP + TSTFLAG(C);
			cbits = (HL ^ SP ^ sum) >> 8;
			HL = sum;
			AF = (AF & ~0xff) | ((sum >> 8) & 0xa8) |
				(((sum & 0xffff) == 0) << 6) |
				(((cbits >> 6) ^ (cbits >> 5)) & 4) |
				(cbits & 0x10) | ((cbits >> 8) & 1);
			break;
		case 0x7B:			/* LD SP,(nnnn) */
			temp = GetWORD(PC);
			SP = GetWORD(temp);
			PC += 2;
			break;
		case 0xA0:			/* LDI */
			acu = GetBYTE_pp(HL);
			PutBYTE_pp(DE, acu);
			acu += hreg(AF);
			AF = (AF & ~0x3e) | (acu & 8) | ((acu & 2) << 4) |
				(((--BC & 0xffff) != 0) << 2);
			break;
		case 0xA1:			/* CPI */
			acu = hreg(AF);
			temp = GetBYTE_pp(HL);
			sum = acu - temp;
			cbits = acu ^ temp ^ sum;
			AF = (AF & ~0xfe) | (sum & 0x80) | (!(sum & 0xff) << 6) |
				(((sum - ((cbits&16)>>4))&2) << 4) | (cbits & 16) |
				((sum - ((cbits >> 4) & 1)) & 8) |
				((--BC & 0xffff) != 0) << 2 | 2;
			if ((sum & 15) == 8 && (cbits & 16) != 0)
				AF &= ~8;
			break;
		case 0xA2:			/* INI */
			PutBYTE(HL, Input(lreg(BC))); ++HL;
			SETFLAG(N, 1);
			SETFLAG(P, (--BC & 0xffff) != 0);
			break;
		case 0xA3:			/* OUTI */
			Output(lreg(BC), GetBYTE(HL)); ++HL;
			SETFLAG(N, 1);
			Sethreg(BC, hreg(BC) - 1);
			SETFLAG(Z, hreg(BC) == 0);
			break;
		case 0xA8:			/* LDD */
			acu = GetBYTE_mm(HL);
			PutBYTE_mm(DE, acu);
			acu += hreg(AF);
			AF = (AF & ~0x3e) | (acu & 8) | ((acu & 2) << 4) |
				(((--BC & 0xffff) != 0) << 2);
			break;
		case 0xA9:			/* CPD */
			acu = hreg(AF);
			temp = GetBYTE_mm(HL);
			sum = acu - temp;
			cbits = acu ^ temp ^ sum;
			AF = (AF & ~0xfe) | (sum & 0x80) | (!(sum & 0xff) << 6) |
				(((sum - ((cbits&16)>>4))&2) << 4) | (cbits & 16) |
				((sum - ((cbits >> 4) & 1)) & 8) |
				((--BC & 0xffff) != 0) << 2 | 2;
			if ((sum & 15) == 8 && (cbits & 16) != 0)
				AF &= ~8;
			break;
		case 0xAA:			/* IND */
			PutBYTE(HL, Input(lreg(BC))); --HL;
			SETFLAG(N, 1);
			Sethreg(BC, lreg(BC) - 1);
			SETFLAG(Z, lreg(BC) == 0);
			break;
		case 0xAB:			/* OUTD */
			Output(lreg(BC), GetBYTE(HL)); --HL;
			SETFLAG(N, 1);
			Sethreg(BC, hreg(BC) - 1);
			SETFLAG(Z, hreg(BC) == 0);
			break;
		case 0xB0:			/* LDIR */
			acu = hreg(AF);
			BC &= 0xffff;
			if (BC == 0)
			    BC = 0x10000;
			do {
				acu = GetBYTE_pp(HL);
				PutBYTE_pp(DE, acu);
			} while (--BC);
			acu += hreg(AF);
			AF = (AF & ~0x3e) | (acu & 8) | ((acu & 2) << 4);
			break;
		case 0xB1:			/* CPIR */
			acu = hreg(AF);
			BC &= 0xffff;
			if (BC == 0)
			    BC = 0x10000;
			do {
				temp = GetBYTE_pp(HL);
				op = --BC != 0;
				sum = acu - temp;
			} while (op && sum != 0);
			cbits = acu ^ temp ^ sum;
			AF = (AF & ~0xfe) | (sum & 0x80) | (!(sum & 0xff) << 6) |
				(((sum - ((cbits&16)>>4))&2) << 4) |
				(cbits & 16) | ((sum - ((cbits >> 4) & 1)) & 8) |
				op << 2 | 2;
			if ((sum & 15) == 8 && (cbits & 16) != 0)
				AF &= ~8;
			break;
		case 0xB2:			/* INIR */
			temp = hreg(BC);
			do {
				PutBYTE(HL, Input(lreg(BC))); ++HL;
			} while (--temp);
			Sethreg(BC, 0);
			SETFLAG(N, 1);
			SETFLAG(Z, 1);
			break;
		case 0xB3:			/* OTIR */
			temp = hreg(BC);
			do {
				Output(lreg(BC), GetBYTE(HL)); ++HL;
			} while (--temp);
			Sethreg(BC, 0);
			SETFLAG(N, 1);
			SETFLAG(Z, 1);
			break;
		case 0xB8:			/* LDDR */
			BC &= 0xffff;
			if (BC == 0)
			    BC = 0x10000;
			do {
				acu = GetBYTE_mm(HL);
				PutBYTE_mm(DE, acu);
			} while (--BC);
			acu += hreg(AF);
			AF = (AF & ~0x3e) | (acu & 8) | ((acu & 2) << 4);
			break;
		case 0xB9:			/* CPDR */
			acu = hreg(AF);
			BC &= 0xffff;
			if (BC == 0)
			    BC = 0x10000;
			do {
				temp = GetBYTE_mm(HL);
				op = --BC != 0;
				sum = acu - temp;
			} while (op && sum != 0);
			cbits = acu ^ temp ^ sum;
			AF = (AF & ~0xfe) | (sum & 0x80) | (!(sum & 0xff) << 6) |
				(((sum - ((cbits&16)>>4))&2) << 4) |
				(cbits & 16) | ((sum - ((cbits >> 4) & 1)) & 8) |
				op << 2 | 2;
			if ((sum & 15) == 8 && (cbits & 16) != 0)
				AF &= ~8;
			break;
		case 0xBA:			/* INDR */
			temp = hreg(BC);
			do {
				PutBYTE(HL, Input(lreg(BC))); --HL;
			} while (--temp);
			Sethreg(BC, 0);
			SETFLAG(N, 1);
			SETFLAG(Z, 1);
			break;
		case 0xBB:			/* OTDR */
			temp = hreg(BC);
			do {
				Output(lreg(BC), GetBYTE(HL)); --HL;
			} while (--temp);
			Sethreg(BC, 0);
			SETFLAG(N, 1);
			SETFLAG(Z, 1);
			break;
		default: if (0x40 <= op && op <= 0x7f) PC--;		/* ignore ED */
		}
		break;
	case 0xEE:			/* XOR nn */
		sum = ((AF >> 8) ^ GetBYTE_pp(PC)) & 0xff;
		AF = (sum << 8) | (sum & 0xa8) | ((sum == 0) << 6) | partab[sum];
		break;
	case 0xEF:			/* RST 28H */
		PUSH(PC); PC = 0x28;
		break;
	case 0xF0:			/* RET P */
		if (!TSTFLAG(S)) POP(PC);
		break;
	case 0xF1:			/* POP AF */
		POP(AF);
		break;
	case 0xF2:			/* JP P,nnnn */
		JPC(!TSTFLAG(S));
		break;
	case 0xF3:			/* DI */
		IFF = 0;
		break;
	case 0xF4:			/* CALL P,nnnn */
		CALLC(!TSTFLAG(S));
		break;
	case 0xF5:			/* PUSH AF */
		PUSH(AF);
		break;
	case 0xF6:			/* OR nn */
		sum = ((AF >> 8) | GetBYTE_pp(PC)) & 0xff;
		AF = (sum << 8) | (sum & 0xa8) | ((sum == 0) << 6) | partab[sum];
		break;
	case 0xF7:			/* RST 30H */
		PUSH(PC); PC = 0x30;
		break;
	case 0xF8:			/* RET M */
		if (TSTFLAG(S)) POP(PC);
		break;
	case 0xF9:			/* LD SP,HL */
		SP = HL;
		break;
	case 0xFA:			/* JP M,nnnn */
		JPC(TSTFLAG(S));
		break;
	case 0xFB:			/* EI */
		IFF = 3;
		break;
	case 0xFC:			/* CALL M,nnnn */
		CALLC(TSTFLAG(S));
		break;
	case 0xFD:			/* FD prefix */
		switch (op = GetBYTE_pp(PC)) {
		case 0x09:			/* ADD IY,BC */
			IY &= 0xffff;
			BC &= 0xffff;
			sum = IY + BC;
			cbits = (IY ^ BC ^ sum) >> 8;
			IY = sum;
			AF = (AF & ~0x3b) | ((sum >> 8) & 0x28) |
				(cbits & 0x10) | ((cbits >> 8) & 1);
			break;
		case 0x19:			/* ADD IY,DE */
			IY &= 0xffff;
			DE &= 0xffff;
			sum = IY + DE;
			cbits = (IY ^ DE ^ sum) >> 8;
			IY = sum;
			AF = (AF & ~0x3b) | ((sum >> 8) & 0x28) |
				(cbits & 0x10) | ((cbits >> 8) & 1);
			break;
		case 0x21:			/* LD IY,nnnn */
			IY = GetWORD(PC);
			PC += 2;
			break;
		case 0x22:			/* LD (nnnn),IY */
			temp = GetWORD(PC);
			PutWORD(temp, IY);
			PC += 2;
			break;
		case 0x23:			/* INC IY */
			++IY;
			break;
		case 0x24:			/* INC IYH */
			IY += 0x100;
			temp = hreg(IY);
			AF = (AF & ~0xfe) | (temp & 0xa8) |
				(((temp & 0xff) == 0) << 6) |
				(((temp & 0xf) == 0) << 4) |
				((temp == 0x80) << 2);
			break;
		case 0x25:			/* DEC IYH */
			IY -= 0x100;
			temp = hreg(IY);
			AF = (AF & ~0xfe) | (temp & 0xa8) |
				(((temp & 0xff) == 0) << 6) |
				(((temp & 0xf) == 0xf) << 4) |
				((temp == 0x7f) << 2) | 2;
			break;
		case 0x26:			/* LD IYH,nn */
			Sethreg(IY, GetBYTE_pp(PC));
			break;
		case 0x29:			/* ADD IY,IY */
			IY &= 0xffff;
			sum = IY + IY;
			cbits = (IY ^ IY ^ sum) >> 8;
			IY = sum;
			AF = (AF & ~0x3b) | ((sum >> 8) & 0x28) |
				(cbits & 0x10) | ((cbits >> 8) & 1);
			break;
		case 0x2A:			/* LD IY,(nnnn) */
			temp = GetWORD(PC);
			IY = GetWORD(temp);
			PC += 2;
			break;
		case 0x2B:			/* DEC IY */
			--IY;
			break;
		case 0x2C:			/* INC IYL */
			temp = lreg(IY)+1;
			Setlreg(IY, temp);
			AF = (AF & ~0xfe) | (temp & 0xa8) |
				(((temp & 0xff) == 0) << 6) |
				(((temp & 0xf) == 0) << 4) |
				((temp == 0x80) << 2);
			break;
		case 0x2D:			/* DEC IYL */
			temp = lreg(IY)-1;
			Setlreg(IY, temp);
			AF = (AF & ~0xfe) | (temp & 0xa8) |
				(((temp & 0xff) == 0) << 6) |
				(((temp & 0xf) == 0xf) << 4) |
				((temp == 0x7f) << 2) | 2;
			break;
		case 0x2E:			/* LD IYL,nn */
			Setlreg(IY, GetBYTE_pp(PC));
			break;
		case 0x34:			/* INC (IY+dd) */
			adr = IY + (signed char) GetBYTE_pp(PC);
			temp = GetBYTE(adr)+1;
			PutBYTE(adr, temp);
			AF = (AF & ~0xfe) | (temp & 0xa8) |
				(((temp & 0xff) == 0) << 6) |
				(((temp & 0xf) == 0) << 4) |
				((temp == 0x80) << 2);
			break;
		case 0x35:			/* DEC (IY+dd) */
			adr = IY + (signed char) GetBYTE_pp(PC);
			temp = GetBYTE(adr)-1;
			PutBYTE(adr, temp);
			AF = (AF & ~0xfe) | (temp & 0xa8) |
				(((temp & 0xff) == 0) << 6) |
				(((temp & 0xf) == 0xf) << 4) |
				((temp == 0x7f) << 2) | 2;
			break;
		case 0x36:			/* LD (IY+dd),nn */
			adr = IY + (signed char) GetBYTE_pp(PC);
			PutBYTE(adr, GetBYTE_pp(PC));
			break;
		case 0x39:			/* ADD IY,SP */
			IY &= 0xffff;
			SP &= 0xffff;
			sum = IY + SP;
			cbits = (IY ^ SP ^ sum) >> 8;
			IY = sum;
			AF = (AF & ~0x3b) | ((sum >> 8) & 0x28) |
				(cbits & 0x10) | ((cbits >> 8) & 1);
			break;
		case 0x44:			/* LD B,IYH */
			Sethreg(BC, hreg(IY));
			break;
		case 0x45:			/* LD B,IYL */
			Sethreg(BC, lreg(IY));
			break;
		case 0x46:			/* LD B,(IY+dd) */
			adr = IY + (signed char) GetBYTE_pp(PC);
			Sethreg(BC, GetBYTE(adr));
			break;
		case 0x4C:			/* LD C,IYH */
			Setlreg(BC, hreg(IY));
			break;
		case 0x4D:			/* LD C,IYL */
			Setlreg(BC, lreg(IY));
			break;
		case 0x4E:			/* LD C,(IY+dd) */
			adr = IY + (signed char) GetBYTE_pp(PC);
			Setlreg(BC, GetBYTE(adr));
			break;
		case 0x54:			/* LD D,IYH */
			Sethreg(DE, hreg(IY));
			break;
		case 0x55:			/* LD D,IYL */
			Sethreg(DE, lreg(IY));
			break;
		case 0x56:			/* LD D,(IY+dd) */
			adr = IY + (signed char) GetBYTE_pp(PC);
			Sethreg(DE, GetBYTE(adr));
			break;
		case 0x5C:			/* LD E,H */
			Setlreg(DE, hreg(IY));
			break;
		case 0x5D:			/* LD E,L */
			Setlreg(DE, lreg(IY));
			break;
		case 0x5E:			/* LD E,(IY+dd) */
			adr = IY + (signed char) GetBYTE_pp(PC);
			Setlreg(DE, GetBYTE(adr));
			break;
		case 0x60:			/* LD IYH,B */
			Sethreg(IY, hreg(BC));
			break;
		case 0x61:			/* LD IYH,C */
			Sethreg(IY, lreg(BC));
			break;
		case 0x62:			/* LD IYH,D */
			Sethreg(IY, hreg(DE));
			break;
		case 0x63:			/* LD IYH,E */
			Sethreg(IY, lreg(DE));
			break;
		case 0x64:			/* LD IYH,IYH */
			/* nop */
			break;
		case 0x65:			/* LD IYH,IYL */
			Sethreg(IY, lreg(IY));
			break;
		case 0x66:			/* LD H,(IY+dd) */
			adr = IY + (signed char) GetBYTE_pp(PC);
			Sethreg(HL, GetBYTE(adr));
			break;
		case 0x67:			/* LD IYH,A */
			Sethreg(IY, hreg(AF));
			break;
		case 0x68:			/* LD IYL,B */
			Setlreg(IY, hreg(BC));
			break;
		case 0x69:			/* LD IYL,C */
			Setlreg(IY, lreg(BC));
			break;
		case 0x6A:			/* LD IYL,D */
			Setlreg(IY, hreg(DE));
			break;
		case 0x6B:			/* LD IYL,E */
			Setlreg(IY, lreg(DE));
			break;
		case 0x6C:			/* LD IYL,IYH */
			Setlreg(IY, hreg(IY));
			break;
		case 0x6D:			/* LD IYL,IYL */
			/* nop */
			break;
		case 0x6E:			/* LD L,(IY+dd) */
			adr = IY + (signed char) GetBYTE_pp(PC);
			Setlreg(HL, GetBYTE(adr));
			break;
		case 0x6F:			/* LD IYL,A */
			Setlreg(IY, hreg(AF));
			break;
		case 0x70:			/* LD (IY+dd),B */
			adr = IY + (signed char) GetBYTE_pp(PC);
			PutBYTE(adr, hreg(BC));
			break;
		case 0x71:			/* LD (IY+dd),C */
			adr = IY + (signed char) GetBYTE_pp(PC);
			PutBYTE(adr, lreg(BC));
			break;
		case 0x72:			/* LD (IY+dd),D */
			adr = IY + (signed char) GetBYTE_pp(PC);
			PutBYTE(adr, hreg(DE));
			break;
		case 0x73:			/* LD (IY+dd),E */
			adr = IY + (signed char) GetBYTE_pp(PC);
			PutBYTE(adr, lreg(DE));
			break;
		case 0x74:			/* LD (IY+dd),H */
			adr = IY + (signed char) GetBYTE_pp(PC);
			PutBYTE(adr, hreg(HL));
			break;
		case 0x75:			/* LD (IY+dd),L */
			adr = IY + (signed char) GetBYTE_pp(PC);
			PutBYTE(adr, lreg(HL));
			break;
		case 0x77:			/* LD (IY+dd),A */
			adr = IY + (signed char) GetBYTE_pp(PC);
			PutBYTE(adr, hreg(AF));
			break;
		case 0x7C:			/* LD A,IYH */
			Sethreg(AF, hreg(IY));
			break;
		case 0x7D:			/* LD A,IYL */
			Sethreg(AF, lreg(IY));
			break;
		case 0x7E:			/* LD A,(IY+dd) */
			adr = IY + (signed char) GetBYTE_pp(PC);
			Sethreg(AF, GetBYTE(adr));
			break;
		case 0x84:			/* ADD A,IYH */
			temp = hreg(IY);
			acu = hreg(AF);
			sum = acu + temp;
			cbits = acu ^ temp ^ sum;
			AF = ((sum & 0xff) << 8) | (sum & 0xa8) |
				(((sum & 0xff) == 0) << 6) | (cbits & 0x10) |
				(((cbits >> 6) ^ (cbits >> 5)) & 4) |
				((cbits >> 8) & 1);
			break;
		case 0x85:			/* ADD A,IYL */
			temp = lreg(IY);
			acu = hreg(AF);
			sum = acu + temp;
			cbits = acu ^ temp ^ sum;
			AF = ((sum & 0xff) << 8) | (sum & 0xa8) |
				(((sum & 0xff) == 0) << 6) | (cbits & 0x10) |
				(((cbits >> 6) ^ (cbits >> 5)) & 4) |
				((cbits >> 8) & 1);
			break;
		case 0x86:			/* ADD A,(IY+dd) */
			adr = IY + (signed char) GetBYTE_pp(PC);
			temp = GetBYTE(adr);
			acu = hreg(AF);
			sum = acu + temp;
			cbits = acu ^ temp ^ sum;
			AF = ((sum & 0xff) << 8) | (sum & 0xa8) |
				(((sum & 0xff) == 0) << 6) | (cbits & 0x10) |
				(((cbits >> 6) ^ (cbits >> 5)) & 4) |
				((cbits >> 8) & 1);
			break;
		case 0x8C:			/* ADC A,IYH */
			temp = hreg(IY);
			acu = hreg(AF);
			sum = acu + temp + TSTFLAG(C);
			cbits = acu ^ temp ^ sum;
			AF = ((sum & 0xff) << 8) | (sum & 0xa8) |
				(((sum & 0xff) == 0) << 6) | (cbits & 0x10) |
				(((cbits >> 6) ^ (cbits >> 5)) & 4) |
				((cbits >> 8) & 1);
			break;
		case 0x8D:			/* ADC A,IYL */
			temp = lreg(IY);
			acu = hreg(AF);
			sum = acu + temp + TSTFLAG(C);
			cbits = acu ^ temp ^ sum;
			AF = ((sum & 0xff) << 8) | (sum & 0xa8) |
				(((sum & 0xff) == 0) << 6) | (cbits & 0x10) |
				(((cbits >> 6) ^ (cbits >> 5)) & 4) |
				((cbits >> 8) & 1);
			break;
		case 0x8E:			/* ADC A,(IY+dd) */
			adr = IY + (signed char) GetBYTE_pp(PC);
			temp = GetBYTE(adr);
			acu = hreg(AF);
			sum = acu + temp + TSTFLAG(C);
			cbits = acu ^ temp ^ sum;
			AF = ((sum & 0xff) << 8) | (sum & 0xa8) |
				(((sum & 0xff) == 0) << 6) | (cbits & 0x10) |
				(((cbits >> 6) ^ (cbits >> 5)) & 4) |
				((cbits >> 8) & 1);
			break;
		case 0x94:			/* SUB IYH */
			temp = hreg(IY);
			acu = hreg(AF);
			sum = acu - temp;
			cbits = acu ^ temp ^ sum;
			AF = ((sum & 0xff) << 8) | (sum & 0xa8) |
				(((sum & 0xff) == 0) << 6) | (cbits & 0x10) |
				(((cbits >> 6) ^ (cbits >> 5)) & 4) | 2 |
				((cbits >> 8) & 1);
			break;
		case 0x95:			/* SUB IYL */
			temp = lreg(IY);
			acu = hreg(AF);
			sum = acu - temp;
			cbits = acu ^ temp ^ sum;
			AF = ((sum & 0xff) << 8) | (sum & 0xa8) |
				(((sum & 0xff) == 0) << 6) | (cbits & 0x10) |
				(((cbits >> 6) ^ (cbits >> 5)) & 4) | 2 |
				((cbits >> 8) & 1);
			break;
		case 0x96:			/* SUB (IY+dd) */
			adr = IY + (signed char) GetBYTE_pp(PC);
			temp = GetBYTE(adr);
			acu = hreg(AF);
			sum = acu - temp;
			cbits = acu ^ temp ^ sum;
			AF = ((sum & 0xff) << 8) | (sum & 0xa8) |
				(((sum & 0xff) == 0) << 6) | (cbits & 0x10) |
				(((cbits >> 6) ^ (cbits >> 5)) & 4) | 2 |
				((cbits >> 8) & 1);
			break;
		case 0x9C:			/* SBC A,IYH */
			temp = hreg(IY);
			acu = hreg(AF);
			sum = acu - temp - TSTFLAG(C);
			cbits = acu ^ temp ^ sum;
			AF = ((sum & 0xff) << 8) | (sum & 0xa8) |
				(((sum & 0xff) == 0) << 6) | (cbits & 0x10) |
				(((cbits >> 6) ^ (cbits >> 5)) & 4) | 2 |
				((cbits >> 8) & 1);
			break;
		case 0x9D:			/* SBC A,IYL */
			temp = lreg(IY);
			acu = hreg(AF);
			sum = acu - temp - TSTFLAG(C);
			cbits = acu ^ temp ^ sum;
			AF = ((sum & 0xff) << 8) | (sum & 0xa8) |
				(((sum & 0xff) == 0) << 6) | (cbits & 0x10) |
				(((cbits >> 6) ^ (cbits >> 5)) & 4) | 2 |
				((cbits >> 8) & 1);
			break;
		case 0x9E:			/* SBC A,(IY+dd) */
			adr = IY + (signed char) GetBYTE_pp(PC);
			temp = GetBYTE(adr);
			acu = hreg(AF);
			sum = acu - temp - TSTFLAG(C);
			cbits = acu ^ temp ^ sum;
			AF = ((sum & 0xff) << 8) | (sum & 0xa8) |
				(((sum & 0xff) == 0) << 6) | (cbits & 0x10) |
				(((cbits >> 6) ^ (cbits >> 5)) & 4) | 2 |
				((cbits >> 8) & 1);
			break;
		case 0xA4:			/* AND IYH */
			sum = ((AF & (IY)) >> 8) & 0xff;
			AF = (sum << 8) | (sum & 0xa8) |
				((sum == 0) << 6) | 0x10 | partab[sum];
			break;
		case 0xA5:			/* AND IYL */
			sum = ((AF >> 8) & IY) & 0xff;
			AF = (sum << 8) | (sum & 0xa8) | 0x10 |
				((sum == 0) << 6) | partab[sum];
			break;
		case 0xA6:			/* AND (IY+dd) */
			adr = IY + (signed char) GetBYTE_pp(PC);
			sum = ((AF >> 8) & GetBYTE(adr)) & 0xff;
			AF = (sum << 8) | (sum & 0xa8) | 0x10 |
				((sum == 0) << 6) | partab[sum];
			break;
		case 0xAC:			/* XOR IYH */
			sum = ((AF ^ (IY)) >> 8) & 0xff;
			AF = (sum << 8) | (sum & 0xa8) | ((sum == 0) << 6) | partab[sum];
			break;
		case 0xAD:			/* XOR IYL */
			sum = ((AF >> 8) ^ IY) & 0xff;
			AF = (sum << 8) | (sum & 0xa8) | ((sum == 0) << 6) | partab[sum];
			break;
		case 0xAE:			/* XOR (IY+dd) */
			adr = IY + (signed char) GetBYTE_pp(PC);
			sum = ((AF >> 8) ^ GetBYTE(adr)) & 0xff;
			AF = (sum << 8) | (sum & 0xa8) | ((sum == 0) << 6) | partab[sum];
			break;
		case 0xB4:			/* OR IYH */
			sum = ((AF | (IY)) >> 8) & 0xff;
			AF = (sum << 8) | (sum & 0xa8) | ((sum == 0) << 6) | partab[sum];
			break;
		case 0xB5:			/* OR IYL */
			sum = ((AF >> 8) | IY) & 0xff;
			AF = (sum << 8) | (sum & 0xa8) | ((sum == 0) << 6) | partab[sum];
			break;
		case 0xB6:			/* OR (IY+dd) */
			adr = IY + (signed char) GetBYTE_pp(PC);
			sum = ((AF >> 8) | GetBYTE(adr)) & 0xff;
			AF = (sum << 8) | (sum & 0xa8) | ((sum == 0) << 6) | partab[sum];
			break;
		case 0xBC:			/* CP IYH */
			temp = hreg(IY);
			AF = (AF & ~0x28) | (temp & 0x28);
			acu = hreg(AF);
			sum = acu - temp;
			cbits = acu ^ temp ^ sum;
			AF = (AF & ~0xff) | (sum & 0x80) |
				(((sum & 0xff) == 0) << 6) | (temp & 0x28) |
				(((cbits >> 6) ^ (cbits >> 5)) & 4) | 2 |
				(cbits & 0x10) | ((cbits >> 8) & 1);
			break;
		case 0xBD:			/* CP IYL */
			temp = lreg(IY);
			AF = (AF & ~0x28) | (temp & 0x28);
			acu = hreg(AF);
			sum = acu - temp;
			cbits = acu ^ temp ^ sum;
			AF = (AF & ~0xff) | (sum & 0x80) |
				(((sum & 0xff) == 0) << 6) | (temp & 0x28) |
				(((cbits >> 6) ^ (cbits >> 5)) & 4) | 2 |
				(cbits & 0x10) | ((cbits >> 8) & 1);
			break;
		case 0xBE:			/* CP (IY+dd) */
			adr = IY + (signed char) GetBYTE_pp(PC);
			temp = GetBYTE(adr);
			AF = (AF & ~0x28) | (temp & 0x28);
			acu = hreg(AF);
			sum = acu - temp;
			cbits = acu ^ temp ^ sum;
			AF = (AF & ~0xff) | (sum & 0x80) |
				(((sum & 0xff) == 0) << 6) | (temp & 0x28) |
				(((cbits >> 6) ^ (cbits >> 5)) & 4) | 2 |
				(cbits & 0x10) | ((cbits >> 8) & 1);
			break;
		case 0xCB:			/* CB prefix */
			adr = IY + (signed char) GetBYTE_pp(PC);
			switch ((op = GetBYTE(PC)) & 7) {
			    /*
			     * default: supresses compiler warning: "warning:
			     * 'acu' may be used uninitialized in this function"
			     */
			default:
			case 0: ++PC; acu = hreg(BC); break;
			case 1: ++PC; acu = lreg(BC); break;
			case 2: ++PC; acu = hreg(DE); break;
			case 3: ++PC; acu = lreg(DE); break;
			case 4: ++PC; acu = hreg(HL); break;
			case 5: ++PC; acu = lreg(HL); break;
			case 6: ++PC; acu = GetBYTE(adr);  break;
			case 7: ++PC; acu = hreg(AF); break;
			}
			switch (op & 0xc0) {
			/*
			 * Use default to supress compiler warning: "warning:
			 * 'temp' may be used uninitialized in this function"
			 */
			default:
			case 0x00:		/* shift/rotate */
				switch (op & 0x38) {
				/*
				 * Use default to supress compiler warning:
				 * "warning: 'temp' may be used uninitialized
				 * in this function"
				 */
				default:
				case 0x00:	/* RLC */
					temp = (acu << 1) | (acu >> 7);
					cbits = temp & 1;
					goto cbshflg3;
				case 0x08:	/* RRC */
					temp = (acu >> 1) | (acu << 7);
					cbits = temp & 0x80;
					goto cbshflg3;
				case 0x10:	/* RL */
					temp = (acu << 1) | TSTFLAG(C);
					cbits = acu & 0x80;
					goto cbshflg3;
				case 0x18:	/* RR */
					temp = (acu >> 1) | (TSTFLAG(C) << 7);
					cbits = acu & 1;
					goto cbshflg3;
				case 0x20:	/* SLA */
					temp = acu << 1;
					cbits = acu & 0x80;
					goto cbshflg3;
				case 0x28:	/* SRA */
					temp = (acu >> 1) | (acu & 0x80);
					cbits = acu & 1;
					goto cbshflg3;
				case 0x30:	/* SLIA */
					temp = (acu << 1) | 1;
					cbits = acu & 0x80;
					goto cbshflg3;
				case 0x38:	/* SRL */
					temp = acu >> 1;
					cbits = acu & 1;
				cbshflg3:
					AF = (AF & ~0xff) | (temp & 0xa8) |
						(((temp & 0xff) == 0) << 6) |
						parity(temp) | !!cbits;
				}
				break;
			case 0x40:		/* BIT */
				if (acu & (1 << ((op >> 3) & 7)))
					AF = (AF & ~0xfe) | 0x10 |
					(((op & 0x38) == 0x38) << 7);
				else
					AF = (AF & ~0xfe) | 0x54;
				if ((op&7) != 6)
					AF |= (acu & 0x28);
				temp = acu;
				break;
			case 0x80:		/* RES */
				temp = acu & ~(1 << ((op >> 3) & 7));
				break;
			case 0xc0:		/* SET */
				temp = acu | (1 << ((op >> 3) & 7));
				break;
			}
			switch (op & 7) {
			case 0: Sethreg(BC, temp); break;
			case 1: Setlreg(BC, temp); break;
			case 2: Sethreg(DE, temp); break;
			case 3: Setlreg(DE, temp); break;
			case 4: Sethreg(HL, temp); break;
			case 5: Setlreg(HL, temp); break;
			case 6: PutBYTE(adr, temp);  break;
			case 7: Sethreg(AF, temp); break;
			}
			break;
		case 0xE1:			/* POP IY */
			POP(IY);
			break;
		case 0xE3:			/* EX (SP),IY */
			temp = IY; POP(IY); PUSH(temp);
			break;
		case 0xE5:			/* PUSH IY */
			PUSH(IY);
			break;
		case 0xE9:			/* JP (IY) */
			PC = IY;
			break;
		case 0xF9:			/* LD SP,IY */
			SP = IY;
			break;
		default: PC--;		/* ignore DD */
		}
		break;
	case 0xFE:			/* CP nn */
		temp = GetBYTE_pp(PC);
		AF = (AF & ~0x28) | (temp & 0x28);
		acu = hreg(AF);
		sum = acu - temp;
		cbits = acu ^ temp ^ sum;
		AF = (AF & ~0xff) | (sum & 0x80) |
			(((sum & 0xff) == 0) << 6) | (temp & 0x28) |
			(((cbits >> 6) ^ (cbits >> 5)) & 4) | 2 |
			(cbits & 0x10) | ((cbits >> 8) & 1);
		break;
	case 0xFF:			/* RST 38H */
		PUSH(PC); PC = 0x38;
    }
    }
/* make registers visible for debugging if interrupted */
    SAVE_STATE();
    return (PC&0xffff)|0x10000;	/* flag non-bios stop */
}
