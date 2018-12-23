/*
 * Copyright 2001 by Arto Salmi and Joze Fabcic
 * Copyright 2006, 2007 by Brian Dominy <brian@oddchange.com>
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

#include "mc6809.h"
#include <stdarg.h>
#include "../tube.h"

#ifdef INCLUDE_DEBUGGER
#include "mc6809_debug.h"
#include "../cpu_debug.h"
#endif

static unsigned X, Y, S, U, PC;
static unsigned A, B, DP;
static unsigned H, N, Z, OV, C;
static unsigned EFI;

#ifdef H6309
static unsigned E, F, V, MD;

#define MD_NATIVE 0x1		/* if 1, execute in 6309 mode */
#define MD_FIRQ_LIKE_IRQ 0x2	/* if 1, FIRQ acts like IRQ */
#define MD_ILL 0x40		/* illegal instruction */
#define MD_DBZ 0x80		/* divide by zero */
#endif /* H6309 */

static unsigned iPC;

static unsigned ea = 0;
static long cpu_clk = 0;
static long cpu_period = 0;
static unsigned int irqs_pending = 0;
static unsigned int firqs_pending = 0;
static unsigned int cc_changed = 0;

static unsigned *index_regs[4] = { &X, &Y, &U, &S };

static int sync_flag;

static void irq (void);
static void firq (void);


void mc6809nc_request_irq (unsigned int source)
{
	/* If the interrupt is not masked, generate
	 * IRQ immediately.  Else, mark it pending and
	 * we'll check it later when the flags change.
	 */
//	irqs_pending |= (1 << source);
   sync_flag = 0;
	if (!(EFI & I_FLAG))
		irq ();
}

void mc6809nc_release_irq (unsigned int source)
{
	irqs_pending &= ~(1 << source);
}

void mc6809nc_request_firq (unsigned int source)
{
	/* If the interrupt is not masked, generate
	 * IRQ immediately.  Else, mark it pending and
	 * we'll check it later when the flags change.
	 */
	//firqs_pending |= (1 << source);
   sync_flag = 0;
	if (!(EFI & F_FLAG))
		firq ();
}

void mc6809nc_release_firq (unsigned int source)
{
	firqs_pending &= ~(1 << source);
}

static inline void check_pc (void)
{
	/* TODO */
}

static inline void check_stack (void)
{
	/* TODO */
}

static void sim_error (const char *format, ...)
{
	va_list ap;

	va_start (ap, format);
	printf ("m6809-run: (at PC=%04X) ", iPC);
	vprintf (format, ap);
	va_end (ap);
}

static inline void change_pc (unsigned newPC)
{
  PC = newPC & 0xffff; /* [NAC HACK 2016Oct21] stop PC from going out of range. Crude.
                          why have I not seen this problem before? Did I introduce this
                          bug as a side-effect of another change?
                       */
}

static inline unsigned imm_byte (void)
{
  unsigned val = read8 (PC);
  PC++;
  return val;
}

static inline unsigned imm_word (void)
{
  unsigned val = read16 (PC);
  PC += 2;
  return val;
}

#define WRMEM(addr, data) write8 (addr, data)

static void WRMEM16 (unsigned addr, unsigned data)
{
  WRMEM (addr, data >> 8);
  cpu_clk--;
  WRMEM ((addr + 1) & 0xffff, data & 0xff);
}

#define RDMEM(addr) read8 (addr)

static unsigned RDMEM16 (unsigned addr)
{
  unsigned val = RDMEM (addr) << 8;
  cpu_clk--;
  val |= RDMEM ((addr + 1) & 0xffff);
  return val;
}

#define write_stack WRMEM
#define read_stack  RDMEM

static void write_stack16 (unsigned addr, unsigned data)
{
  write_stack ((addr + 1) & 0xffff, data & 0xff);
  write_stack (addr, data >> 8);
}

static unsigned read_stack16 (unsigned addr)
{
  return (read_stack (addr) << 8) | read_stack ((addr + 1) & 0xffff);
}

static void direct (void)
{
  unsigned val = read8 (PC) | DP;
  PC++;
  ea = val;
}

static void indexed (void)			/* note take 1 extra cycle */
{
  unsigned post = imm_byte ();
  unsigned *R = index_regs[(post >> 5) & 0x3];

  if (post & 0x80)
    {
      switch (post & 0x1f)
	{
	case 0x00:
	  ea = *R;
	  *R = (*R + 1) & 0xffff;
	  cpu_clk -= 6;
	  break;
	case 0x01:
	  ea = *R;
	  *R = (*R + 2) & 0xffff;
	  cpu_clk -= 7;
	  break;
	case 0x02:
	  *R = (*R - 1) & 0xffff;
	  ea = *R;
	  cpu_clk -= 6;
	  break;
	case 0x03:
	  *R = (*R - 2) & 0xffff;
	  ea = *R;
	  cpu_clk -= 7;
	  break;
	case 0x04:
	  ea = *R;
	  cpu_clk -= 4;
	  break;
	case 0x05:
	  ea = (*R + ((int8_t) B)) & 0xffff;
	  cpu_clk -= 5;
	  break;
	case 0x06:
	  ea = (*R + ((int8_t) A)) & 0xffff;
	  cpu_clk -= 5;
	  break;
	case 0x08:
	  ea = (*R + ((int8_t) imm_byte ())) & 0xffff;
	  cpu_clk -= 5;
	  break;
	case 0x09:
	  ea = (*R + imm_word ()) & 0xffff;
	  cpu_clk -= 8;
	  break;
	case 0x0b:
	  ea = (*R + get_d ()) & 0xffff;
	  cpu_clk -= 8;
	  break;
	case 0x0c:
	  ea = (int8_t) imm_byte ();
	  ea = (ea + PC) & 0xffff;
	  cpu_clk -= 5;
	  break;
	case 0x0d:
	  ea = imm_word ();
	  ea = (ea + PC) & 0xffff;
	  cpu_clk -= 9;
	  break;

	case 0x11:
	  ea = *R;
	  *R = (*R + 2) & 0xffff;
	  cpu_clk -= 7;
	  ea = RDMEM16 (ea);
	  cpu_clk -= 2;
	  break;
	case 0x13:
	  *R = (*R - 2) & 0xffff;
	  ea = *R;
	  cpu_clk -= 7;
	  ea = RDMEM16 (ea);
	  cpu_clk -= 2;
	  break;
	case 0x14:
	  ea = *R;
	  cpu_clk -= 4;
	  ea = RDMEM16 (ea);
	  cpu_clk -= 2;
	  break;
	case 0x15:
	  ea = (*R + ((int8_t) B)) & 0xffff;
	  cpu_clk -= 5;
	  ea = RDMEM16 (ea);
	  cpu_clk -= 2;
	  break;
	case 0x16:
	  ea = (*R + ((int8_t) A)) & 0xffff;
	  cpu_clk -= 5;
	  ea = RDMEM16 (ea);
	  cpu_clk -= 2;
	  break;
	case 0x18:
	  ea = (*R + ((int8_t) imm_byte ())) & 0xffff;
	  cpu_clk -= 5;
	  ea = RDMEM16 (ea);
	  cpu_clk -= 2;
	  break;
	case 0x19:
	  ea = (*R + imm_word ()) & 0xffff;
	  cpu_clk -= 8;
	  ea = RDMEM16 (ea);
	  cpu_clk -= 2;
	  break;
	case 0x1b:
	  ea = (*R + get_d ()) & 0xffff;
	  cpu_clk -= 8;
	  ea = RDMEM16 (ea);
	  cpu_clk -= 2;
	  break;
	case 0x1c:
	  ea = (int8_t) imm_byte ();
	  ea = (ea + PC) & 0xffff;
	  cpu_clk -= 5;
	  ea = RDMEM16 (ea);
	  cpu_clk -= 2;
	  break;
	case 0x1d:
	  ea = imm_word ();
	  ea = (ea + PC) & 0xffff;
	  cpu_clk -= 9;
	  ea = RDMEM16 (ea);
	  cpu_clk -= 2;
	  break;
	case 0x1f:
	  ea = imm_word ();
	  cpu_clk -= 6;
	  ea = RDMEM16 (ea);
	  cpu_clk -= 2;
	  break;
	default:
	  ea = 0;
	  sim_error ("invalid index post $%02X\n", post);
	  break;
	}
    }
  else
    {
      if (post & 0x10)
	post |= 0xfff0;
      else
	post &= 0x000f;
      ea = (*R + post) & 0xffff;
      cpu_clk -= 5;
    }
}

static void extended (void)
{
  unsigned val = read16 (PC);
  PC += 2;
  ea = val;
}

/* external register functions */

unsigned get_a (void)
{
  return A;
}

unsigned get_b (void)
{
  return B;
}

unsigned get_dp (void)
{
  return DP >> 8;
}

unsigned get_x (void)
{
  return X;
}

unsigned get_y (void)
{
  return Y;
}

