/*  XRoar - a Dragon/Tandy Coco emulator
 *  Copyright (C) 2003-2016  Ciaran Anscomb
 *
 *  See COPYING.GPL for redistribution conditions. */

#ifndef XROAR_MC6809_H_
#define XROAR_MC6809_H_

#include "config.h"

#include <stdint.h>

#include "pl-endian.h"

#include "delegate.h"

#define MC6809_VARIANT_MC6809 (0x00006809)

#define MC6809_INT_VEC_RESET (0xfefe)
#define MC6809_INT_VEC_NMI (0xfefc)
#define MC6809_INT_VEC_SWI (0xfefa)
#define MC6809_INT_VEC_IRQ (0xfef8)
#define MC6809_INT_VEC_FIRQ (0xfef6)
#define MC6809_INT_VEC_SWI2 (0xfef4)
#define MC6809_INT_VEC_SWI3 (0xfef2)

#define MC6809_COMPAT_STATE_NORMAL (0)
#define MC6809_COMPAT_STATE_SYNC (1)
#define MC6809_COMPAT_STATE_CWAI (2)
#define MC6809_COMPAT_STATE_DONE_INSTRUCTION (11)
#define MC6809_COMPAT_STATE_HCF (12)

/* MPU state.  Represents current position in the high-level flow chart from
 * the data sheet (figure 14). */
enum mc6809_state {
	mc6809_state_label_a      = MC6809_COMPAT_STATE_NORMAL,
	mc6809_state_sync         = MC6809_COMPAT_STATE_SYNC,
	mc6809_state_dispatch_irq = MC6809_COMPAT_STATE_CWAI,
	mc6809_state_label_b,
	mc6809_state_reset,
	mc6809_state_reset_check_halt,
	mc6809_state_next_instruction,
	mc6809_state_instruction_page_2,
	mc6809_state_instruction_page_3,
	mc6809_state_cwai_check_halt,
	mc6809_state_sync_check_halt,
	mc6809_state_done_instruction = MC6809_COMPAT_STATE_DONE_INSTRUCTION,
	mc6809_state_hcf          = MC6809_COMPAT_STATE_HCF
};

/* Interface shared with all 6809-compatible CPUs */
struct MC6809 {
	// Variant
	uint32_t variant;

	/* Interrupt lines */
	_Bool halt, nmi, firq, irq;
	uint8_t D;

	/* Methods */

	void (*free)(struct MC6809 *cpu);
	void (*reset)(struct MC6809 *cpu);
	void (*run)(struct MC6809 *cpu);
	void (*jump)(struct MC6809 *cpu, uint16_t pc);

	/* External handlers */

	/* Memory access cycle */
	DELEGATE_T2(void, bool, uint16) mem_cycle;
	/* Called just before instruction fetch if non-NULL */
	DELEGATE_T0(void) instruction_hook;
	/* Called after instruction is executed */
	DELEGATE_T0(void) instruction_posthook;
	/* Called just before an interrupt vector is read */
	DELEGATE_T1(void, int) interrupt_hook;

	/* Internal state */

	enum mc6809_state state;
	_Bool running;

	/* Registers */
	uint8_t reg_cc, reg_dp;
	uint16_t reg_d;
	uint16_t reg_x, reg_y, reg_u, reg_s, reg_pc;
	/* Interrupts */
	_Bool nmi_armed;
	_Bool nmi_latch, firq_latch, irq_latch;
	_Bool nmi_active, firq_active, irq_active;
};

#if __BYTE_ORDER == __BIG_ENDIAN
# define MC6809_REG_HI (0)
# define MC6809_REG_LO (1)
#else
# define MC6809_REG_HI (1)
# define MC6809_REG_LO (0)
#endif

#define MC6809_REG_A(cpu) (*((uint8_t *)&cpu->reg_d + MC6809_REG_HI))
#define MC6809_REG_B(cpu) (*((uint8_t *)&cpu->reg_d + MC6809_REG_LO))

extern inline void MC6809_HALT_SET(struct MC6809 *cpu, _Bool val);
extern inline void MC6809_NMI_SET(struct MC6809 *cpu, _Bool val);
extern inline void MC6809_FIRQ_SET(struct MC6809 *cpu, _Bool val);
extern inline void MC6809_IRQ_SET(struct MC6809 *cpu, _Bool val);

struct MC6809 *mc6809_new(void);

#endif  /* XROAR_MC6809_H_ */
