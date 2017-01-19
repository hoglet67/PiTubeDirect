/*  Copyright 2003-2016 Ciaran Anscomb
 *
 *  This file is part of XRoar.
 *
 *  XRoar is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  XRoar is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with XRoar.  If not, see <http://www.gnu.org/licenses/>.
 */

/* References:
 *     MC6809E data sheet,
 *         Motorola
 *     MC6809 Cycle-By-Cycle Performance,
 *         http://koti.mbnet.fi/~atjs/mc6809/Information/6809cyc.txt
 *     Dragon Update, Illegal Op-codes,
 *         Feb 1994 Ciaran Anscomb
 *     Motorola 6809 and Hitachi 6309 Programmers Reference,
 *         2009 Darren Atkinson
 */

#include "config.h"

#include <limits.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "xalloc.h"

#include "delegate.h"
#include "mc6809.h"

inline void MC6809_HALT_SET(struct MC6809 *cpu, _Bool val) {
	cpu->halt = val;
}

inline void MC6809_NMI_SET(struct MC6809 *cpu, _Bool val) {
	cpu->nmi = val;
}

inline void MC6809_FIRQ_SET(struct MC6809 *cpu, _Bool val) {
	cpu->firq = val;
}

inline void MC6809_IRQ_SET(struct MC6809 *cpu, _Bool val) {
	cpu->irq = val;
}

/*
 * External interface
 */

static void mc6809_free(struct MC6809 *cpu);
static void mc6809_reset(struct MC6809 *cpu);
static void mc6809_run(struct MC6809 *cpu);
static void mc6809_jump(struct MC6809 *cpu, uint16_t pc);

/*
 * Common 6809 functions
 */

#include "mc6809_common.h"

/*
 * Data reading & writing
 */

/* Compute effective address */

static uint16_t ea_direct(struct MC6809 *cpu);
static uint16_t ea_extended(struct MC6809 *cpu);
static uint16_t ea_indexed(struct MC6809 *cpu);

/*
 * Interrupt handling
 */

static void push_irq_registers(struct MC6809 *cpu);
static void push_firq_registers(struct MC6809 *cpu);
static void stack_irq_registers(struct MC6809 *cpu, _Bool entire);
static void take_interrupt(struct MC6809 *cpu, uint8_t mask, uint16_t vec);
static void instruction_posthook(struct MC6809 *cpu);

/*
 * ALU operations
 */

/* Illegal 6809 8-bit inherent operations */

static uint8_t op_negcom(struct MC6809 *cpu, uint8_t in);

/* Illegal 6809 8-bit arithmetic operations */

static uint8_t op_discard(struct MC6809 *cpu, uint8_t a, uint8_t b);

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

/*
 * Register handling macros
 */

#define REG_CC (cpu->reg_cc)
#define REG_D (cpu->reg_d)
#define REG_DP (cpu->reg_dp)
#define REG_X (cpu->reg_x)
#define REG_Y (cpu->reg_y)
#define REG_U (cpu->reg_u)
#define REG_S (cpu->reg_s)
#define REG_PC (cpu->reg_pc)

/* Separate read & write for accumulators */

#define RREG_A (REG_D >> 8)
#define RREG_B (REG_D & 0xff)
#define WREG_A (MC6809_REG_A(cpu))
#define WREG_B (MC6809_REG_B(cpu))

/* Condition code register macros */

#define CC_E (0x80)
#define CC_F (0x40)
#define CC_H (0x20)
#define CC_I (0x10)
#define CC_N (0x08)
#define CC_Z (0x04)
#define CC_V (0x02)
#define CC_C (0x01)

#define CLR_HNZVC ( REG_CC &= ~(CC_H|CC_N|CC_Z|CC_V|CC_C) )
#define CLR_NZ    ( REG_CC &= ~(CC_N|CC_Z) )
#define CLR_NZV   ( REG_CC &= ~(CC_N|CC_Z|CC_V) )
#define CLR_NZVC  ( REG_CC &= ~(CC_N|CC_Z|CC_V|CC_C) )
#define CLR_Z     ( REG_CC &= ~(CC_Z) )
#define CLR_NZC   ( REG_CC &= ~(CC_N|CC_Z|CC_C) )
#define CLR_NVC   ( REG_CC &= ~(CC_N|CC_V|CC_C) )
#define CLR_ZC    ( REG_CC &= ~(CC_Z|CC_C) )

#define SET_Z(r)          ( REG_CC |= ((r) ? 0 : CC_Z) )
#define SET_N8(r)         ( REG_CC |= (r&0x80)>>4 )
#define SET_N16(r)        ( REG_CC |= (r&0x8000)>>12 )
#define SET_H(a,b,r)      ( REG_CC |= ((a^b^r)&0x10)<<1 )
#define SET_C8(r)         ( REG_CC |= (r&0x100)>>8 )
#define SET_C16(r)        ( REG_CC |= (r&0x10000)>>16 )
#define SET_V8(a,b,r)     ( REG_CC |= ((a^b^r^(r>>1))&0x80)>>6 )
#define SET_V16(a,b,r)    ( REG_CC |= ((a^b^r^(r>>1))&0x8000)>>14 )
#define SET_NZ8(r)        ( SET_N8(r), SET_Z(r&0xff) )
#define SET_NZ16(r)       ( SET_N16(r), SET_Z(r&0xffff) )
#define SET_NZC8(r)       ( SET_N8(r), SET_Z(r&0xff), SET_C8(r) )
#define SET_NZVC8(a,b,r)  ( SET_N8(r), SET_Z(r&0xff), SET_V8(a,b,r), SET_C8(r) )
#define SET_NZVC16(a,b,r) ( SET_N16(r), SET_Z(r&0xffff), SET_V16(a,b,r), SET_C16(r) )

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

/*
 * External interface
 */

struct MC6809 *mc6809_new(void) {
	struct MC6809 *cpu = xmalloc(sizeof(*cpu));
	*cpu = (struct MC6809){0};
	cpu->variant = MC6809_VARIANT_MC6809;
	cpu->free = mc6809_free;
	cpu->reset = mc6809_reset;
	cpu->run = mc6809_run;
	cpu->jump = mc6809_jump;
	// External handlers
	cpu->mem_cycle = DELEGATE_DEFAULT2(void, bool, uint16);
	mc6809_reset(cpu);
	return cpu;
}

static void mc6809_free(struct MC6809 *cpu) {
	free(cpu);
}