unsigned get_s (void)
{
  return S;
}

unsigned get_u (void)
{
  return U;
}

unsigned get_pc (void)
{
  return PC & 0xffff;
}

unsigned get_d (void)
{
  return (A << 8) | B;
}

unsigned get_flags (void)
{
  return EFI;
}

#ifdef H6309
unsigned get_e (void)
{
  return E;
}

unsigned get_f (void)
{
  return F;
}

unsigned get_w (void)
{
  return (E << 8) | F;
}

unsigned get_q (void)
{
  return (get_w () << 16) | get_d ();
}

unsigned get_v (void)
{
  return V;
}

unsigned get_zero (void)
{
  return 0;
}

unsigned get_md (void)
{
  return MD;
}
#endif

void set_a (unsigned val)
{
  A = val & 0xff;
}

void set_b (unsigned val)
{
  B = val & 0xff;
}

void set_dp (unsigned val)
{
  DP = (val & 0xff) << 8;
}

void set_x (unsigned val)
{
  X = val & 0xffff;
}

void set_y (unsigned val)
{
  Y = val & 0xffff;
}

void set_s (unsigned val)
{
  S = val & 0xffff;
  check_stack ();
}

void set_u (unsigned val)
{
  U = val & 0xffff;
}

void set_pc (unsigned val)
{
  PC = val & 0xffff;
  check_pc ();
}

void set_d (unsigned val)
{
  A = (val >> 8) & 0xff;
  B = val & 0xff;
}

#ifdef H6309
void set_e (unsigned val)
{
  E = val & 0xff;
}

void set_f (unsigned val)
{
  F = val & 0xff;
}

void set_w (unsigned val)
{
  E = (val >> 8) & 0xff;
  F = val & 0xff;
}

void set_q (unsigned val)
{
  set_w ((val >> 16) & 0xffff);
  set_d (val & 0xffff);
}

void set_v (unsigned val)
{
  V = val & 0xff;
}

void set_zero (unsigned val)
{
}

void set_md (unsigned val)
{
  MD = val & 0xff;
}
#endif

/* handle condition code register */

unsigned get_cc (void)
{
  unsigned res = EFI & (E_FLAG | F_FLAG | I_FLAG);

  if (H & 0x10)
    res |= H_FLAG;
  if (N & 0x80)
    res |= N_FLAG;
  if (Z == 0)
    res |= Z_FLAG;
  if (OV & 0x80)
    res |= V_FLAG;
  if (C != 0)
    res |= C_FLAG;

  return res;
}

void set_cc (unsigned arg)
{
  EFI = arg & (E_FLAG | F_FLAG | I_FLAG);
  H = ((arg & H_FLAG )? 0x10 : 0);
  N = ((arg & N_FLAG )? 0x80 : 0);
  Z = (~arg) & Z_FLAG;
  OV = ((arg & V_FLAG) ? 0x80 : 0);
  C = arg & C_FLAG;
  cc_changed = 1;
}

void cc_modified (void)
{
  /* Check for pending interrupts */
	if (firqs_pending && !(EFI & F_FLAG))
		firq ();
	else if (irqs_pending && !(EFI & I_FLAG))
		irq ();
	cc_changed = 0;
}

unsigned get_reg (unsigned nro)
{
  unsigned val = 0xff;

  switch (nro)
    {
    case 0:
      val = (A << 8) | B;
      break;
    case 1:
      val = X;
      break;
    case 2:
      val = Y;
      break;
    case 3:
      val = U;
      break;
    case 4:
      val = S;
      break;
    case 5:
      val = PC & 0xffff;
      break;
#ifdef H6309
    case 6:
      val = (E << 8) | F;
      break;
    case 7:
      val = V;
      break;
#endif
    case 8:
      val = A;
      break;
    case 9:
      val = B;
      break;
    case 10:
      val = get_cc ();
      break;
    case 11:
      val = DP >> 8;
      break;
#ifdef H6309
    case 14:
      val = E;
      break;
    case 15:
      val = F;
      break;
#endif
    }

  return val;
}

void set_reg (unsigned nro, unsigned val)
{
  switch (nro)
    {
    case 0:
      A = val >> 8;
      B = val & 0xff;
      break;
    case 1:
      X = val;
      break;
    case 2:
      Y = val;
      break;
    case 3:
      U = val;
      break;
    case 4:
      S = val;
      break;
    case 5:
      PC = val;
      check_pc ();
      break;
#ifdef H6309
    case 6:
      E = val >> 8;
      F = val & 0xff;
      break;
    case 7:
      V = val;
      break;
#endif
    case 8:
      A = val;
      break;
    case 9:
      B = val;
      break;
    case 10:
      set_cc (val);
      break;
    case 11:
      DP = val << 8;
      break;
#ifdef H6309
    case 14:
      E = val;
      break;
    case 15:
      F = val;
      break;
#endif
    }
}

/* 8-Bit Accumulator and Memory Instructions */

static unsigned adc (unsigned arg, unsigned val)
{
  unsigned res = arg + val + (C != 0);

  C = (res >> 1) & 0x80;
  N = Z = res &= 0xff;
  OV = H = arg ^ val ^ res ^ C;

  return res;
}

static unsigned add (unsigned arg, unsigned val)
{
  unsigned res = arg + val;

  C = (res >> 1) & 0x80;
  N = Z = res &= 0xff;
  OV = H = arg ^ val ^ res ^ C;

  return res;
}

static unsigned and (unsigned arg, unsigned val)
{
  unsigned res = arg & val;

  N = Z = res;
  OV = 0;

  return res;
}

static unsigned asl (unsigned arg)		/* same as lsl */
{
  unsigned res = arg << 1;

  C = res & 0x100;
  N = Z = res &= 0xff;
  OV = arg ^ res;
  cpu_clk -= 2;

  return res;
}

static unsigned asr (unsigned arg)
{
  unsigned res = (int8_t) arg;

  C = res & 1;
  N = Z = res = (res >> 1) & 0xff;
  cpu_clk -= 2;

  return res;
}

static void bit (unsigned arg, unsigned val)
{
  unsigned res = arg & val;

  N = Z = res;
  OV = 0;
}

static unsigned clr (unsigned arg)
{
  C = N = Z = OV = arg = 0;
  cpu_clk -= 2;

  return arg;
}

static void cmp (unsigned arg, unsigned val)
{
  unsigned res = arg - val;

  C = res & 0x100;
  N = Z = res &= 0xff;
  OV = (arg ^ val) & (arg ^ res);
}

static unsigned com (unsigned arg)
{
  unsigned res = arg ^ 0xff;

  N = Z = res;
  OV = 0;
  C = 1;
  cpu_clk -= 2;

  return res;
}

static void daa (void)
{
  unsigned res = A;
  unsigned msn = res & 0xf0;
  unsigned lsn = res & 0x0f;

  if (lsn > 0x09 || (H & 0x10))
    res += 0x06;
  if (msn > 0x80 && lsn > 0x09)
    res += 0x60;
  if (msn > 0x90 || (C != 0))
    res += 0x60;

  C |= (res & 0x100);
  A = N = Z = (res & 0xff);
  OV = 0;			/* fix this */

  cpu_clk -= 2;
}

static unsigned dec (unsigned arg)
{
  unsigned res = (arg - 1) & 0xff;

  N = Z = res;
  OV = arg & ~res;
  cpu_clk -= 2;

  return res;
}

unsigned eor (unsigned arg, unsigned val)
{
  unsigned res = arg ^ val;

  N = Z = res;
  OV = 0;

  return res;
}

static void exg (void)
{
  unsigned tmp1 = 0xff;
  unsigned tmp2 = 0xff;
  unsigned post = imm_byte ();

  if (((post ^ (post << 4)) & 0x80) == 0)
    {
      tmp1 = get_reg (post >> 4);
      tmp2 = get_reg (post & 15);
    }

  set_reg (post & 15, tmp1);
  set_reg (post >> 4, tmp2);

  cpu_clk -= 8;
}

static unsigned inc (unsigned arg)
{
  unsigned res = (arg + 1) & 0xff;

  N = Z = res;
  OV = ~arg & res;
  cpu_clk -= 2;

  return res;
}

static unsigned ld (unsigned arg)
{
  unsigned res = arg;

  N = Z = res;
  OV = 0;

  return res;
}

static unsigned lsr (unsigned arg)
{
  unsigned res = arg >> 1;

  N = 0;
  Z = res;
  C = arg & 1;
  cpu_clk -= 2;

  return res;
}

static void mul (void)
{
  unsigned res = (A * B) & 0xffff;

  Z = res;
  C = res & 0x80;
  A = res >> 8;
  B = res & 0xff;
  cpu_clk -= 11;
}

static unsigned neg (int arg)
{
  unsigned res = (-arg) & 0xff;

  C = N = Z = res;
  OV = res & arg;
  cpu_clk -= 2;

  return res;
}

static unsigned or (unsigned arg, unsigned val)
{
  unsigned res = arg | val;

  N = Z = res;
  OV = 0;

  return res;
}

static unsigned rol (unsigned arg)
{
  unsigned res = (arg << 1) + (C != 0);

  C = res & 0x100;
  N = Z = res &= 0xff;
  OV = arg ^ res;
  cpu_clk -= 2;

  return res;
}

static unsigned ror (unsigned arg)
{
  unsigned res = arg;

  if (C != 0)
    res |= 0x100;
  C = res & 1;
  N = Z = res >>= 1;
  cpu_clk -= 2;

  return res;
}

static unsigned sbc (unsigned arg, unsigned val)
{
  unsigned res = arg - val - (C != 0);

  C = res & 0x100;
  N = Z = res &= 0xff;
  OV = (arg ^ val) & (arg ^ res);

  return res;
}

static void st (unsigned arg)
{
  unsigned res = arg;

  N = Z = res;
  OV = 0;

  WRMEM (ea, res);
}

static unsigned sub (unsigned arg, unsigned val)
{
  unsigned res = arg - val;

  C = res & 0x100;
  N = Z = res &= 0xff;
  OV = (arg ^ val) & (arg ^ res);

  return res;
}

static void tst (unsigned arg)
{
  unsigned res = arg;

  N = Z = res;
  OV = 0;
  cpu_clk -= 2;
}

static void tfr (void)
{
  unsigned tmp1 = 0xff;
  unsigned post = imm_byte ();

  if (((post ^ (post << 4)) & 0x80) == 0)
    tmp1 = get_reg (post >> 4);

  set_reg (post & 15, tmp1);

  cpu_clk -= 6;
}

/* 16-Bit Accumulator Instructions */

static void abx (void)
{
  X = (X + B) & 0xffff;
  cpu_clk -= 3;
}

static void addd (unsigned val)
{
  unsigned arg = (A << 8) | B;
  unsigned res = arg + val;

  C = res & 0x10000;
  Z = res &= 0xffff;
  OV = ((arg ^ res) & (val ^ res)) >> 8;
  A = N = res >> 8;
  B = res & 0xff;
}

static void cmp16 (unsigned arg, unsigned val)
{
  unsigned res = arg - val;

  C = res & 0x10000;
  Z = res &= 0xffff;
  N = res >> 8;
  OV = ((arg ^ val) & (arg ^ res)) >> 8;
}

static void ldd (unsigned arg)
{
  unsigned res = arg;

  Z = res;
  A = N = res >> 8;
  B = res & 0xff;
  OV = 0;
}

static unsigned ld16 (unsigned arg)
{
  unsigned res = arg;

  Z = res;
  N = res >> 8;
  OV = 0;

  return res;
}

static void sex (void)
{
  unsigned res = B;

  Z = res;
  N = res &= 0x80;
  if (res != 0)
    res = 0xff;
  A = res;
  cpu_clk -= 2;
}

static void std (void)
{
  unsigned res = (A << 8) | B;

  Z = res;
  N = A;
  OV = 0;
  WRMEM16 (ea, res);
}

static void st16 (unsigned arg)
{
  unsigned res = arg;

  Z = res;
  N = res >> 8;
  OV = 0;
  WRMEM16 (ea, res);
}

static void subd (unsigned val)
{
  unsigned arg = (A << 8) | B;
  unsigned res = arg - val;

  C = res & 0x10000;
  Z = res &= 0xffff;
  OV = ((arg ^ val) & (arg ^ res)) >> 8;
  A = N = res >> 8;
  B = res & 0xff;
}

/* stack instructions */

static void pshs (void)
{
  unsigned post = imm_byte ();

  cpu_clk -= 5;

  if (post & 0x80)
    {
      cpu_clk -= 2;
      S = (S - 2) & 0xffff;
      write_stack16 (S, PC & 0xffff);
    }
  if (post & 0x40)
    {
      cpu_clk -= 2;
      S = (S - 2) & 0xffff;
      write_stack16 (S, U);
    }
  if (post & 0x20)
    {
      cpu_clk -= 2;
      S = (S - 2) & 0xffff;
      write_stack16 (S, Y);
    }
  if (post & 0x10)
    {
      cpu_clk -= 2;
      S = (S - 2) & 0xffff;
      write_stack16 (S, X);
    }
  if (post & 0x08)
    {
      cpu_clk -= 1;
      S = (S - 1) & 0xffff;
      write_stack (S, DP >> 8);
    }
  if (post & 0x04)
    {
      cpu_clk -= 1;
      S = (S - 1) & 0xffff;
      write_stack (S, B);
    }
  if (post & 0x02)
    {
      cpu_clk -= 1;
      S = (S - 1) & 0xffff;
      write_stack (S, A);
    }
  if (post & 0x01)
    {
      cpu_clk -= 1;
      S = (S - 1) & 0xffff;
      write_stack (S, get_cc ());
    }
}

static void pshu (void)
{
  unsigned post = imm_byte ();

  cpu_clk -= 5;

  if (post & 0x80)
    {
      cpu_clk -= 2;
      U = (U - 2) & 0xffff;
      write_stack16 (U, PC & 0xffff);
    }
  if (post & 0x40)
    {
      cpu_clk -= 2;
      U = (U - 2) & 0xffff;
      write_stack16 (U, S);
    }
  if (post & 0x20)
    {
      cpu_clk -= 2;
      U = (U - 2) & 0xffff;
      write_stack16 (U, Y);
    }
  if (post & 0x10)
    {
      cpu_clk -= 2;
      U = (U - 2) & 0xffff;
      write_stack16 (U, X);
    }
  if (post & 0x08)
    {
      cpu_clk -= 1;
      U = (U - 1) & 0xffff;
      write_stack (U, DP >> 8);
    }
  if (post & 0x04)
    {
      cpu_clk -= 1;
      U = (U - 1) & 0xffff;
      write_stack (U, B);
    }
  if (post & 0x02)
    {
      cpu_clk -= 1;
      U = (U - 1) & 0xffff;
      write_stack (U, A);
    }
  if (post & 0x01)
    {
      cpu_clk -= 1;
      U = (U - 1) & 0xffff;
      write_stack (U, get_cc ());
    }
}

static void puls (void)
{
  unsigned post = imm_byte ();

  cpu_clk -= 5;

  if (post & 0x01)
    {
      cpu_clk -= 1;
      set_cc (read_stack (S));
      S = (S + 1) & 0xffff;
    }
  if (post & 0x02)
    {
      cpu_clk -= 1;
      A = read_stack (S);
      S = (S + 1) & 0xffff;
    }
  if (post & 0x04)
    {
      cpu_clk -= 1;
      B = read_stack (S);
      S = (S + 1) & 0xffff;
    }
  if (post & 0x08)
    {
      cpu_clk -= 1;
      DP = read_stack (S) << 8;
      S = (S + 1) & 0xffff;
    }
  if (post & 0x10)
    {
      cpu_clk -= 2;
      X = read_stack16 (S);
      S = (S + 2) & 0xffff;
    }
  if (post & 0x20)
    {
      cpu_clk -= 2;
      Y = read_stack16 (S);
      S = (S + 2) & 0xffff;
    }
  if (post & 0x40)
    {
      cpu_clk -= 2;
      U = read_stack16 (S);
      S = (S + 2) & 0xffff;
    }
  if (post & 0x80)
    {
      cpu_clk -= 2;
      PC = read_stack16 (S);
		check_pc ();
      S = (S + 2) & 0xffff;
    }
}

static void pulu (void)
{
  unsigned post = imm_byte ();

  cpu_clk -= 5;

  if (post & 0x01)
    {
      cpu_clk -= 1;
      set_cc (read_stack (U));
      U = (U + 1) & 0xffff;
    }
  if (post & 0x02)
    {
      cpu_clk -= 1;
      A = read_stack (U);
      U = (U + 1) & 0xffff;
    }
  if (post & 0x04)
    {
      cpu_clk -= 1;
      B = read_stack (U);
      U = (U + 1) & 0xffff;
    }
  if (post & 0x08)
    {
      cpu_clk -= 1;
      DP = read_stack (U) << 8;
      U = (U + 1) & 0xffff;
    }
  if (post & 0x10)
    {
      cpu_clk -= 2;
      X = read_stack16 (U);
      U = (U + 2) & 0xffff;
    }
  if (post & 0x20)
    {
      cpu_clk -= 2;
      Y = read_stack16 (U);
      U = (U + 2) & 0xffff;
    }
  if (post & 0x40)
    {
      cpu_clk -= 2;
      S = read_stack16 (U);
      U = (U + 2) & 0xffff;
    }
  if (post & 0x80)
    {
      cpu_clk -= 2;
      PC = read_stack16 (U);
		check_pc ();
      U = (U + 2) & 0xffff;
    }
}

/* Miscellaneous Instructions */

static void nop (void)
{
  cpu_clk -= 2;
}