static void mc6809_reset(struct MC6809 *cpu) {
	cpu->halt = cpu->nmi = 0;
	cpu->nmi_armed = 0;
	cpu->nmi = cpu->nmi_latch = cpu->nmi_active = 0;
	cpu->firq = cpu->firq_latch = cpu->firq_active = 0;
	cpu->irq = cpu->irq_latch = cpu->irq_active = 0;
	cpu->state = mc6809_state_reset;
}

/* Run CPU while cpu->running is true. */

static void mc6809_run(struct MC6809 *cpu) {

	do {

		switch (cpu->state) {

		case mc6809_state_reset:
			REG_DP = 0;
			REG_CC |= (CC_F | CC_I);
			cpu->nmi_armed = 0;
			cpu->nmi = 0;
			cpu->nmi_active = 0;
			cpu->firq_active = 0;
			cpu->irq_active = 0;
			cpu->state = mc6809_state_reset_check_halt;
			// fall through

		case mc6809_state_reset_check_halt:
			if (cpu->halt) {
				NVMA_CYCLE;
				continue;
			}
			REG_PC = fetch_word(cpu, MC6809_INT_VEC_RESET);
			NVMA_CYCLE;
			cpu->state = mc6809_state_label_a;
			continue;

		// done_instruction case for backwards-compatibility
		case mc6809_state_done_instruction:
		case mc6809_state_label_a:
			if (cpu->halt) {
				NVMA_CYCLE;
				continue;
			}
			cpu->state = mc6809_state_label_b;
			// fall through

		case mc6809_state_label_b:
			if (cpu->nmi_active) {
				peek_byte(cpu, REG_PC);
				peek_byte(cpu, REG_PC);
				stack_irq_registers(cpu, 1);
				cpu->state = mc6809_state_dispatch_irq;
				continue;
			}
			if (!(REG_CC & CC_F) && cpu->firq_active) {
				peek_byte(cpu, REG_PC);
				peek_byte(cpu, REG_PC);
				stack_irq_registers(cpu, 0);
				cpu->state = mc6809_state_dispatch_irq;
				continue;
			}
			if (!(REG_CC & CC_I) && cpu->irq_active) {
				peek_byte(cpu, REG_PC);
				peek_byte(cpu, REG_PC);
				stack_irq_registers(cpu, 1);
				cpu->state = mc6809_state_dispatch_irq;
				continue;
			}
			cpu->state = mc6809_state_next_instruction;
			// Instruction fetch hook called here so that machine
			// can be stopped beforehand.
			DELEGATE_SAFE_CALL0(cpu->instruction_hook);
			continue;

		case mc6809_state_dispatch_irq:
			if (cpu->nmi_active) {
				cpu->nmi_active = cpu->nmi = cpu->nmi_latch = 0;
				take_interrupt(cpu, CC_F|CC_I, MC6809_INT_VEC_NMI);
				cpu->state = mc6809_state_label_a;
				continue;
			}
			if (!(REG_CC & CC_F) && cpu->firq_active) {
				take_interrupt(cpu, CC_F|CC_I, MC6809_INT_VEC_FIRQ);
				cpu->state = mc6809_state_label_a;
				continue;
			}
			if (!(REG_CC & CC_I) && cpu->irq_active) {
				take_interrupt(cpu, CC_I, MC6809_INT_VEC_IRQ);
				cpu->state = mc6809_state_label_a;
				continue;
			}
			cpu->state = mc6809_state_cwai_check_halt;
			continue;

		case mc6809_state_cwai_check_halt:
			cpu->nmi_active = cpu->nmi_latch;
			cpu->firq_active = cpu->firq_latch;
			cpu->irq_active = cpu->irq_latch;
			NVMA_CYCLE;
			if (cpu->halt) {
				continue;
			}
			cpu->state = mc6809_state_dispatch_irq;
			continue;

		case mc6809_state_sync:
			if (cpu->nmi_active || cpu->firq_active || cpu->irq_active) {
				NVMA_CYCLE;
				instruction_posthook(cpu);
				cpu->state = mc6809_state_label_b;
				continue;
			}
			cpu->nmi_active = cpu->nmi_latch;
			cpu->firq_active = cpu->firq_latch;
			cpu->irq_active = cpu->irq_latch;
			NVMA_CYCLE;
			if (cpu->halt)
				cpu->state = mc6809_state_sync_check_halt;
			continue;

		case mc6809_state_sync_check_halt:
			NVMA_CYCLE;
			if (!cpu->halt) {
				cpu->state = mc6809_state_sync;
			}
			continue;

		case mc6809_state_next_instruction:
			{
			unsigned op;
			cpu->state = mc6809_state_label_a;
			// Fetch op-code and process
			op = byte_immediate(cpu);
			switch (op) {

			// 0x00 - 0x0f direct mode ops
			// 0x40 - 0x4f inherent A register ops
			// 0x50 - 0x5f inherent B register ops
			// 0x60 - 0x6f indexed mode ops
			// 0x70 - 0x7f extended mode ops
			case 0x00: case 0x01: case 0x02: case 0x03:
			case 0x04: case 0x05: case 0x06: case 0x07:
			case 0x08: case 0x09: case 0x0a: case 0x0b:
			case 0x0c: case 0x0d: case 0x0f:
			case 0x40: case 0x41: case 0x42: case 0x43:
			case 0x44: case 0x45: case 0x46: case 0x47:
			case 0x48: case 0x49: case 0x4a: case 0x4b:
			case 0x4c: case 0x4d: case 0x4f:
			case 0x50: case 0x51: case 0x52: case 0x53:
			case 0x54: case 0x55: case 0x56: case 0x57:
			case 0x58: case 0x59: case 0x5a: case 0x5b:
			case 0x5c: case 0x5d: case 0x5f:
			case 0x60: case 0x61: case 0x62: case 0x63:
			case 0x64: case 0x65: case 0x66: case 0x67:
			case 0x68: case 0x69: case 0x6a: case 0x6b:
			case 0x6c: case 0x6d: case 0x6f:
			case 0x70: case 0x71: case 0x72: case 0x73:
			case 0x74: case 0x75: case 0x76: case 0x77:
			case 0x78: case 0x79: case 0x7a: case 0x7b:
			case 0x7c: case 0x7d: case 0x7f: {
				uint16_t ea;
				unsigned tmp1;
				switch ((op >> 4) & 0xf) {
				case 0x0: ea = ea_direct(cpu); tmp1 = fetch_byte(cpu, ea); break;
				case 0x4: ea = 0; tmp1 = RREG_A; break;
				case 0x5: ea = 0; tmp1 = RREG_B; break;
				case 0x6: ea = ea_indexed(cpu); tmp1 = fetch_byte(cpu, ea); break;
				case 0x7: ea = ea_extended(cpu); tmp1 = fetch_byte(cpu, ea); break;
				default: ea = tmp1 = 0; break;
				}
				switch (op & 0xf) {
				case 0x1: // NEG illegal
				case 0x0: tmp1 = op_neg(cpu, tmp1); break; // NEG, NEGA, NEGB
				case 0x2: tmp1 = op_negcom(cpu, tmp1); break; // NEGCOM illegal
				case 0x3: tmp1 = op_com(cpu, tmp1); break; // COM, COMA, COMB
				case 0x5: // LSR illegal
				case 0x4: tmp1 = op_lsr(cpu, tmp1); break; // LSR, LSRA, LSRB
				case 0x6: tmp1 = op_ror(cpu, tmp1); break; // ROR, RORA, RORB
				case 0x7: tmp1 = op_asr(cpu, tmp1); break; // ASR, ASRA, ASRB
				case 0x8: tmp1 = op_asl(cpu, tmp1); break; // ASL, ASLA, ASLB
				case 0x9: tmp1 = op_rol(cpu, tmp1); break; // ROL, ROLA, ROLB
				case 0xb: // DEC illegal
				case 0xa: tmp1 = op_dec(cpu, tmp1); break; // DEC, DECA, DECB
				case 0xc: tmp1 = op_inc(cpu, tmp1); break; // INC, INCA, INCB
				case 0xd: tmp1 = op_tst(cpu, tmp1); break; // TST, TSTA, TSTB
				case 0xf: tmp1 = op_clr(cpu, tmp1); break; // CLR, CLRA, CLRB
				default: break;
				}
				switch (op & 0xf) {
				case 0xd: // TST
					NVMA_CYCLE;
					NVMA_CYCLE;
					break;
				default: // the rest need storing
					switch ((op >> 4) & 0xf) {
					default:
					case 0x0: case 0x6: case 0x7:
						NVMA_CYCLE;
						store_byte(cpu, ea, tmp1);
						break;
					case 0x4:
						WREG_A = tmp1;
						peek_byte(cpu, REG_PC);
						break;
					case 0x5:
						WREG_B = tmp1;
						peek_byte(cpu, REG_PC);
						break;
					}
				}
			} break;

			// 0x0e JMP direct
			// 0x6e JMP indexed
			// 0x7e JMP extended
			case 0x0e: case 0x6e: case 0x7e: {
				unsigned ea;
				switch ((op >> 4) & 0xf) {
				case 0x0: ea = ea_direct(cpu); break;
				case 0x6: ea = ea_indexed(cpu); break;
				case 0x7: ea = ea_extended(cpu); break;
				default: ea = 0; break;
				}
				REG_PC = ea;
			} break;

			// 0x10 Page 2
			case 0x10:
				cpu->state = mc6809_state_instruction_page_2;
				continue;
			// 0x11 Page 3
			case 0x11:
				cpu->state = mc6809_state_instruction_page_3;
				continue;
			// 0x12 NOP inherent
			case 0x12: peek_byte(cpu, REG_PC); break;
			// 0x13 SYNC inherent
			case 0x13:
				peek_byte(cpu, REG_PC);
				cpu->nmi_active = cpu->nmi_latch;
				cpu->firq_active = cpu->firq_latch;
				cpu->irq_active = cpu->irq_latch;
				instruction_posthook(cpu);
				cpu->state = mc6809_state_sync;
				continue;
			// 0x14, 0x15 HCF? (illegal)
			case 0x14:
			case 0x15:
				cpu->state = mc6809_state_hcf;
				break;
			// 0x16 LBRA relative
			case 0x16: {
				uint16_t ea;
				ea = long_relative(cpu);
				REG_PC += ea;
				NVMA_CYCLE;
				NVMA_CYCLE;
			} break;
			// 0x17 LBSR relative
			case 0x17: {
				uint16_t ea;
				ea = long_relative(cpu);
				ea += REG_PC;
				NVMA_CYCLE;
				NVMA_CYCLE;
				NVMA_CYCLE;
				NVMA_CYCLE;
				push_s_word(cpu, REG_PC);
				REG_PC = ea;
			} break;
			// 0x18 Shift CC with mask inherent (illegal)
			case 0x18:
				REG_CC = (REG_CC << 1) & (CC_H | CC_Z);
				NVMA_CYCLE;
				peek_byte(cpu, REG_PC);
				break;
			// 0x19 DAA inherent
			case 0x19: {
				unsigned tmp = 0;
				if ((RREG_A&0x0f) >= 0x0a || REG_CC & CC_H) tmp |= 0x06;
				if (RREG_A >= 0x90 && (RREG_A&0x0f) >= 0x0a) tmp |= 0x60;
				if (RREG_A >= 0xa0 || REG_CC & CC_C) tmp |= 0x60;
				tmp += RREG_A;
				WREG_A = tmp;
				// CC.C NOT cleared, only set if appropriate
				CLR_NZV;
				SET_NZC8(tmp);
				peek_byte(cpu, REG_PC);
			} break;
			// 0x1a ORCC immediate
			case 0x1a: {
				unsigned data;
				data = byte_immediate(cpu);
				REG_CC |= data;
				peek_byte(cpu, REG_PC);
			} break;
			// 0x1b NOP inherent (illegal)
			case 0x1b: peek_byte(cpu, REG_PC); break;
			// 0x1c ANDCC immediate
			case 0x1c: {
				unsigned data;
				data = byte_immediate(cpu);
				REG_CC &= data;
				peek_byte(cpu, REG_PC);
			} break;
			// 0x1d SEX inherent
			case 0x1d:
				WREG_A = (RREG_B & 0x80) ? 0xff : 0;
				CLR_NZ;
				SET_NZ16(REG_D);
				peek_byte(cpu, REG_PC);
				break;
			// 0x1e EXG immediate
			case 0x1e: {
				unsigned postbyte;
				uint16_t tmp1, tmp2;
				postbyte = byte_immediate(cpu);
				switch (postbyte >> 4) {
					case 0x0: tmp1 = REG_D; break;
					case 0x1: tmp1 = REG_X; break;
					case 0x2: tmp1 = REG_Y; break;
					case 0x3: tmp1 = REG_U; break;
					case 0x4: tmp1 = REG_S; break;
					case 0x5: tmp1 = REG_PC; break;
					case 0x8: tmp1 = RREG_A | 0xff00; break;
					case 0x9: tmp1 = RREG_B | 0xff00; break;
					// TODO: verify this behaviour
					case 0xa: tmp1 = (REG_CC << 8) | REG_CC; break;
					case 0xb: tmp1 = (REG_DP << 8) | REG_DP; break;
					default: tmp1 = 0xffff; break;
				}
				switch (postbyte & 0xf) {
					case 0x0: tmp2 = REG_D; REG_D = tmp1; break;
					case 0x1: tmp2 = REG_X; REG_X = tmp1; break;
					case 0x2: tmp2 = REG_Y; REG_Y = tmp1; break;
					case 0x3: tmp2 = REG_U; REG_U = tmp1; break;
					case 0x4: tmp2 = REG_S; REG_S = tmp1; break;
					case 0x5: tmp2 = REG_PC; REG_PC = tmp1; break;
					case 0x8: tmp2 = RREG_A | 0xff00; WREG_A = tmp1; break;
					case 0x9: tmp2 = RREG_B | 0xff00; WREG_B = tmp1; break;
					// TODO: verify this behaviour
					case 0xa: tmp2 = (REG_CC << 8) | REG_CC; REG_CC = tmp1; break;
					case 0xb: tmp2 = (REG_DP << 8) | REG_DP; REG_DP = tmp1; break;
					default: tmp2 = 0xffff; break;
				}
				switch (postbyte >> 4) {
					case 0x0: REG_D = tmp2; break;
					case 0x1: REG_X = tmp2; break;
					case 0x2: REG_Y = tmp2; break;
					case 0x3: REG_U = tmp2; break;
					case 0x4: REG_S = tmp2; break;
					case 0x5: REG_PC = tmp2; break;
					case 0x8: WREG_A = tmp2; break;
					case 0x9: WREG_B = tmp2; break;
					case 0xa: REG_CC = tmp2; break;
					case 0xb: REG_DP = tmp2; break;
					default: break;
				}
				NVMA_CYCLE;
				NVMA_CYCLE;
				NVMA_CYCLE;
				NVMA_CYCLE;
				NVMA_CYCLE;
				NVMA_CYCLE;
			} break;
			// 0x1f TFR immediate
			case 0x1f: {
				unsigned postbyte;
				uint16_t tmp1;
				postbyte = byte_immediate(cpu);
				switch (postbyte >> 4) {
					case 0x0: tmp1 = REG_D; break;
					case 0x1: tmp1 = REG_X; break;
					case 0x2: tmp1 = REG_Y; break;
					case 0x3: tmp1 = REG_U; break;
					case 0x4: tmp1 = REG_S; break;
					case 0x5: tmp1 = REG_PC; break;
					case 0x8: tmp1 = RREG_A | 0xff00; break;
					case 0x9: tmp1 = RREG_B | 0xff00; break;
					// TODO: verify this behaviour
					case 0xa: tmp1 = (REG_CC << 8) | REG_CC; break;
					case 0xb: tmp1 = (REG_DP << 8) | REG_DP; break;
					default: tmp1 = 0xffff; break;
				}
				switch (postbyte & 0xf) {
					case 0x0: REG_D = tmp1; break;
					case 0x1: REG_X = tmp1; break;
					case 0x2: REG_Y = tmp1; break;
					case 0x3: REG_U = tmp1; break;
					case 0x4: REG_S = tmp1; break;
					case 0x5: REG_PC = tmp1; break;
					case 0x8: WREG_A = tmp1; break;
					case 0x9: WREG_B = tmp1; break;
					case 0xa: REG_CC = tmp1; break;
					case 0xb: REG_DP = tmp1; break;
					default: break;
				}
				NVMA_CYCLE;
				NVMA_CYCLE;
				NVMA_CYCLE;
				NVMA_CYCLE;
			} break;

			// 0x20 - 0x2f short branches
			case 0x20: case 0x21: case 0x22: case 0x23:
			case 0x24: case 0x25: case 0x26: case 0x27:
			case 0x28: case 0x29: case 0x2a: case 0x2b:
			case 0x2c: case 0x2d: case 0x2e: case 0x2f: {
				unsigned tmp = sex8(byte_immediate(cpu));
				NVMA_CYCLE;
				if (branch_condition(cpu, op))
					REG_PC += tmp;
			} break;

			// 0x30 LEAX indexed
			case 0x30:
				REG_X = ea_indexed(cpu);
				CLR_Z;
				SET_Z(REG_X);
				NVMA_CYCLE;
				break;
			// 0x31 LEAY indexed
			case 0x31:
				REG_Y = ea_indexed(cpu);
				CLR_Z;
				SET_Z(REG_Y);
				NVMA_CYCLE;
				break;
			// 0x32 LEAS indexed
			case 0x32:
				REG_S = ea_indexed(cpu);
				NVMA_CYCLE;
				cpu->nmi_armed = 1;  // XXX: Really?
				break;
			// 0x33 LEAU indexed
			case 0x33:
				REG_U = ea_indexed(cpu);
				NVMA_CYCLE;
				break;

			// 0x34 PSHS immediate
			case 0x34:
				{
					unsigned postbyte = byte_immediate(cpu);
					NVMA_CYCLE;
					NVMA_CYCLE;
					peek_byte(cpu, REG_S);
					if (postbyte & 0x80) { push_s_word(cpu, REG_PC); }
					if (postbyte & 0x40) { push_s_word(cpu, REG_U); }
					if (postbyte & 0x20) { push_s_word(cpu, REG_Y); }
					if (postbyte & 0x10) { push_s_word(cpu, REG_X); }
					if (postbyte & 0x08) { push_s_byte(cpu, REG_DP); }
					if (postbyte & 0x04) { push_s_byte(cpu, RREG_B); }
					if (postbyte & 0x02) { push_s_byte(cpu, RREG_A); }
					if (postbyte & 0x01) { push_s_byte(cpu, REG_CC); }
				}
				break;
			// 0x35 PULS immediate
			case 0x35:
				{
					unsigned postbyte = byte_immediate(cpu);
					NVMA_CYCLE;
					NVMA_CYCLE;
					if (postbyte & 0x01) { REG_CC = pull_s_byte(cpu); }
					if (postbyte & 0x02) { WREG_A = pull_s_byte(cpu); }
					if (postbyte & 0x04) { WREG_B = pull_s_byte(cpu); }
					if (postbyte & 0x08) { REG_DP = pull_s_byte(cpu); }
					if (postbyte & 0x10) { REG_X = pull_s_word(cpu); }
					if (postbyte & 0x20) { REG_Y = pull_s_word(cpu); }
					if (postbyte & 0x40) { REG_U = pull_s_word(cpu); }
					if (postbyte & 0x80) { REG_PC = pull_s_word(cpu); }
					peek_byte(cpu, REG_S);
				}
				break;
			// 0x36 PSHU immediate
			case 0x36:
				{
					unsigned postbyte = byte_immediate(cpu);
					NVMA_CYCLE;
					NVMA_CYCLE;
					peek_byte(cpu, REG_U);
					if (postbyte & 0x80) { push_u_word(cpu, REG_PC); }
					if (postbyte & 0x40) { push_u_word(cpu, REG_S); }
					if (postbyte & 0x20) { push_u_word(cpu, REG_Y); }
					if (postbyte & 0x10) { push_u_word(cpu, REG_X); }
					if (postbyte & 0x08) { push_u_byte(cpu, REG_DP); }
					if (postbyte & 0x04) { push_u_byte(cpu, RREG_B); }
					if (postbyte & 0x02) { push_u_byte(cpu, RREG_A); }
					if (postbyte & 0x01) { push_u_byte(cpu, REG_CC); }
				}
				break;
			// 0x37 PULU immediate
			case 0x37:
				{
					unsigned postbyte = byte_immediate(cpu);
					NVMA_CYCLE;
					NVMA_CYCLE;
					if (postbyte & 0x01) { REG_CC = pull_u_byte(cpu); }
					if (postbyte & 0x02) { WREG_A = pull_u_byte(cpu); }
					if (postbyte & 0x04) { WREG_B = pull_u_byte(cpu); }
					if (postbyte & 0x08) { REG_DP = pull_u_byte(cpu); }
					if (postbyte & 0x10) { REG_X = pull_u_word(cpu); }
					if (postbyte & 0x20) { REG_Y = pull_u_word(cpu); }
					if (postbyte & 0x40) { REG_S = pull_u_word(cpu); }
					if (postbyte & 0x80) { REG_PC = pull_u_word(cpu); }
					peek_byte(cpu, REG_U);
				}
				break;
			// 0x38 ANDCC immediate (illegal)
			case 0x38: {
				unsigned data;
				data = byte_immediate(cpu);
				REG_CC &= data;
				peek_byte(cpu, REG_PC);
				/* Differs from legal 0x1c version by
				 * taking one more cycle: */
				NVMA_CYCLE;
			} break;
			// 0x39 RTS inherent
			case 0x39:
				peek_byte(cpu, REG_PC);
				REG_PC = pull_s_word(cpu);
				NVMA_CYCLE;
				break;
			// 0x3a ABX inherent
			case 0x3a:
				REG_X += RREG_B;
				peek_byte(cpu, REG_PC);
				NVMA_CYCLE;
				break;
			// 0x3b RTI inherent
			case 0x3b:
				peek_byte(cpu, REG_PC);
				REG_CC = pull_s_byte(cpu);
				if (REG_CC & CC_E) {
					WREG_A = pull_s_byte(cpu);
					WREG_B = pull_s_byte(cpu);
					REG_DP = pull_s_byte(cpu);
					REG_X = pull_s_word(cpu);
					REG_Y = pull_s_word(cpu);
					REG_U = pull_s_word(cpu);
					REG_PC = pull_s_word(cpu);
				} else {
					REG_PC = pull_s_word(cpu);
				}
				cpu->nmi_armed = 1;
				peek_byte(cpu, REG_S);
				break;
			// 0x3c CWAI immediate
			case 0x3c: {
				unsigned data;
				data = byte_immediate(cpu);
				REG_CC &= data;
				peek_byte(cpu, REG_PC);
				NVMA_CYCLE;
				stack_irq_registers(cpu, 1);
				NVMA_CYCLE;
				cpu->state = mc6809_state_dispatch_irq;
			} break;
			// 0x3d MUL inherent
			case 0x3d: {
				unsigned tmp = RREG_A * RREG_B;
				REG_D = tmp;
				CLR_ZC;
				SET_Z(tmp);
				if (tmp & 0x80)
					REG_CC |= CC_C;
				peek_byte(cpu, REG_PC);
				NVMA_CYCLE;
				NVMA_CYCLE;
				NVMA_CYCLE;
				NVMA_CYCLE;
				NVMA_CYCLE;
				NVMA_CYCLE;
				NVMA_CYCLE;
				NVMA_CYCLE;
				NVMA_CYCLE;
			} break;
			// 0x3e RESET (illegal)
			case 0x3e:
				peek_byte(cpu, REG_PC);
				push_irq_registers(cpu);
				instruction_posthook(cpu);
				take_interrupt(cpu, CC_F|CC_I, MC6809_INT_VEC_RESET);
				cpu->state = mc6809_state_label_a;
				continue;
			// 0x3f SWI inherent
			case 0x3f:
				peek_byte(cpu, REG_PC);
				stack_irq_registers(cpu, 1);
				instruction_posthook(cpu);
				take_interrupt(cpu, CC_F|CC_I, MC6809_INT_VEC_SWI);
				cpu->state = mc6809_state_label_a;
				continue;

			// 0x80 - 0xbf A register arithmetic ops
			// 0xc0 - 0xff B register arithmetic ops
			case 0x80: case 0x81: case 0x82:
			case 0x84: case 0x85: case 0x86: case 0x87:
			case 0x88: case 0x89: case 0x8a: case 0x8b:
			case 0x90: case 0x91: case 0x92:
			case 0x94: case 0x95: case 0x96:
			case 0x98: case 0x99: case 0x9a: case 0x9b:
			case 0xa0: case 0xa1: case 0xa2:
			case 0xa4: case 0xa5: case 0xa6:
			case 0xa8: case 0xa9: case 0xaa: case 0xab:
			case 0xb0: case 0xb1: case 0xb2:
			case 0xb4: case 0xb5: case 0xb6:
			case 0xb8: case 0xb9: case 0xba: case 0xbb:
			case 0xc0: case 0xc1: case 0xc2:
			case 0xc4: case 0xc5: case 0xc6: case 0xc7:
			case 0xc8: case 0xc9: case 0xca: case 0xcb:
			case 0xd0: case 0xd1: case 0xd2:
			case 0xd4: case 0xd5: case 0xd6:
			case 0xd8: case 0xd9: case 0xda: case 0xdb:
			case 0xe0: case 0xe1: case 0xe2:
			case 0xe4: case 0xe5: case 0xe6:
			case 0xe8: case 0xe9: case 0xea: case 0xeb:
			case 0xf0: case 0xf1: case 0xf2:
			case 0xf4: case 0xf5: case 0xf6:
			case 0xf8: case 0xf9: case 0xfa: case 0xfb: {
				unsigned tmp1, tmp2;
				tmp1 = !(op & 0x40) ? RREG_A : RREG_B;
				switch ((op >> 4) & 3) {
				case 0: tmp2 = byte_immediate(cpu); break;
				case 1: tmp2 = byte_direct(cpu); break;
				case 2: tmp2 = byte_indexed(cpu); break;
				case 3: tmp2 = byte_extended(cpu); break;
				default: tmp2 = 0; break;
				}
				switch (op & 0xf) {
				case 0x0: tmp1 = op_sub(cpu, tmp1, tmp2); break; // SUBA, SUBB
				case 0x1: (void)op_sub(cpu, tmp1, tmp2); break; // CMPA, CMPB
				case 0x2: tmp1 = op_sbc(cpu, tmp1, tmp2); break; // SBCA, SBCB
				case 0x4: tmp1 = op_and(cpu, tmp1, tmp2); break; // ANDA, ANDB
				case 0x5: (void)op_and(cpu, tmp1, tmp2); break; // BITA, BITB
				case 0x6: tmp1 = op_ld(cpu, 0, tmp2); break; // LDA, LDB
				case 0x7: tmp1 = op_discard(cpu, tmp1, tmp2); break; // illegal
				case 0x8: tmp1 = op_eor(cpu, tmp1, tmp2); break; // EORA, EORB
				case 0x9: tmp1 = op_adc(cpu, tmp1, tmp2); break; // ADCA, ADCB
				case 0xa: tmp1 = op_or(cpu, tmp1, tmp2); break; // ORA, ORB
				case 0xb: tmp1 = op_add(cpu, tmp1, tmp2); break; // ADDA, ADDB
				default: break;
				}
				if (!(op & 0x40)) {
					WREG_A = tmp1;
				} else {
					WREG_B = tmp1;
				}
			} break;

			// 0x83, 0x93, 0xa3, 0xb3 SUBD
			// 0x8c, 0x9c, 0xac, 0xbc CMPX
			// 0xc3, 0xd3, 0xe3, 0xf3 ADDD
			case 0x83: case 0x93: case 0xa3: case 0xb3:
			case 0x8c: case 0x9c: case 0xac: case 0xbc:
			case 0xc3: case 0xd3: case 0xe3: case 0xf3: {
				unsigned tmp1, tmp2;
				tmp1 = !(op & 0x08) ? REG_D : REG_X;
				switch ((op >> 4) & 3) {
				case 0: tmp2 = word_immediate(cpu); break;
				case 1: tmp2 = word_direct(cpu); break;
				case 2: tmp2 = word_indexed(cpu); break;
				case 3: tmp2 = word_extended(cpu); break;
				default: tmp2 = 0; break;
				}
				switch (op & 0x4f) {
				case 0x03: tmp1 = op_sub16(cpu, tmp1, tmp2); break; // SUBD
				case 0x0c: (void)op_sub16(cpu, tmp1, tmp2); break; // CMPX
				case 0x43: tmp1 = op_add16(cpu, tmp1, tmp2); break; // ADDD
				default: break;
				}
				NVMA_CYCLE;
				if (!(op & 0x08)) {
					REG_D = tmp1;
				}
			} break;

			// 0x8d BSR
			// 0x9d, 0xad, 0xbd JSR
			case 0x8d: case 0x9d: case 0xad: case 0xbd: {
				unsigned ea;
				switch ((op >> 4) & 3) {
				case 0: ea = short_relative(cpu); ea += REG_PC; NVMA_CYCLE; NVMA_CYCLE; NVMA_CYCLE; break;
				case 1: ea = ea_direct(cpu); peek_byte(cpu, ea); NVMA_CYCLE; break;
				case 2: ea = ea_indexed(cpu); peek_byte(cpu, ea); NVMA_CYCLE; break;
				case 3: ea = ea_extended(cpu); peek_byte(cpu, ea); NVMA_CYCLE; break;
				default: ea = 0; break;
				}
				push_s_word(cpu, REG_PC);
				REG_PC = ea;
			} break;

			// 0x8e, 0x9e, 0xae, 0xbe LDX
			// 0xcc, 0xdc, 0xec, 0xfc LDD
			// 0xce, 0xde, 0xee, 0xfe LDU
			case 0x8e: case 0x9e: case 0xae: case 0xbe:
			case 0xcc: case 0xdc: case 0xec: case 0xfc:
			case 0xce: case 0xde: case 0xee: case 0xfe: {
				unsigned tmp1, tmp2;
				switch ((op >> 4) & 3) {
				case 0: tmp2 = word_immediate(cpu); break;
				case 1: tmp2 = word_direct(cpu); break;
				case 2: tmp2 = word_indexed(cpu); break;
				case 3: tmp2 = word_extended(cpu); break;
				default: tmp2 = 0; break;
				}
				tmp1 = op_ld16(cpu, 0, tmp2);
				switch (op & 0x42) {
				case 0x02:
					REG_X = tmp1;
					break;
				case 0x40:
					REG_D = tmp1;
					break;
				case 0x42:
					REG_U = tmp1;
					break;
				default: break;
				}
			} break;

			// 0x97, 0xa7, 0xb7 STA
			// 0xd7, 0xe7, 0xf7 STB
			case 0x97: case 0xa7: case 0xb7:
			case 0xd7: case 0xe7: case 0xf7: {
				uint16_t ea;
				uint8_t tmp1;
				tmp1 = !(op & 0x40) ? RREG_A : RREG_B;
				switch ((op >> 4) & 3) {
				case 1: ea = ea_direct(cpu); break;
				case 2: ea = ea_indexed(cpu); break;
				case 3: ea = ea_extended(cpu); break;
				default: ea = 0; break;
				}
				store_byte(cpu, ea, tmp1);
				CLR_NZV;
				SET_NZ8(tmp1);
			} break;

			// 0x9f, 0xaf, 0xbf STX
			// 0xdd, 0xed, 0xfd STD
			// 0xdf, 0xef, 0xff STU
			case 0x9f: case 0xaf: case 0xbf:
			case 0xdd: case 0xed: case 0xfd:
			case 0xdf: case 0xef: case 0xff: {
				uint16_t ea, tmp1;
				switch (op & 0x42) {
				case 0x02: tmp1 = REG_X; break;
				case 0x40: tmp1 = REG_D; break;
				case 0x42: tmp1 = REG_U; break;
				default: tmp1 = 0; break;
				}
				switch ((op >> 4) & 3) {
				case 1: ea = ea_direct(cpu); break;
				case 2: ea = ea_indexed(cpu); break;
				case 3: ea = ea_extended(cpu); break;
				default: ea = 0; break;
				}
				CLR_NZV;
				SET_NZ16(tmp1);
				store_byte(cpu, ea, tmp1 >> 8);
				store_byte(cpu, ea+1, tmp1);
			} break;

			// 0x8f STX immediate (illegal)
			// 0xcf STU immediate (illegal)
			// Illegal instruction only part working
			case 0x8f: case 0xcf: {
				unsigned tmp1;
				tmp1 = !(op & 0x40) ? REG_X : REG_U;
				(void)fetch_byte(cpu, REG_PC);
				REG_PC++;
				store_byte(cpu, REG_PC, tmp1);
				REG_PC++;
				CLR_NZV;
				REG_CC |= CC_N;
			} break;

			// 0xcd HCF? (illegal)
			case 0xcd:
				cpu->state = mc6809_state_hcf;
				break;

			// Illegal instruction
			default:
				NVMA_CYCLE;
				break;
			}
			break;
			}

		case mc6809_state_instruction_page_2:
			{
			unsigned op;
			cpu->state = mc6809_state_label_a;
			op = byte_immediate(cpu);
			switch (op) {

			// 0x10, 0x11 Page 2
			case 0x10:
			case 0x11:
				cpu->state = mc6809_state_instruction_page_2;
				continue;

			// 0x1020 - 0x102f long branches
			case 0x20: case 0x21: case 0x22: case 0x23:
			case 0x24: case 0x25: case 0x26: case 0x27:
			case 0x28: case 0x29: case 0x2a: case 0x2b:
			case 0x2c: case 0x2d: case 0x2e: case 0x2f: {
				unsigned tmp = word_immediate(cpu);
				if (branch_condition(cpu, op)) {
					REG_PC += tmp;
					NVMA_CYCLE;
				}
				NVMA_CYCLE;
			} break;

			// 0x103f SWI2 inherent
			case 0x3f:
				peek_byte(cpu, REG_PC);
				stack_irq_registers(cpu, 1);
				instruction_posthook(cpu);
				take_interrupt(cpu, 0, MC6809_INT_VEC_SWI2);
				cpu->state = mc6809_state_label_a;
				continue;

			// 0x1083, 0x1093, 0x10a3, 0x10b3 CMPD
			// 0x108c, 0x109c, 0x10ac, 0x10bc CMPY
			case 0x83: case 0x93: case 0xa3: case 0xb3:
			case 0x8c: case 0x9c: case 0xac: case 0xbc: {
				unsigned tmp1, tmp2;
				tmp1 = !(op & 0x08) ? REG_D : REG_Y;
				switch ((op >> 4) & 3) {
				case 0: tmp2 = word_immediate(cpu); break;
				case 1: tmp2 = word_direct(cpu); break;
				case 2: tmp2 = word_indexed(cpu); break;
				case 3: tmp2 = word_extended(cpu); break;
				default: tmp2 = 0; break;
				}
				(void)op_sub16(cpu, tmp1, tmp2);
				NVMA_CYCLE;
			} break;

			// 0x108e, 0x109e, 0x10ae, 0x10be LDY
			// 0x10ce, 0x10de, 0x10ee, 0x10fe LDS
			case 0x8e: case 0x9e: case 0xae: case 0xbe:
			case 0xce: case 0xde: case 0xee: case 0xfe: {
				unsigned tmp1, tmp2;
				switch ((op >> 4) & 3) {
				case 0: tmp2 = word_immediate(cpu); break;
				case 1: tmp2 = word_direct(cpu); break;
				case 2: tmp2 = word_indexed(cpu); break;
				case 3: tmp2 = word_extended(cpu); break;
				default: tmp2 = 0; break;
				}
				tmp1 = op_ld16(cpu, 0, tmp2);
				if (!(op & 0x40)) {
					REG_Y = tmp1;
				} else {
					REG_S = tmp1;
					cpu->nmi_armed = 1;
				}
			} break;

			// 0x109f, 0x10af, 0x10bf STY
			// 0x10df, 0x10ef, 0x10ff STS
			case 0x9f: case 0xaf: case 0xbf:
			case 0xdf: case 0xef: case 0xff: {
				unsigned ea, tmp1;
				tmp1 = !(op & 0x40) ? REG_Y : REG_S;
				switch ((op >> 4) & 3) {
				case 1: ea = ea_direct(cpu); break;
				case 2: ea = ea_indexed(cpu); break;
				case 3: ea = ea_extended(cpu); break;
				default: ea = 0; break;
				}
				CLR_NZV;
				SET_NZ16(tmp1);
				store_byte(cpu, ea, tmp1 >> 8);
				store_byte(cpu, ea+1, tmp1);
			} break;

			// Illegal instruction
			default:
				NVMA_CYCLE;
				break;
			}
			break;
			}

		case mc6809_state_instruction_page_3:
			{
			unsigned op;
			cpu->state = mc6809_state_label_a;
			op = byte_immediate(cpu);
			switch (op) {

			// 0x10, 0x11 Page 3
			case 0x10:
			case 0x11:
				cpu->state = mc6809_state_instruction_page_3;
				continue;

			// 0x113F SWI3 inherent
			case 0x3f:
				peek_byte(cpu, REG_PC);
				stack_irq_registers(cpu, 1);
				instruction_posthook(cpu);
				take_interrupt(cpu, 0, MC6809_INT_VEC_SWI3);
				cpu->state = mc6809_state_label_a;
				continue;

			// 0x1183, 0x1193, 0x11a3, 0x11b3 CMPU
			// 0x118c, 0x119c, 0x11ac, 0x11bc CMPS
			case 0x83: case 0x93: case 0xa3: case 0xb3:
			case 0x8c: case 0x9c: case 0xac: case 0xbc: {
				unsigned tmp1, tmp2;
				tmp1 = !(op & 0x08) ? REG_U : REG_S;
				switch ((op >> 4) & 3) {
				case 0: tmp2 = word_immediate(cpu); break;
				case 1: tmp2 = word_direct(cpu); break;
				case 2: tmp2 = word_indexed(cpu); break;
				case 3: tmp2 = word_extended(cpu); break;
				default: tmp2 = 0; break;
				}
				(void)op_sub16(cpu, tmp1, tmp2);
				NVMA_CYCLE;
			} break;

			// Illegal instruction
			default:
				NVMA_CYCLE;
				break;
			}
			break;
			}

		// Certain illegal instructions cause the CPU to lock up:
		case mc6809_state_hcf:
			NVMA_CYCLE;
			continue;

		}

		cpu->nmi_active = cpu->nmi_latch;
		cpu->firq_active = cpu->firq_latch;
		cpu->irq_active = cpu->irq_latch;
		instruction_posthook(cpu);
		continue;

	} while (0); // (cpu->running);

}