static void jsr (void)
{
  S = (S - 2) & 0xffff;
  write_stack16 (S, PC & 0xffff);
  change_pc (ea);
}

static void rti (void)
{
  cpu_clk -= 6;
  set_cc (read_stack (S));
  S = (S + 1) & 0xffff;

  if ((EFI & E_FLAG) != 0)
    {
      cpu_clk -= 9;
      A = read_stack (S);
      S = (S + 1) & 0xffff;
      B = read_stack (S);
      S = (S + 1) & 0xffff;
      DP = read_stack (S) << 8;
      S = (S + 1) & 0xffff;
      X = read_stack16 (S);
      S = (S + 2) & 0xffff;
      Y = read_stack16 (S);
      S = (S + 2) & 0xffff;
      U = read_stack16 (S);
      S = (S + 2) & 0xffff;
    }
  PC = read_stack16 (S);
  check_pc ();
  S = (S + 2) & 0xffff;
}

static void rts (void)
{
  cpu_clk -= 5;
  PC = read_stack16 (S);
  check_pc ();
  S = (S + 2) & 0xffff;
}

static void irq (void)
{
  EFI |= E_FLAG;
  S = (S - 2) & 0xffff;
  write_stack16 (S, PC & 0xffff);
  S = (S - 2) & 0xffff;
  write_stack16 (S, U);
  S = (S - 2) & 0xffff;
  write_stack16 (S, Y);
  S = (S - 2) & 0xffff;
  write_stack16 (S, X);
  S = (S - 1) & 0xffff;
  write_stack (S, DP >> 8);
  S = (S - 1) & 0xffff;
  write_stack (S, B);
  S = (S - 1) & 0xffff;
  write_stack (S, A);
  S = (S - 1) & 0xffff;
  write_stack (S, get_cc ());
  EFI |= I_FLAG;

  change_pc (read16 (0xfef8));
#if 1
  irqs_pending = 0;
#endif
}

static void firq (void)
{
  EFI &= ~E_FLAG;
  S = (S - 2) & 0xffff;
  write_stack16 (S, PC & 0xffff);
  S = (S - 1) & 0xffff;
  write_stack (S, get_cc ());
  EFI |= (I_FLAG | F_FLAG);

  change_pc (read16 (0xfef6));
#if 1
  firqs_pending = 0;
#endif
}

static void swi (void)
{
  cpu_clk -= 19;
  EFI |= E_FLAG;
  S = (S - 2) & 0xffff;
  write_stack16 (S, PC & 0xffff);
  S = (S - 2) & 0xffff;
  write_stack16 (S, U);
  S = (S - 2) & 0xffff;
  write_stack16 (S, Y);
  S = (S - 2) & 0xffff;
  write_stack16 (S, X);
  S = (S - 1) & 0xffff;
  write_stack (S, DP >> 8);
  S = (S - 1) & 0xffff;
  write_stack (S, B);
  S = (S - 1) & 0xffff;
  write_stack (S, A);
  S = (S - 1) & 0xffff;
  write_stack (S, get_cc ());
  EFI |= (I_FLAG | F_FLAG);

  change_pc (read16 (0xfefa));
}

static void swi2 (void)
{
  cpu_clk -= 20;
  EFI |= E_FLAG;
  S = (S - 2) & 0xffff;
  write_stack16 (S, PC & 0xffff);
  S = (S - 2) & 0xffff;
  write_stack16 (S, U);
  S = (S - 2) & 0xffff;
  write_stack16 (S, Y);
  S = (S - 2) & 0xffff;
  write_stack16 (S, X);
  S = (S - 1) & 0xffff;
  write_stack (S, DP >> 8);
  S = (S - 1) & 0xffff;
  write_stack (S, B);
  S = (S - 1) & 0xffff;
  write_stack (S, A);
  S = (S - 1) & 0xffff;
  write_stack (S, get_cc ());

  change_pc (read16 (0xfef4));
}

static void swi3 (void)
{
  cpu_clk -= 20;
  EFI |= E_FLAG;
  S = (S - 2) & 0xffff;
  write_stack16 (S, PC & 0xffff);
  S = (S - 2) & 0xffff;
  write_stack16 (S, U);
  S = (S - 2) & 0xffff;
  write_stack16 (S, Y);
  S = (S - 2) & 0xffff;
  write_stack16 (S, X);
  S = (S - 1) & 0xffff;
  write_stack (S, DP >> 8);
  S = (S - 1) & 0xffff;
  write_stack (S, B);
  S = (S - 1) & 0xffff;
  write_stack (S, A);
  S = (S - 1) & 0xffff;
  write_stack (S, get_cc ());

  change_pc (read16 (0xfef2));
}

#ifdef H6309
static void trap (void)
{
  cpu_clk -= 20;
  EFI |= E_FLAG;
  S = (S - 2) & 0xffff;
  write_stack16 (S, PC & 0xffff);
  S = (S - 2) & 0xffff;
  write_stack16 (S, U);
  S = (S - 2) & 0xffff;
  write_stack16 (S, Y);
  S = (S - 2) & 0xffff;
  write_stack16 (S, X);
  S = (S - 1) & 0xffff;
  write_stack (S, DP >> 8);
  S = (S - 1) & 0xffff;
  write_stack (S, B);
  S = (S - 1) & 0xffff;
  write_stack (S, A);
  S = (S - 1) & 0xffff;
  write_stack (S, get_cc ());

  change_pc (read16 (0xfef0));
}
#endif

static void cwai (void)
{
  sim_error ("CWAI - not supported yet!");
}

static void sync (void)
{
  sync_flag = 1; 
  do {
    cpu_clk -= 4;
    tubeUseCycles(0xFFFF);
  } while (tubeContinueRunning());
}

static void orcc (void)
{
  unsigned tmp = imm_byte ();

  set_cc (get_cc () | tmp);
  cpu_clk -= 3;
}

static void andcc (void)
{
  unsigned tmp = imm_byte ();

  set_cc (get_cc () & tmp);
  cpu_clk -= 3;
}

/* Branch Instructions */

#define cond_HI() ((Z != 0) && (C == 0))
#define cond_LS() ((Z == 0) || (C != 0))
#define cond_HS() (C == 0)
#define cond_LO() (C != 0)
#define cond_NE() (Z != 0)
#define cond_EQ() (Z == 0)
#define cond_VC() ((OV & 0x80) == 0)
#define cond_VS() ((OV & 0x80) != 0)
#define cond_PL() ((N & 0x80) == 0)
#define cond_MI() ((N & 0x80) != 0)
#define cond_GE() (((N^OV) & 0x80) == 0)
#define cond_LT() (((N^OV) & 0x80) != 0)
#define cond_GT() ((((N^OV) & 0x80) == 0) && (Z != 0))
#define cond_LE() ((((N^OV) & 0x80) != 0) || (Z == 0))

static void bra (void)
{
  int8_t tmp = (int8_t) imm_byte ();
  change_pc (PC + tmp);
}

static void branch (unsigned cond)
{
  if (cond)
    bra ();
  else
    change_pc (PC+1);

  cpu_clk -= 3;
}

static void long_bra (void)
{
  int16_t tmp = (int16_t) imm_word ();
  change_pc (PC + tmp);
}

static void long_branch (unsigned cond)
{
  if (cond)
    {
      long_bra ();
      cpu_clk -= 6;
    }
  else
    {
      change_pc (PC + 2);
      cpu_clk -= 5;
    }
}

static void long_bsr (void)
{
  int16_t tmp = (int16_t) imm_word ();
  ea = PC + tmp;
  S = (S - 2) & 0xffff;
  write_stack16 (S, PC & 0xffff);
  cpu_clk -= 9;
  change_pc (ea);
}

static void bsr (void)
{
  int8_t tmp = (int8_t) imm_byte ();
  ea = PC + tmp;
  S = (S - 2) & 0xffff;
  write_stack16 (S, PC & 0xffff);
  cpu_clk -= 7;
  change_pc (ea);
}

/* Execute 6809 code for a certain number of cycles. */
int mc6809nc_execute (int tube_cycles)
{
  unsigned opcode;

  cpu_period = cpu_clk = tube_cycles;

  if (sync_flag) {
     return cpu_period;
  }

  do
    {

      iPC = PC;

#ifdef INCLUDE_DEBUGGER
      if (mc6809nc_debug_enabled)
      {
         debug_preexec(&mc6809nc_cpu_debug, PC);
      }
#endif

      opcode = imm_byte ();

      switch (opcode)
	{
	case 0x00:
	  direct ();
	  cpu_clk -= 4;
	  WRMEM (ea, neg (RDMEM (ea)));
	  break;		/* NEG direct */
#ifdef H6309
	case 0x01:		/* OIM */
	  break;
	case 0x02:		/* AIM */
	  break;
#endif
	case 0x03:
	  direct ();
	  cpu_clk -= 4;
	  WRMEM (ea, com (RDMEM (ea)));
	  break;		/* COM direct */
	case 0x04:
	  direct ();
	  cpu_clk -= 4;
	  WRMEM (ea, lsr (RDMEM (ea)));
	  break;		/* LSR direct */
#ifdef H6309
	case 0x05:		/* EIM */
	  break;
#endif
	case 0x06:
	  direct ();
	  cpu_clk -= 4;
	  WRMEM (ea, ror (RDMEM (ea)));
	  break;		/* ROR direct */
	case 0x07:
	  direct ();
	  cpu_clk -= 4;
	  WRMEM (ea, asr (RDMEM (ea)));
	  break;		/* ASR direct */
	case 0x08:
	  direct ();
	  cpu_clk -= 4;
	  WRMEM (ea, asl (RDMEM (ea)));
	  break;		/* ASL direct */
	case 0x09:
	  direct ();
	  cpu_clk -= 4;
	  WRMEM (ea, rol (RDMEM (ea)));
	  break;		/* ROL direct */
	case 0x0a:
	  direct ();
	  cpu_clk -= 4;
	  WRMEM (ea, dec (RDMEM (ea)));
	  break;		/* DEC direct */
#ifdef H6309
	case 0x0B:		/* TIM */
	  break;
#endif
	case 0x0c:
	  direct ();
	  cpu_clk -= 4;
	  WRMEM (ea, inc (RDMEM (ea)));
	  break;		/* INC direct */
	case 0x0d:
	  direct ();
	  cpu_clk -= 4;
	  tst (RDMEM (ea));
	  break;		/* TST direct */
	case 0x0e:
	  direct ();
	  cpu_clk -= 3;
	  PC = ea;
     check_pc ();
	  break;		/* JMP direct */
	case 0x0f:
	  direct ();
	  cpu_clk -= 4;
	  WRMEM (ea, clr (RDMEM (ea)));
	  break;		/* CLR direct */
	case 0x10:
	  {
	    opcode = imm_byte ();

	    switch (opcode)
	      {
	      case 0x21:
		cpu_clk -= 5;
		PC += 2;
		break;
	      case 0x22:
		long_branch (cond_HI ());
		break;
	      case 0x23:
		long_branch (cond_LS ());
		break;
	      case 0x24:
		long_branch (cond_HS ());
		break;
	      case 0x25:
		long_branch (cond_LO ());
		break;
	      case 0x26:
		long_branch (cond_NE ());
		break;
	      case 0x27:
		long_branch (cond_EQ ());
		break;
	      case 0x28:
		long_branch (cond_VC ());
		break;
	      case 0x29:
		long_branch (cond_VS ());
		break;
	      case 0x2a:
		long_branch (cond_PL ());
		break;
	      case 0x2b:
		long_branch (cond_MI ());
		break;
	      case 0x2c:
		long_branch (cond_GE ());
		break;
	      case 0x2d:
		long_branch (cond_LT ());
		break;
	      case 0x2e:
		long_branch (cond_GT ());
		break;
	      case 0x2f:
		long_branch (cond_LE ());
		break;
#ifdef H6309
	      case 0x30:	/* ADDR */
		break;
	      case 0x31:	/* ADCR */
		break;
	      case 0x32:	/* SUBR */
		break;
	      case 0x33:	/* SBCR */
		break;
	      case 0x34:	/* ANDR */
		break;
	      case 0x35:	/* ORR */
		break;
	      case 0x36:	/* EORR */
		break;
	      case 0x37:	/* CMPR */
		break;
	      case 0x38:	/* PSHSW */
		break;
	      case 0x39:	/* PULSW */
		break;
	      case 0x3a:	/* PSHUW */
		break;
	      case 0x3b:	/* PULUW */
		break;
#endif
	      case 0x3f:
		swi2 ();
		break;
#ifdef H6309
	      case 0x40:	/* NEGD */
		break;
	      case 0x43:	/* COMD */
		break;
	      case 0x44:	/* LSRD */
		break;
	      case 0x46:	/* RORD */
		break;
	      case 0x47:	/* ASRD */
		break;
	      case 0x48:	/* ASLD/LSLD */
		break;
	      case 0x49:	/* ROLD */
		break;
	      case 0x4a:	/* DECD */
		break;
	      case 0x4c:	/* INCD */
		break;
	      case 0x4d:	/* TSTD */
		break;
	      case 0x4f:	/* CLRD */
		break;
	      case 0x53:	/* COMW */
		break;
	      case 0x54:	/* LSRW */
		break;
	      case 0x56:	/* ??RORW */
		break;
	      case 0x59:	/* ROLW */
		break;
	      case 0x5a:	/* DECW */
		break;
	      case 0x5c:	/* INCW */
		break;
	      case 0x5d:	/* TSTW */
		break;
	      case 0x5f:	/* CLRW */
		break;
	      case 0x80:	/* SUBW */
		break;
	      case 0x81:	/* CMPW */
		break;
	      case 0x82:	/* SBCD */
		break;
#endif
	      case 0x83:
		cpu_clk -= 5;
		cmp16 (get_d (), imm_word ());
		break;
#ifdef H6309
	      case 0x84:	/* ANDD */
		break;
	      case 0x85:	/* BITD */
		break;
	      case 0x86:	/* LDW */
		break;
	      case 0x88:	/* EORD */
		break;
	      case 0x89:	/* ADCD */
		break;
	      case 0x8a:	/* ORD */
		break;
	      case 0x8b:	/* ADDW */
		break;
#endif
	      case 0x8c:
		cpu_clk -= 5;
		cmp16 (Y, imm_word ());
		break;
	      case 0x8e:
		cpu_clk -= 4;
		Y = ld16 (imm_word ());
		break;
#ifdef H6309
	      case 0x90:	/* SUBW */
		break;
	      case 0x91:	/* CMPW */
		break;
	      case 0x92:	/* SBCD */
		break;
#endif
	      case 0x93:
		direct ();
		cpu_clk -= 5;
		cmp16 (get_d (), RDMEM16 (ea));
		cpu_clk--;
		break;
	      case 0x9c:
		direct ();
		cpu_clk -= 5;
		cmp16 (Y, RDMEM16 (ea));
		cpu_clk--;
		break;
	      case 0x9e:
		direct ();
		cpu_clk -= 5;
		Y = ld16 (RDMEM16 (ea));
		break;
	      case 0x9f:
		direct ();
		cpu_clk -= 5;
		st16 (Y);
		break;
	      case 0xa3:
		cpu_clk--;
		indexed ();
		cmp16 (get_d (), RDMEM16 (ea));
		cpu_clk--;
		break;
	      case 0xac:
		cpu_clk--;
		indexed ();
		cmp16 (Y, RDMEM16 (ea));
		cpu_clk--;
		break;
	      case 0xae:
		cpu_clk--;
		indexed ();
		Y = ld16 (RDMEM16 (ea));
		break;
	      case 0xaf:
		cpu_clk--;
		indexed ();
		st16 (Y);
		break;
	      case 0xb3:
		extended ();
		cpu_clk -= 6;
		cmp16 (get_d (), RDMEM16 (ea));
		cpu_clk--;
		break;
	      case 0xbc:
		extended ();
		cpu_clk -= 6;
		cmp16 (Y, RDMEM16 (ea));
		cpu_clk--;
		break;
	      case 0xbe:
		extended ();
		cpu_clk -= 6;
		Y = ld16 (RDMEM16 (ea));
		break;
	      case 0xbf:
		extended ();
		cpu_clk -= 6;
		st16 (Y);
		break;
	      case 0xce:
		cpu_clk -= 4;
		S = ld16 (imm_word ());
		break;
	      case 0xde:
		direct ();
		cpu_clk -= 5;
		S = ld16 (RDMEM16 (ea));
		break;
	      case 0xdf:
		direct ();
		cpu_clk -= 5;
		st16 (S);
		break;
	      case 0xee:
		cpu_clk--;
		indexed ();
		S = ld16 (RDMEM16 (ea));
		break;
	      case 0xef:
		cpu_clk--;
		indexed ();
		st16 (S);
		break;
	      case 0xfe:
		extended ();
		cpu_clk -= 6;
		S = ld16 (RDMEM16 (ea));
		break;
	      case 0xff:
		extended ();
		cpu_clk -= 6;
		st16 (S);
		break;
	      default:
	        sim_error ("invalid opcode (1) at %04x\n", iPC);
		break;
	      }
	  }
	  break;

	case 0x11:
	  {
	    opcode = imm_byte ();

	    switch (opcode)
	      {
	      case 0x3f:
		swi3 ();
		break;
#ifdef H6309
			case 0x80: /* SUBE */
			case 0x81: /* CMPE */
#endif
	      case 0x83:
		cpu_clk -= 5;
		cmp16 (U, imm_word ());
		break;
#ifdef H6309
			case 0x86: /* LDE */
			case 0x8B: /* ADDE */
#endif
	      case 0x8c:
		cpu_clk -= 5;
		cmp16 (S, imm_word ());
		break;
#ifdef H6309
			case 0x8D: /* DIVD */
			case 0x8E: /* DIVQ */
			case 0x8F: /* MULD */
			case 0x90: /* SUBE */
			case 0x91: /* CMPE */
#endif
	      case 0x93:
		direct ();
		cpu_clk -= 5;
		cmp16 (U, RDMEM16 (ea));
		cpu_clk--;
		break;
	      case 0x9c:
		direct ();
		cpu_clk -= 5;
		cmp16 (S, RDMEM16 (ea));
		cpu_clk--;
		break;
	      case 0xa3:
		cpu_clk--;
		indexed ();
		cmp16 (U, RDMEM16 (ea));
		cpu_clk--;
		break;
	      case 0xac:
		cpu_clk--;
		indexed ();
		cmp16 (S, RDMEM16 (ea));
		cpu_clk--;
		break;
	      case 0xb3:
		extended ();
		cpu_clk -= 6;
		cmp16 (U, RDMEM16 (ea));
		cpu_clk--;
		break;
	      case 0xbc:
		extended ();
		cpu_clk -= 6;
		cmp16 (S, RDMEM16 (ea));
		cpu_clk--;
		break;
	      default:
	        sim_error ("invalid opcode (2) at %04x\n", iPC);
		break;
	      }
	  }
	  break;

	case 0x12:
	  nop ();
	  break;
	case 0x13:
	  sync ();
	  break;
#ifdef H6309
	case 0x14:		/* SEXW */
	  break;
#endif
	case 0x16:
	  long_bra ();
	  cpu_clk -= 5;
	  break;
	case 0x17:
	  long_bsr ();
	  break;
	case 0x19:
	  daa ();
	  break;
	case 0x1a:
	  orcc ();
	  break;
	case 0x1c:
	  andcc ();
	  break;
	case 0x1d:
	  sex ();
	  break;
	case 0x1e:
	  exg ();
	  break;
	case 0x1f:
	  tfr ();
	  break;

	case 0x20:
	  bra ();
	  cpu_clk -= 3;
	  break;
	case 0x21:
	  PC++;
	  cpu_clk -= 3;
	  break;
	case 0x22:
	  branch (cond_HI ());
	  break;
	case 0x23:
	  branch (cond_LS ());
	  break;
	case 0x24:
	  branch (cond_HS ());
	  break;
	case 0x25:
	  branch (cond_LO ());
	  break;
	case 0x26:
	  branch (cond_NE ());
	  break;
	case 0x27:
	  branch (cond_EQ ());
	  break;
	case 0x28:
	  branch (cond_VC ());
	  break;
	case 0x29:
	  branch (cond_VS ());
	  break;
	case 0x2a:
	  branch (cond_PL ());
	  break;
	case 0x2b:
	  branch (cond_MI ());
	  break;
	case 0x2c:
	  branch (cond_GE ());
	  break;
	case 0x2d:
	  branch (cond_LT ());
	  break;
	case 0x2e:
	  branch (cond_GT ());
	  break;
	case 0x2f:
	  branch (cond_LE ());
	  break;

	case 0x30:
	  indexed ();
	  Z = X = ea;
	  break;		/* LEAX indexed */
	case 0x31:
	  indexed ();
	  Z = Y = ea;
	  break;		/* LEAY indexed */
	case 0x32:
	  indexed ();
	  S = ea;
	  break;		/* LEAS indexed */
	case 0x33:
	  indexed ();
	  U = ea;
	  break;		/* LEAU indexed */
	case 0x34:
	  pshs ();
	  break;		/* PSHS implied */
	case 0x35:
	  puls ();
	  break;		/* PULS implied */
	case 0x36:
	  pshu ();
	  break;		/* PSHU implied */
	case 0x37:
	  pulu ();
	  break;		/* PULU implied */
	case 0x39:
	  rts ();
	  break;		/* RTS implied  */
	case 0x3a:
	  abx ();
	  break;		/* ABX implied  */
	case 0x3b:
	  rti ();
	  break;		/* RTI implied  */
	case 0x3c:
	  cwai ();
	  break;		/* CWAI implied */
	case 0x3d:
	  mul ();
	  break;		/* MUL implied  */
	case 0x3f:
	  swi ();
	  break;		/* SWI implied  */

	case 0x40:
	  A = neg (A);
	  break;		/* NEGA implied */
	case 0x43:
	  A = com (A);
	  break;		/* COMA implied */
	case 0x44:
	  A = lsr (A);
	  break;		/* LSRA implied */
	case 0x46:
	  A = ror (A);
	  break;		/* RORA implied */
	case 0x47:
	  A = asr (A);
	  break;		/* ASRA implied */
	case 0x48:
	  A = asl (A);
	  break;		/* ASLA implied */
	case 0x49:
	  A = rol (A);
	  break;		/* ROLA implied */
	case 0x4a:
	  A = dec (A);
	  break;		/* DECA implied */
	case 0x4c:
	  A = inc (A);
	  break;		/* INCA implied */
	case 0x4d:
	  tst (A);
	  break;		/* TSTA implied */
	case 0x4f:
	  A = clr (A);
	  break;		/* CLRA implied */

	case 0x50:
	  B = neg (B);
	  break;		/* NEGB implied */
	case 0x53:
	  B = com (B);
	  break;		/* COMB implied */
	case 0x54:
	  B = lsr (B);
	  break;		/* LSRB implied */
	case 0x56:
	  B = ror (B);
	  break;		/* RORB implied */
	case 0x57:
	  B = asr (B);
	  break;		/* ASRB implied */
	case 0x58:
	  B = asl (B);
	  break;		/* ASLB implied */
	case 0x59:
	  B = rol (B);
	  break;		/* ROLB implied */
	case 0x5a:
	  B = dec (B);
	  break;		/* DECB implied */
	case 0x5c:
	  B = inc (B);
	  break;		/* INCB implied */
	case 0x5d:
	  tst (B);
	  break;		/* TSTB implied */
	case 0x5f:
	  B = clr (B);
	  break;		/* CLRB implied */
	case 0x60:
	  indexed ();
	  WRMEM (ea, neg (RDMEM (ea)));
	  break;		/* NEG indexed */
#ifdef H6309
	case 0x61:		/* OIM indexed */
	  break;
	case 0x62:		/* AIM indexed */
	  break;
#endif
	case 0x63:
	  indexed ();
	  WRMEM (ea, com (RDMEM (ea)));
	  break;		/* COM indexed */
	case 0x64:
	  indexed ();
	  WRMEM (ea, lsr (RDMEM (ea)));
	  break;		/* LSR indexed */
#ifdef H6309
	case 0x65:		/* EIM indexed */
	  break;
#endif
	case 0x66:
	  indexed ();
	  WRMEM (ea, ror (RDMEM (ea)));
	  break;		/* ROR indexed */
	case 0x67:
	  indexed ();
	  WRMEM (ea, asr (RDMEM (ea)));
	  break;		/* ASR indexed */
	case 0x68:
	  indexed ();
	  WRMEM (ea, asl (RDMEM (ea)));
	  break;		/* ASL indexed */
	case 0x69:
	  indexed ();
	  WRMEM (ea, rol (RDMEM (ea)));
	  break;		/* ROL indexed */
	case 0x6a:
	  indexed ();
	  WRMEM (ea, dec (RDMEM (ea)));
	  break;		/* DEC indexed */
#ifdef H6309
	case 0x6b:		/* TIM indexed */
	  break;
#endif
	case 0x6c:
	  indexed ();
	  WRMEM (ea, inc (RDMEM (ea)));
	  break;		/* INC indexed */
	case 0x6d:
	  indexed ();
	  tst (RDMEM (ea));
	  break;		/* TST indexed */
	case 0x6e:
	  indexed ();
	  cpu_clk += 1;
	  PC = ea;
	  check_pc ();
	  break;		/* JMP indexed */
	case 0x6f:
	  indexed ();
	  WRMEM (ea, clr (RDMEM (ea)));
	  break;		/* CLR indexed */
	case 0x70:
	  extended ();
	  cpu_clk -= 5;
	  WRMEM (ea, neg (RDMEM (ea)));
	  break;		/* NEG extended */
#ifdef H6309
	case 0x71:		/* OIM extended */
	  break;
	case 0x72:		/* AIM extended */
	  break;
#endif
	case 0x73:
	  extended ();
	  cpu_clk -= 5;
	  WRMEM (ea, com (RDMEM (ea)));
	  break;		/* COM extended */
	case 0x74:
	  extended ();
	  cpu_clk -= 5;
	  WRMEM (ea, lsr (RDMEM (ea)));
	  break;		/* LSR extended */
#ifdef H6309
	case 0x75:		/* EIM extended */
	  break;
#endif
	case 0x76:
	  extended ();
	  cpu_clk -= 5;
	  WRMEM (ea, ror (RDMEM (ea)));
	  break;		/* ROR extended */
	case 0x77:
	  extended ();
	  cpu_clk -= 5;
	  WRMEM (ea, asr (RDMEM (ea)));
	  break;		/* ASR extended */
	case 0x78:
	  extended ();
	  cpu_clk -= 5;
	  WRMEM (ea, asl (RDMEM (ea)));
	  break;		/* ASL extended */
	case 0x79:
	  extended ();
	  cpu_clk -= 5;
	  WRMEM (ea, rol (RDMEM (ea)));
	  break;		/* ROL extended */
	case 0x7a:
	  extended ();
	  cpu_clk -= 5;
	  WRMEM (ea, dec (RDMEM (ea)));
	  break;		/* DEC extended */
#ifdef H6309
	case 0x7b:		/* TIM indexed */
	  break;
#endif
	case 0x7c:
	  extended ();
	  cpu_clk -= 5;
	  WRMEM (ea, inc (RDMEM (ea)));
	  break;		/* INC extended */
	case 0x7d:
	  extended ();
	  cpu_clk -= 5;
	  tst (RDMEM (ea));
	  break;		/* TST extended */
	case 0x7e:
	  extended ();
	  cpu_clk -= 4;
	  PC = ea;
	  check_pc ();
	  break;		/* JMP extended */
	case 0x7f:
	  extended ();
	  cpu_clk -= 5;
	  WRMEM (ea, clr (RDMEM (ea)));
	  break;		/* CLR extended */
	case 0x80:
	  cpu_clk -= 2;
	  A = sub (A, imm_byte ());
	  break;
	case 0x81:
	  cpu_clk -= 2;
	  cmp (A, imm_byte ());
	  break;
	case 0x82:
	  cpu_clk -= 2;
	  A = sbc (A, imm_byte ());
	  break;
	case 0x83:
	  cpu_clk -= 4;
	  subd (imm_word ());
	  break;
	case 0x84:
	  cpu_clk -= 2;
	  A = and (A, imm_byte ());
	  break;
	case 0x85:
	  cpu_clk -= 2;
	  bit (A, imm_byte ());
	  break;
	case 0x86:
	  cpu_clk -= 2;
	  A = ld (imm_byte ());
	  break;
	case 0x88:
	  cpu_clk -= 2;
	  A = eor (A, imm_byte ());
	  break;
	case 0x89:
	  cpu_clk -= 2;
	  A = adc (A, imm_byte ());
	  break;
	case 0x8a:
	  cpu_clk -= 2;
	  A = or (A, imm_byte ());
	  break;
	case 0x8b:
	  cpu_clk -= 2;
	  A = add (A, imm_byte ());
	  break;
	case 0x8c:
	  cpu_clk -= 4;
	  cmp16 (X, imm_word ());
	  break;
	case 0x8d:
	  bsr ();
	  break;
	case 0x8e:
	  cpu_clk -= 3;
	  X = ld16 (imm_word ());
	  break;

	case 0x90:
	  direct ();
	  cpu_clk -= 4;
	  A = sub (A, RDMEM (ea));
	  break;
	case 0x91:
	  direct ();
	  cpu_clk -= 4;
	  cmp (A, RDMEM (ea));
	  break;
	case 0x92:
	  direct ();
	  cpu_clk -= 4;
	  A = sbc (A, RDMEM (ea));
	  break;
	case 0x93:
	  direct ();
	  cpu_clk -= 4;
	  subd (RDMEM16 (ea));
	  cpu_clk--;
	  break;
	case 0x94:
	  direct ();
	  cpu_clk -= 4;
	  A = and (A, RDMEM (ea));
	  break;
	case 0x95:
	  direct ();
	  cpu_clk -= 4;
	  bit (A, RDMEM (ea));
	  break;
	case 0x96:
	  direct ();
	  cpu_clk -= 4;
	  A = ld (RDMEM (ea));
	  break;
	case 0x97:
	  direct ();
	  cpu_clk -= 4;
	  st (A);
	  break;
	case 0x98:
	  direct ();
	  cpu_clk -= 4;
	  A = eor (A, RDMEM (ea));
	  break;
	case 0x99:
	  direct ();
	  cpu_clk -= 4;
	  A = adc (A, RDMEM (ea));
	  break;
	case 0x9a:
	  direct ();
	  cpu_clk -= 4;
	  A = or (A, RDMEM (ea));
	  break;
	case 0x9b:
	  direct ();
	  cpu_clk -= 4;
	  A = add (A, RDMEM (ea));
	  break;
	case 0x9c:
	  direct ();
	  cpu_clk -= 4;
	  cmp16 (X, RDMEM16 (ea));
	  cpu_clk--;
	  break;
	case 0x9d:
	  direct ();
	  cpu_clk -= 7;
	  jsr ();
	  break;
	case 0x9e:
	  direct ();
	  cpu_clk -= 4;
	  X = ld16 (RDMEM16 (ea));
	  break;
	case 0x9f:
	  direct ();
	  cpu_clk -= 4;
	  st16 (X);
	  break;

	case 0xa0:
	  indexed ();
	  A = sub (A, RDMEM (ea));
	  break;
	case 0xa1:
	  indexed ();
	  cmp (A, RDMEM (ea));
	  break;
	case 0xa2:
	  indexed ();
	  A = sbc (A, RDMEM (ea));
	  break;
	case 0xa3:
	  indexed ();
	  subd (RDMEM16 (ea));
	  cpu_clk--;
	  break;
	case 0xa4:
	  indexed ();
	  A = and (A, RDMEM (ea));
	  break;
	case 0xa5:
	  indexed ();
	  bit (A, RDMEM (ea));
	  break;
	case 0xa6:
	  indexed ();
	  A = ld (RDMEM (ea));
	  break;
	case 0xa7:
	  indexed ();
	  st (A);
	  break;
	case 0xa8:
	  indexed ();
	  A = eor (A, RDMEM (ea));
	  break;
	case 0xa9:
	  indexed ();
	  A = adc (A, RDMEM (ea));
	  break;
	case 0xaa:
	  indexed ();
	  A = or (A, RDMEM (ea));
	  break;
	case 0xab:
	  indexed ();
	  A = add (A, RDMEM (ea));
	  break;
	case 0xac:
	  indexed ();
	  cmp16 (X, RDMEM16 (ea));
	  cpu_clk--;
	  break;
	case 0xad:
	  indexed ();
	  cpu_clk -= 3;
	  jsr ();
	  break;
	case 0xae:
	  indexed ();
	  X = ld16 (RDMEM16 (ea));
	  break;
	case 0xaf:
	  indexed ();
	  st16 (X);
	  break;

	case 0xb0:
	  extended ();
	  cpu_clk -= 5;
	  A = sub (A, RDMEM (ea));
	  break;
	case 0xb1:
	  extended ();
	  cpu_clk -= 5;
	  cmp (A, RDMEM (ea));
	  break;
	case 0xb2:
	  extended ();
	  cpu_clk -= 5;
	  A = sbc (A, RDMEM (ea));
	  break;
	case 0xb3:
	  extended ();
	  cpu_clk -= 5;
	  subd (RDMEM16 (ea));
	  cpu_clk--;
	  break;
	case 0xb4:
	  extended ();
	  cpu_clk -= 5;
	  A = and (A, RDMEM (ea));
	  break;
	case 0xb5:
	  extended ();
	  cpu_clk -= 5;
	  bit (A, RDMEM (ea));
	  break;
	case 0xb6:
	  extended ();
	  cpu_clk -= 5;
	  A = ld (RDMEM (ea));
	  break;
	case 0xb7:
	  extended ();
	  cpu_clk -= 5;
	  st (A);
	  break;
	case 0xb8:
	  extended ();
	  cpu_clk -= 5;
	  A = eor (A, RDMEM (ea));
	  break;
	case 0xb9:
	  extended ();
	  cpu_clk -= 5;
	  A = adc (A, RDMEM (ea));
	  break;
	case 0xba:
	  extended ();
	  cpu_clk -= 5;
	  A = or (A, RDMEM (ea));
	  break;
	case 0xbb:
	  extended ();
	  cpu_clk -= 5;
	  A = add (A, RDMEM (ea));
	  break;
	case 0xbc:
	  extended ();
	  cpu_clk -= 5;
	  cmp16 (X, RDMEM16 (ea));
	  cpu_clk--;
	  break;
	case 0xbd:
	  extended ();
	  cpu_clk -= 8;
	  jsr ();
	  break;
	case 0xbe:
	  extended ();
	  cpu_clk -= 5;
	  X = ld16 (RDMEM16 (ea));
	  break;
	case 0xbf:
	  extended ();
	  cpu_clk -= 5;
	  st16 (X);
	  break;

	case 0xc0:
	  cpu_clk -= 2;
	  B = sub (B, imm_byte ());
	  break;
	case 0xc1:
	  cpu_clk -= 2;
	  cmp (B, imm_byte ());
	  break;
	case 0xc2:
	  cpu_clk -= 2;
	  B = sbc (B, imm_byte ());
	  break;
	case 0xc3:
	  cpu_clk -= 4;
	  addd (imm_word ());
	  break;
	case 0xc4:
	  cpu_clk -= 2;
	  B = and (B, imm_byte ());
	  break;
	case 0xc5:
	  cpu_clk -= 2;
	  bit (B, imm_byte ());
	  break;
	case 0xc6:
	  cpu_clk -= 2;
	  B = ld (imm_byte ());
	  break;
	case 0xc8:
	  cpu_clk -= 2;
	  B = eor (B, imm_byte ());
	  break;
	case 0xc9:
	  cpu_clk -= 2;
	  B = adc (B, imm_byte ());
	  break;
	case 0xca:
	  cpu_clk -= 2;
	  B = or (B, imm_byte ());
	  break;
	case 0xcb:
	  cpu_clk -= 2;
	  B = add (B, imm_byte ());
	  break;
	case 0xcc:
	  cpu_clk -= 3;
	  ldd (imm_word ());
	  break;
#ifdef H6309
	case 0xcd:		/* LDQ immed */
	  break;
#endif
	case 0xce:
	  cpu_clk -= 3;
	  U = ld16 (imm_word ());
	  break;

	case 0xd0:
	  direct ();
	  cpu_clk -= 4;
	  B = sub (B, RDMEM (ea));
	  break;
	case 0xd1:
	  direct ();
	  cpu_clk -= 4;
	  cmp (B, RDMEM (ea));
	  break;
	case 0xd2:
	  direct ();
	  cpu_clk -= 4;
	  B = sbc (B, RDMEM (ea));
	  break;
	case 0xd3:
	  direct ();
	  cpu_clk -= 4;
	  addd (RDMEM16 (ea));
	  cpu_clk--;
	  break;
	case 0xd4:
	  direct ();
	  cpu_clk -= 4;
	  B = and (B, RDMEM (ea));
	  break;
	case 0xd5:
	  direct ();
	  cpu_clk -= 4;
	  bit (B, RDMEM (ea));
	  break;
	case 0xd6:
	  direct ();
	  cpu_clk -= 4;
	  B = ld (RDMEM (ea));
	  break;
	case 0xd7:
	  direct ();
	  cpu_clk -= 4;
	  st (B);
	  break;
	case 0xd8:
	  direct ();
	  cpu_clk -= 4;
	  B = eor (B, RDMEM (ea));
	  break;
	case 0xd9:
	  direct ();
	  cpu_clk -= 4;
	  B = adc (B, RDMEM (ea));
	  break;
	case 0xda:
	  direct ();
	  cpu_clk -= 4;
	  B = or (B, RDMEM (ea));
	  break;
	case 0xdb:
	  direct ();
	  cpu_clk -= 4;
	  B = add (B, RDMEM (ea));
	  break;
	case 0xdc:
	  direct ();
	  cpu_clk -= 4;
	  ldd (RDMEM16 (ea));
	  break;
	case 0xdd:
	  direct ();
	  cpu_clk -= 4;
	  std ();
	  break;
	case 0xde:
	  direct ();
	  cpu_clk -= 4;
	  U = ld16 (RDMEM16 (ea));
	  break;
	case 0xdf:
	  direct ();
	  cpu_clk -= 4;
	  st16 (U);
	  break;

	case 0xe0:
	  indexed ();
	  B = sub (B, RDMEM (ea));
	  break;
	case 0xe1:
	  indexed ();
	  cmp (B, RDMEM (ea));
	  break;
	case 0xe2:
	  indexed ();
	  B = sbc (B, RDMEM (ea));
	  break;
	case 0xe3:
	  indexed ();
	  addd (RDMEM16 (ea));
	  cpu_clk--;
	  break;
	case 0xe4:
	  indexed ();
	  B = and (B, RDMEM (ea));
	  break;
	case 0xe5:
	  indexed ();
	  bit (B, RDMEM (ea));
	  break;
	case 0xe6:
	  indexed ();
	  B = ld (RDMEM (ea));
	  break;
	case 0xe7:
	  indexed ();
	  st (B);
	  break;
	case 0xe8:
	  indexed ();
	  B = eor (B, RDMEM (ea));
	  break;
	case 0xe9:
	  indexed ();
	  B = adc (B, RDMEM (ea));
	  break;
	case 0xea:
	  indexed ();
	  B = or (B, RDMEM (ea));
	  break;
	case 0xeb:
	  indexed ();
	  B = add (B, RDMEM (ea));
	  break;
	case 0xec:
	  indexed ();
	  ldd (RDMEM16 (ea));
	  break;
	case 0xed:
	  indexed ();
	  std ();
	  break;
	case 0xee:
	  indexed ();
	  U = ld16 (RDMEM16 (ea));
	  break;
	case 0xef:
	  indexed ();
	  st16 (U);
	  break;

	case 0xf0:
	  extended ();
	  cpu_clk -= 5;
	  B = sub (B, RDMEM (ea));
	  break;
	case 0xf1:
	  extended ();
	  cpu_clk -= 5;
	  cmp (B, RDMEM (ea));
	  break;
	case 0xf2:
	  extended ();
	  cpu_clk -= 5;
	  B = sbc (B, RDMEM (ea));
	  break;
	case 0xf3:
	  extended ();
	  cpu_clk -= 5;
	  addd (RDMEM16 (ea));
	  cpu_clk--;
	  break;
	case 0xf4:
	  extended ();
	  cpu_clk -= 5;
	  B = and (B, RDMEM (ea));
	  break;
	case 0xf5:
	  extended ();
	  cpu_clk -= 5;
	  bit (B, RDMEM (ea));
	  break;
	case 0xf6:
	  extended ();
	  cpu_clk -= 5;
	  B = ld (RDMEM (ea));
	  break;
	case 0xf7:
	  extended ();
	  cpu_clk -= 5;
	  st (B);
	  break;
	case 0xf8:
	  extended ();
	  cpu_clk -= 5;
	  B = eor (B, RDMEM (ea));
	  break;
	case 0xf9:
	  extended ();
	  cpu_clk -= 5;
	  B = adc (B, RDMEM (ea));
	  break;
	case 0xfa:
	  extended ();
	  cpu_clk -= 5;
	  B = or (B, RDMEM (ea));
	  break;
	case 0xfb:
	  extended ();
	  cpu_clk -= 5;
	  B = add (B, RDMEM (ea));
	  break;
	case 0xfc:
	  extended ();
	  cpu_clk -= 5;
	  ldd (RDMEM16 (ea));
	  break;
	case 0xfd:
	  extended ();
	  cpu_clk -= 5;
	  std ();
	  break;
	case 0xfe:
	  extended ();
	  cpu_clk -= 5;
	  U = ld16 (RDMEM16 (ea));
	  break;
	case 0xff:
	  extended ();
	  cpu_clk -= 5;
	  st16 (U);
	  break;

	default:
	  cpu_clk -= 2;
	  sim_error ("invalid opcode '%02X'\n", opcode);
	  PC = iPC;
	  break;
	}

	if (cc_changed)
	  cc_modified ();
      
   tubeUseCycles(1);   
  } while (tubeContinueRunning());
  //while (cpu_clk > 0);

  cpu_period -= cpu_clk;
  cpu_clk = cpu_period;
  return cpu_period;
}