static void mc6809_jump(struct MC6809 *cpu, uint16_t pc) {
	REG_PC = pc;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

/*
 * Common 6809 functions
 */

#include "mc6809_common.c"

/*
 * Data reading & writing
 */

/* Compute effective address */

static uint16_t ea_direct(struct MC6809 *cpu) {
	unsigned ea = REG_DP << 8 | fetch_byte(cpu, REG_PC++);
	NVMA_CYCLE;
	return ea;
}

static uint16_t ea_extended(struct MC6809 *cpu) {
	unsigned ea = fetch_word(cpu, REG_PC);
	REG_PC += 2;
	NVMA_CYCLE;
	return ea;
}

static uint16_t ea_indexed(struct MC6809 *cpu) {
	unsigned ea;
	uint16_t reg;
	unsigned postbyte = byte_immediate(cpu);
	switch ((postbyte >> 5) & 3) {
		case 0: reg = REG_X; break;
		case 1: reg = REG_Y; break;
		case 2: reg = REG_U; break;
		case 3: reg = REG_S; break;
		default: reg = 0; break;
	}
	if ((postbyte & 0x80) == 0) {
		peek_byte(cpu, REG_PC);
		NVMA_CYCLE;
		return reg + sex5(postbyte & 0x1f);
	}
	switch (postbyte & 0x0f) {
		case 0x00: ea = reg; reg += 1; peek_byte(cpu, REG_PC); NVMA_CYCLE; NVMA_CYCLE; break;
		case 0x01: ea = reg; reg += 2; peek_byte(cpu, REG_PC); NVMA_CYCLE; NVMA_CYCLE; NVMA_CYCLE; break;
		case 0x02: reg -= 1; ea = reg; peek_byte(cpu, REG_PC); NVMA_CYCLE; NVMA_CYCLE; break;
		case 0x03: reg -= 2; ea = reg; peek_byte(cpu, REG_PC); NVMA_CYCLE; NVMA_CYCLE; NVMA_CYCLE; break;
		case 0x04: ea = reg; peek_byte(cpu, REG_PC); break;
		case 0x05: ea = reg + sex8(RREG_B); peek_byte(cpu, REG_PC); NVMA_CYCLE; break;
		case 0x07: // illegal
		case 0x06: ea = reg + sex8(RREG_A); peek_byte(cpu, REG_PC); NVMA_CYCLE; break;
		case 0x08: ea = byte_immediate(cpu); ea = sex8(ea) + reg; NVMA_CYCLE; break;
		case 0x09: ea = word_immediate(cpu); ea = ea + reg; NVMA_CYCLE; NVMA_CYCLE; NVMA_CYCLE; break;
		case 0x0a: ea = REG_PC | 0xff; break;
		case 0x0b: ea = reg + REG_D; peek_byte(cpu, REG_PC); peek_byte(cpu, REG_PC + 1); NVMA_CYCLE; NVMA_CYCLE; NVMA_CYCLE; break;
		case 0x0c: ea = byte_immediate(cpu); ea = sex8(ea) + REG_PC; NVMA_CYCLE; break;
		case 0x0d: ea = word_immediate(cpu); ea = ea + REG_PC; peek_byte(cpu, REG_PC); NVMA_CYCLE; NVMA_CYCLE; NVMA_CYCLE; break;
		case 0x0e: ea = 0xffff; break; // illegal
		case 0x0f: ea = word_immediate(cpu); NVMA_CYCLE; break;
		default: ea = 0; break;
	}
	if (postbyte & 0x10) {
		ea = fetch_word(cpu, ea);
		NVMA_CYCLE;
	}
	switch ((postbyte >> 5) & 3) {
	case 0: REG_X = reg; break;
	case 1: REG_Y = reg; break;
	case 2: REG_U = reg; break;
	case 3: REG_S = reg; break;
	}
	return ea;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

/*
 * Interrupt handling
 */

static void push_irq_registers(struct MC6809 *cpu) {
	NVMA_CYCLE;
	push_s_word(cpu, REG_PC);
	push_s_word(cpu, REG_U);
	push_s_word(cpu, REG_Y);
	push_s_word(cpu, REG_X);
	push_s_byte(cpu, REG_DP);
	push_s_byte(cpu, RREG_B);
	push_s_byte(cpu, RREG_A);
	push_s_byte(cpu, REG_CC);
}

static void push_firq_registers(struct MC6809 *cpu) {
	NVMA_CYCLE;
	push_s_word(cpu, REG_PC);
	push_s_byte(cpu, REG_CC);
}

static void stack_irq_registers(struct MC6809 *cpu, _Bool entire) {
	if (entire) {
		REG_CC |= CC_E;
		push_irq_registers(cpu);
	} else {
		REG_CC &= ~CC_E;
		push_firq_registers(cpu);
	}
}

static void take_interrupt(struct MC6809 *cpu, uint8_t mask, uint16_t vec) {
	REG_CC |= mask;
	NVMA_CYCLE;
	DELEGATE_SAFE_CALL1(cpu->interrupt_hook, vec);
	REG_PC = fetch_word(cpu, vec);
	NVMA_CYCLE;
}

static void instruction_posthook(struct MC6809 *cpu) {
	DELEGATE_SAFE_CALL0(cpu->instruction_posthook);
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

/*
 * ALU operations
 */

/* Illegal 6809 8-bit inherent operations */

static uint8_t op_negcom(struct MC6809 *cpu, uint8_t in) {
	unsigned out = ~in + (~REG_CC & 1);
	CLR_NZVC;
	SET_NZVC8(0, in, out);
	return out;
}

/* Illegal 6809 8-bit arithmetic operations */

static uint8_t op_discard(struct MC6809 *cpu, uint8_t a, uint8_t b) {
	(void)b;
	CLR_NZV;
	REG_CC |= CC_N;
	return a;
}