void mc6809nc_reset (void)
{
   X = Y = S = U = A = B = DP = 0;
   H = N = OV = C = 0;
   Z = 1;
   EFI = F_FLAG | I_FLAG;
#ifdef H6309
   MD = E = F = V = 0;
#endif
   sync_flag = 0;
   change_pc (read16 (0xfefe));
}

void print_regs (void)
{
   char flags[9] = "        \0";
   if (get_cc() & C_FLAG) flags[0] = 'C';
   if (get_cc() & V_FLAG) flags[1] = 'V';
   if (get_cc() & Z_FLAG) flags[2] = 'Z';
   if (get_cc() & N_FLAG) flags[3] = 'N';
   if (get_cc() & I_FLAG) flags[4] = 'I';
   if (get_cc() & H_FLAG) flags[5] = 'H';
   if (get_cc() & F_FLAG) flags[6] = 'F';
   if (get_cc() & E_FLAG) flags[7] = 'E';

   printf (" X: 0x%04X  [X]: 0x%04X    Y: 0x%04X  [Y]: 0x%04X    ",
            get_x(), read16(get_x()), get_y(), read16(get_y()) );
   printf ("PC: 0x%04X [PC]: 0x%04X\n",
            get_pc(), read16(get_pc()) );
   printf (" U: 0x%04X  [U]: 0x%04X    S: 0x%04X  [S]: 0x%04X    ",
            get_u(), read16(get_u()), get_s(), read16(get_s()) );
   printf ("DP: 0x%02X\n", get_dp() );
   printf (" A: 0x%02X      B: 0x%02X    [D]: 0x%04X   CC: %s\n",
            get_a(), get_b(), read16(get_d()), flags );
}
